//
//  GKCallStack.m
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/9/14.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#import "GKBacktrace.h"
#import "GKBacktraceModel.h"
#import <objc/objc.h>
#import <objc/message.h>

@implementation GKBacktrace

+ (void)start {
    gk_call_trace_start();
}

+ (void)startWithMaxDepth:(int)depth {
    gk_config_max_depth(depth);
    [self start];
}

+ (void)startWithMinCost:(double)ms {
    gk_config_min_time(ms * 1000);
    [self start];
}

+ (void)startWithMinDepth:(uint32_t)depth minCost:(double)ms {
    gk_config_max_depth(depth);
    gk_config_min_time(ms);
    [self start];
}

+ (void)stop {
    gk_call_trace_stop();
}

+ (void)save {
    NSArray<GKBacktraceModel *> *arr = [self loadRecords];
    for (GKBacktraceModel *model in arr) {
        model.path = [NSString stringWithFormat:@"[%@ %@]", model.className, model.methodName];
        NSLog(@"%@", model);
    }
}

+ (void)saveAndClean {
    [self save];
//    gk_clean_call_records();
}

+ (void)appendRecord:(GKBacktraceModel *)model text:(NSMutableString *)str {
    if (model.subs.count == 0) {
        model.isLastCall = YES;
    } else {
        for (GKBacktraceModel *m in model.subs) {
            if ([m.className isEqualToString:@"GKBacktrace"]) {
                continue;
            }
            model.path = [NSString stringWithFormat:@"%@ - [%@ %@]", model.path, model.className, model.methodName];
            [self appendRecord:model text:str];
        }
    }
}

+ (NSArray<GKBacktraceModel *> *)loadRecords {
    NSMutableArray<GKBacktraceModel *> *arr = [NSMutableArray new];
    int num = 0;
    gk_call_record *records = gk_get_call_records(&num);
    for (uint32_t i = 0; i < num; i++) {
        gk_call_record *rd = &records[i];
        GKBacktraceModel *model = [GKBacktraceModel new];
        model.className = NSStringFromClass(rd->cls);
        model.methodName = NSStringFromSelector(rd->sel);
        model.isClassMethod = class_isMetaClass(rd->cls);
        model.cost = (double)rd->time / 1000000.0;
        model.depth = rd->depth;
        [arr addObject:model];
    }
    
    // 整理同一层级的调用记录
//    NSUInteger count = arr.count;
//    for (NSUInteger i = 0; i < count; i++) {
//        NSLog(@"%@" , arr[i]);
//        GKBacktraceModel *model = arr[i];
//        if (model.depth > 0) {
//            [arr removeObjectAtIndex:i];
//            for (NSUInteger j = i; j < count; j++) {
//                if (arr[j].depth + 1 == model.depth) {
//                    NSMutableArray *sub = [NSMutableArray arrayWithArray:arr[j].subs];
//                    if (!sub) {
//                        sub = [NSMutableArray new];
//                        arr[j].subs = sub;
//                    }
//                    [sub insertObject:model atIndex:0];
//                }
//            }
//            i--;
//            count--;
//        }
//    }
    return [arr copy];
}

@end

// 存储 thread 信息的结构体
typedef struct GKThreadInfo {
    double cpuUsage;
    integer_t userTime;
} GKThreadInfo;

//NSString *stackOfThread(thread_t thread) {
//    GKThreadInfo gkThreadInfo = {0};
//    thread_info_data_t threadInfo;
//    // 线程基本信息
//    thread_basic_info_t threadBasicInfo;
//
//    mach_msg_type_number_t threadInfoCount = THREAD_INFO_MAX;
//
//    if (thread_info((thread_act_t)thread, THREAD_BASIC_INFO, (thread_info_t)threadInfo, &threadInfoCount) == KERN_SUCCESS) {
//        threadBasicInfo = (thread_basic_info_t)threadInfo;
//        if (!(threadBasicInfo->flags & TH_FLAGS_IDLE)) {
//            gkThreadInfo.cpuUsage = threadBasicInfo->cpu_usage / 10;
//            gkThreadInfo.userTime = threadBasicInfo->system_time.microseconds;
//        }
//    }
//
//    uintptr_t buffer[100];
//     int i = 0;
//     NSMutableString *reStr = [NSMutableString stringWithFormat:@"Stack of thread: %u:\nCPU used: %.1f percent\nuser time: %d second\n", thread, gkThreadInfo.cpuUsage, gkThreadInfo.userTime];
//
//
//    return @"";
//}
