//
//  GKCallStackCore.h
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/9/15.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#ifndef GKBacktraceCore_h
#define GKBacktraceCore_h

#include <stdio.h>
#include <objc/objc.h>

// 满足耗时条件的栈帧记录
typedef struct {
    __unsafe_unretained Class cls; // 类名
    SEL sel; // 方法名
    uint64_t time; // 方法调用耗时 us
    uint32_t depth; // 当前方法调用栈深度
} gk_call_record;

void gk_call_trace_start(void);
void gk_call_trace_stop(void);

void gk_config_min_time(uint64_t time);
void gk_config_max_depth(int depth);

gk_call_record *gk_get_call_records(int *count);
void gk_clean_call_records(void);


#endif /* GKBacktraceCore_h */
