//
//  GKStackCapturer.m
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/9/7.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#import "GKStackCapturer.h"
#import "mach_backtrace.h"
#import <pthread.h>
#import <dlfcn.h>

#define THREAD_NAME_SIZE 256

static NSString* kMainThreadName = @"main.thread";

//static const double kCapturperInterval = 0.01;

thread_t get_mach_thread(NSThread *thread) {
    thread_act_array_t threads;
    mach_msg_type_number_t count = 0;
    
    if (0 != task_threads(mach_task_self_, &threads, &count)) {
        return mach_thread_self();
    }
    
    char name[THREAD_NAME_SIZE];
    for (int i = 0; i < count; ++i) {
        pthread_t pthread = pthread_from_mach_thread_np(threads[i]);
        if (pthread) {
            name[0] = '\0';
            pthread_getname_np(pthread, name, THREAD_NAME_SIZE);
            if (0 == strcmp(name, [thread.name UTF8String])) {
                return threads[i];
            }
        }
    }
    
    return mach_thread_self();
}

@implementation GKStackCapturer {
    dispatch_source_t _timer;
}

+ (instancetype)sharedInstance {
    static GKStackCapturer *capturer;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        [NSThread mainThread].name = kMainThreadName;
        capturer = [[self alloc] init];
    });
    return capturer;
}

- (void)start {
    dispatch_resume(_timer);
}

- (void)stop {
    dispatch_suspend(_timer);
}

+ (thread_t)machThread:(NSThread *)thread {
    thread_act_array_t threads;
    mach_msg_type_number_t count = 0;
    
    if (0 != task_threads(mach_task_self_, &threads, &count)) {
        return mach_thread_self();
    }
    
    char name[THREAD_NAME_SIZE];
    for (int i = 0; i < count; ++i) {
        pthread_t pthread = pthread_from_mach_thread_np(threads[i]);
        if (pthread) {
            name[0] = '\0';
            pthread_getname_np(pthread, name, THREAD_NAME_SIZE);
            if (0 == strcmp(name, [thread.name UTF8String])) {
                return threads[i];
            }
        }
    }
    
    return mach_thread_self();
}

@end
