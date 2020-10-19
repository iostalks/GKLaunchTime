//
//  GKCallStackCore.c
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/9/15.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#include "GKBacktraceCore.h"

#ifdef __aarch64__

#pragma mark - fish hook

#include <stddef.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <sys/mman.h>

// API

/*
 * A structure representing a particular intended rebinding from a symbol
 * name to its replacement
 */
struct rebinding {
    const char *name;
    void *replacement;
    void **replaced;
};

/*
 * For each rebinding in rebindings, rebinds references to external, indirect
 * symbols with the specified name to instead point at replacement for each
 * image in the calling process as well as for all future images that are loaded
 * by the process. If rebind_functions is called more than once, the symbols to
 * rebind are added to the existing list of rebindings, and if a given symbol
 * is rebound more than once, the later rebinding will take precedence.
 */
static int fish_rebind_symbols(struct rebinding rebindings[], size_t rebindings_nel);

#ifdef __LP64__
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct section_64 section_t;
typedef struct nlist_64 nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT_64
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct section section_t;
typedef struct nlist nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT
#endif

#ifndef SEG_DATA_CONST
#define SEG_DATA_CONST  "__DATA_CONST"
#endif


// 重绑定符号 Node
struct rebindings_entry {
    struct rebinding *rebindings; // 待重绑定的符号链表
    size_t rebindings_nel; // 重绑定符号的数量
    struct rebindings_entry *next;
};

static struct rebindings_entry *_rebindings_head = NULL;

/**
 重绑定准备工作
 
 将绑定信息存储到 rebindings_entry 链表
 
 每调用一次 fishhook 创建一个 rebindings_entry 链表节点
 */
static int prepare_rebindings(struct rebindings_entry **rebindings_head, struct rebinding rebindings[], size_t nel) {
    struct rebindings_entry *new_entry = malloc(sizeof(struct rebindings_entry));
    if (!new_entry) return -1;
    new_entry->rebindings = malloc(sizeof(struct rebinding) * nel);
    if (!new_entry->rebindings) {
        free(new_entry);
        return -1;
    }
    memcpy(new_entry->rebindings, rebindings, sizeof(struct rebinding) * nel);
    new_entry->rebindings_nel = nel;
    new_entry->next = *rebindings_head;
    *rebindings_head = new_entry;
    return 0;
}

static vm_prot_t get_protection(void *sectionStart) {
  mach_port_t task = mach_task_self();
  vm_size_t size = 0;
  vm_address_t address = (vm_address_t)sectionStart;
  memory_object_name_t object;
#if __LP64__
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
  vm_region_basic_info_data_64_t info;
  kern_return_t info_ret = vm_region_64(
      task, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_64_t)&info, &count, &object);
#else
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT;
  vm_region_basic_info_data_t info;
  kern_return_t info_ret = vm_region(task, &address, &size, VM_REGION_BASIC_INFO, (vm_region_info_t)&info, &count, &object);
#endif
  if (info_ret == KERN_SUCCESS) {
    return info.protection;
  } else {
    return VM_PROT_READ;
  }
}

static void perform_rebinding_with_section(struct rebindings_entry *rebindings,
                                           section_t *section,
                                           intptr_t slide,
                                           nlist_t *symtab,
                                           char *strtab,
                                           uint32_t *indirect_symtab) {
    const bool isDataConst = strcmp(section->segname, SEG_DATA_CONST) == 0;
    uint32_t *indirect_symbol_indices = indirect_symtab + section->reserved1;
    void **indirect_symbol_bindings = (void **)((uintptr_t)slide + section->addr);
    vm_prot_t oldProtection = VM_PROT_READ;
    if (isDataConst) {
        // 修改内存保护权限
      oldProtection = get_protection(rebindings);
      mprotect(indirect_symbol_bindings, section->size, PROT_READ | PROT_WRITE);
    }
    for (uint i = 0; i < section->size / sizeof(void *); i++) {
        uint32_t symtab_index = indirect_symbol_indices[i];
        if (symtab_index == INDIRECT_SYMBOL_ABS || symtab_index == INDIRECT_SYMBOL_LOCAL
            || symtab_index == (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
            continue;
        }
        uint32_t strtab_offset = symtab[symtab_index].n_un.n_strx;
        char *symbol_name = strtab + strtab_offset;
        if (strnlen(symbol_name, 2) < 2) {
            continue;
        }
        struct rebindings_entry *cur = rebindings;
        while (cur) {
            for (uint j = 0; j < cur->rebindings_nel; j++) {
                if (strcmp(&symbol_name[1], cur->rebindings[j].name) == 0) {
                    if (cur->rebindings[j].replaced != NULL
                        && indirect_symbol_bindings[i] != cur->rebindings[j].replacement) {
                        *(cur->rebindings[j].replaced) = indirect_symbol_bindings[i]; // 将原始符号指针地址保存入我们函数符号
                    }
                    // 将我们hook实现的指针写入原始的符号
                    if (indirect_symbol_bindings[i]) {
                        indirect_symbol_bindings[i] = cur->rebindings[j].replacement;
                    }
                    goto symbol_loop;
                }
            }
            cur = cur->next;
        }
    symbol_loop:;
    }
}

static void rebind_symbols_for_image(struct rebindings_entry *rebindings, const struct mach_header *header, intptr_t slide) {
    Dl_info info;
    if (dladdr(header, &info) == 0) {
        printf("error: %s\n", info.dli_fname);
        return;
    }

    segment_command_t *cur_seg_cmd;
    segment_command_t *linkedit_segment = NULL;
    struct symtab_command* symtab_cmd = NULL;
    struct dysymtab_command* dysymtab_cmd = NULL;

    uintptr_t cur = (uintptr_t)header + sizeof(mach_header_t);
    for (uint i = 0; i < header->ncmds; i++, cur += cur_seg_cmd->cmdsize) {
      cur_seg_cmd = (segment_command_t *)cur;
      if (cur_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
        if (strcmp(cur_seg_cmd->segname, SEG_LINKEDIT) == 0) {
          linkedit_segment = cur_seg_cmd;
        }
      } else if (cur_seg_cmd->cmd == LC_SYMTAB) {
        symtab_cmd = (struct symtab_command*)cur_seg_cmd;
      } else if (cur_seg_cmd->cmd == LC_DYSYMTAB) {
        dysymtab_cmd = (struct dysymtab_command*)cur_seg_cmd;
      }
    }
    
    // 没有找到需绑定的符号
    if (!symtab_cmd || !dysymtab_cmd || !linkedit_segment || !dysymtab_cmd->nindirectsyms) {
        return;
    }
    
    printf("hook dylib: %s\n", info.dli_fname);
    
    // Fixedup base symbol/string table addresses
    // segment 中默认的虚拟地址是需要经过修正的
    uintptr_t linkedit_base = (uintptr_t)slide + linkedit_segment->vmaddr - linkedit_segment->fileoff;
    nlist_t *symtab = (nlist_t *)(linkedit_base + symtab_cmd->symoff);
    char *strtab = (char *)(linkedit_base + symtab_cmd->stroff);
    
    // Get indirect symbol table
    uint32_t *indirect_symtab = (uint32_t *)(linkedit_base + dysymtab_cmd->indirectsymoff);
    
    cur = (uintptr_t)header + sizeof(mach_header_t);
    for (uint i = 0; i < header->ncmds; i++, cur += cur_seg_cmd->cmdsize) {
        cur_seg_cmd = (segment_command_t *)cur;
        if (cur_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
            if (strcmp(cur_seg_cmd->segname, SEG_DATA) != 0
                && strcmp(cur_seg_cmd->segname, SEG_DATA_CONST) != 0) {
                continue;
            }
            for (uint j = 0; j < cur_seg_cmd->nsects; j++) {
                // section 紧随当前 segment 后面
                section_t *sect = (section_t *)(cur + sizeof(segment_command_t)) + j; // 指针+j
                if ((sect->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS) {
                  perform_rebinding_with_section(rebindings, sect, slide, symtab, strtab, indirect_symtab);
                }
                if ((sect->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS) {
                  perform_rebinding_with_section(rebindings, sect, slide, symtab, strtab, indirect_symtab);
                }
            }
        }
    }
    
}

// 动态库被 load 时调用，在注册回调前已经 load 的库，也会执行一次回调。
// slide 表示当前 image 到 dyld 的虚拟内存地址偏移
static void _rebind_symbols_for_image(const struct mach_header *header, intptr_t slide) {
    rebind_symbols_for_image(_rebindings_head, header, slide);
}

static int fish_rebind_symbols(struct rebinding rebindings[], size_t rebindings_nel) {
    int retval = prepare_rebindings(&_rebindings_head, rebindings, rebindings_nel);
    if (retval < 0) return retval;
    
    // 如果是第一次调用，先注册 callback
    // 之后动态 load 的动态库，也会被重绑定
    if (!_rebindings_head->next) {
        _dyld_register_func_for_add_image(_rebind_symbols_for_image);
    } else {
        uint32_t c = _dyld_image_count();
        for (uint32_t i = 0; i < c; i++) {
            _rebind_symbols_for_image(_dyld_get_image_header(i), _dyld_get_image_vmaddr_slide(i));
        }
    }
    return retval;
}


//#pragam mark - Call Stack

#include <stdlib.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include <objc/runtime.h>
#include <objc/message.h>
//#include <sys/stat.h>
#include <sys/time.h>

static bool _call_record_enable = false;
static uint64_t _min_time_cost = 100000; // 1ms
static uint32_t _max_call_depth = 5;

static pthread_key_t _thread_key;

__unused static id (*orig_objc_msgSend)(id, SEL, ...);

// 主线程调用栈记录，用于外部访问
static gk_call_record *_gkCallFrames;
static int _gkFrameNum = 0;
static int _gkFrameAlloc;

#ifndef MICRO_SECOND
#define MICRO_SECOND 1000000
#endif

#ifndef THREAD_FRAME_PAGE
#define THREAD_FRAME_PAGE 64
#endif

// objc 调用栈帧信息
typedef struct {
    id self; // 当前栈帧 self 指针
    Class cls; // self 所属的类
    SEL cmd; // 方法名
    uint64_t time; // 方法调用耗时
    uintptr_t lr; // 函数返回值地址
} thread_call_frame;

typedef struct {
    thread_call_frame *frame; // 栈帧数组
    int allocated_length; // 已分配的栈帧数量
    int depth; // 调用栈深度
    bool is_main_thread; // 是否是主线程
} thread_call_stack;

// 获取当前线程绑定的调用栈
static inline thread_call_stack* get_thread_call_stack() {
    thread_call_stack *cs = (thread_call_stack *)pthread_getspecific(_thread_key);
    if (!cs) {
        cs = (thread_call_stack *)malloc(sizeof(thread_call_stack));
        if (!cs) return NULL;
        thread_call_frame *cf = (thread_call_frame *)calloc(THREAD_FRAME_PAGE, sizeof(thread_call_frame));
        if (!cf) {
            free(cs);
            return NULL;
        }
        cs->frame = cf;
        cs->allocated_length = THREAD_FRAME_PAGE;
        cs->depth = -1;
        cs->is_main_thread = pthread_main_np();
        pthread_setspecific(_thread_key, cs);
    }
    return cs;
}

// 线程销毁时释放调用栈内存
void release_thread_call_stack(void *stackPtr) {
    thread_call_stack *cs = (thread_call_stack *)stackPtr;
    if (!cs) return;
    if (cs->frame) free(cs->frame);
    free(cs);
}

// 方法调用入栈
static inline void push_call_frame(id _self, Class _cls, SEL _cmd, uintptr_t lr) {
    thread_call_stack *cs = get_thread_call_stack();
    if (cs) {
        int nextIndex = (++cs->depth);
        if (nextIndex >= cs->allocated_length) {
            cs->allocated_length += THREAD_FRAME_PAGE;
            cs->frame = (thread_call_frame *)realloc(cs->frame, cs->allocated_length * sizeof(thread_call_frame));
        }
        thread_call_frame *newFrame = &cs->frame[nextIndex];
        newFrame->self = _self;
        newFrame->cls = _cls;
        newFrame->cmd = _cmd;
        newFrame->lr = lr;
        // 如果是主线程记录方法开始时间戳
        if (cs->is_main_thread && _call_record_enable) {
            struct timeval now;
            gettimeofday(&now, NULL);
            newFrame->time = now.tv_sec * MICRO_SECOND + now.tv_usec;
        }
    }
}

// 方法调用出栈
static inline uintptr_t pop_call_frame() {
    thread_call_stack *cs = get_thread_call_stack();
    int curIndex = cs->depth;
    int nextIndex = cs->depth--;
    thread_call_frame *pFrame = &cs->frame[nextIndex];
    // 只记录主线程耗时
    if (cs->is_main_thread && _call_record_enable) {
        struct timeval now;
        gettimeofday(&now, NULL);
        uint64_t time = now.tv_sec * MICRO_SECOND + now.tv_usec;
        uint64_t cost = time - pFrame->time;
        // 记录耗时超过一毫秒的调用帧
        if (cost > _min_time_cost && cs->depth < (int)_max_call_depth) {
            if (!_gkCallFrames) {
                _gkFrameAlloc = 1024;
                _gkCallFrames = malloc(sizeof(gk_call_record) * _gkFrameAlloc);
            }
            _gkFrameNum++;
            if (_gkFrameNum >= _gkFrameAlloc) {
                _gkFrameAlloc += 1024;
                _gkCallFrames = realloc(_gkCallFrames, sizeof(_gkCallFrames) * _gkFrameAlloc);
            }
            gk_call_record *log = &_gkCallFrames[_gkFrameNum - 1];
            log->cls = pFrame->cls;
            log->sel = pFrame->cmd;
            log->depth = curIndex;
            log->time = cost;
        }
    }
    return pFrame->lr;
}

void before_objc_msgSend(id self, SEL _cmd, uintptr_t lr) {
//    printf("before obj send %p, %s \n", self, _cmd);
    push_call_frame(self, object_getClass(self), _cmd, lr);
}

uintptr_t after_objc_msgSend() {
    return pop_call_frame();
}

//replacement objc_msgSend (arm64)
// https://blog.nelhage.com/2010/10/amd64-and-va_arg/
// http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
// https://developer.apple.com/library/ios/documentation/Xcode/Conceptual/iPhoneOSABIReference/Articles/ARM64FunctionCallingConventions.html

#define call(b, value) \
__asm volatile ("stp x8, x9, [sp, #-16]!\n"); \
__asm volatile ("mov x12, %0\n" :: "r"(value)); \
__asm volatile ("ldp x8, x9, [sp], #16\n"); \
__asm volatile (#b " x12\n");


// 存储 x0~x9 寄存器的值到栈，为什么是从大到小的存储？
// x0~x7 是参数寄存器 x0 代表第一个参数，以此类推
// x8 是 syscall 的 number，如何理解？
/**
 x0~x9 参数寄存器
 */
#define save() \
__asm volatile ( \
"stp x8, x9, [sp, #-16]!\n" \
"stp x6, x7, [sp, #-16]!\n" \
"stp x4, x5, [sp, #-16]!\n" \
"stp x2, x3, [sp, #-16]!\n" \
"stp x0, x1, [sp, #-16]!\n");

// 出栈值到寄存器 x0~x9
#define load() \
__asm volatile ( \
"ldp x0, x1, [sp], #16\n" \
"ldp x2, x3, [sp], #16\n" \
"ldp x4, x5, [sp], #16\n" \
"ldp x6, x7, [sp], #16\n" \
"ldp x8, x9, [sp], #16\n" );

#define link(b, value) \
__asm volatile ("stp x8, lr, [sp, #-16]!\n"); \
__asm volatile ("sub sp, sp, #16\n"); \
call(b, value); \
__asm volatile ("add sp, sp, #16\n"); \
__asm volatile ("ldp x8, lr, [sp], #16\n");

#define ret() __asm volatile ("ret\n");

__attribute__((__naked__)) // 汇编实现标识
static void hook_objc_msgSend() {
    // 保存寄存器值
    save();
    
    // lr 返回值寄存器
    // 保存返回值
    __asm volatile ("mov x2, lr\n");
    __asm volatile ("mov x3, x4\n");
    
    // Call our before_objc_msgSend.
    call(blr, &before_objc_msgSend)
    
    // Load parameters.
    load()
    
    // Call through to the original objc_msgSend.
    call(blr, orig_objc_msgSend)
    
    // Save original objc_msgSend return value.
    save()
    
    // Call our after_objc_msgSend.
    call(blr, &after_objc_msgSend)
    
    // restore lr
    __asm volatile ("mov lr, x0\n");
    
    // Load original objc_msgSend return value.
    load()
    
    // return
    ret()
}

#pragma mark - public

void gk_call_trace_start(void) {
    _call_record_enable = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        pthread_key_create(&_thread_key, &release_thread_call_stack);
        fish_rebind_symbols((struct rebinding[6]){
            {"objc_msgSend", (void *)hook_objc_msgSend, (void **)&orig_objc_msgSend},
        }, 1);
    });
}

void gk_call_trace_stop() {
    _call_record_enable = false;
}

void gk_config_min_time(uint64_t time) {
    _min_time_cost = time;
}

void gk_config_max_depth(int depth) {
    _max_call_depth = depth;
}

// 可在任意线程读取堆栈记录
gk_call_record *gk_get_call_records(int *count) {
    if (count) {
        *count = _gkFrameNum;
    }
    return _gkCallFrames;
}

void gk_clean_call_records() {
    if (_gkCallFrames) {
        free(_gkCallFrames);
        _gkCallFrames = NULL;
    }
    _gkFrameNum = 0;
}


#else


void gk_call_trace_start(void) {}
void gk_call_trace_stop() {}
void gk_config_min_time(uint64_t time) {
}
void gk_config_max_depth(int depth) {
}
gk_call_record *gk_get_call_records(int *depth) {
    if (depth) {
        *depth = 0;
    }
    return NULL;
}
void gk_clean_call_records() {}
#endif

// Test
__attribute__((constructor))
static void initHook() {
    gk_call_trace_start();
}
