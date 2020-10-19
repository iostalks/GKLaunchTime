//
//  GKRunLoopObserver.m
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/10/12.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#import "GKRunLoopObserver.h"


@interface GKRunLoopObserverListenerInfo : NSObject
@property (nonatomic, weak) id listener;
@property (nonatomic, assign) SEL action;
@end

@implementation GKRunLoopObserverListenerInfo
@end


@interface GKRunLoopObserver ()
@property (nonatomic, strong) NSMutableDictionary *dictionary;
@property (nonatomic, strong) NSRunLoop *runLoop; // 线程不安全
@end

@implementation GKRunLoopObserver

- (instancetype)initWithRunLoop:(NSRunLoop *)runLoop {
    self = [super init];
    if (!self) return nil;
    _runLoop = runLoop;
    _dictionary = [NSMutableDictionary new];
    return self;
}

- (void)addListener:(id)listener action:(SEL)action forRunLoopModel:(NSRunLoopMode)model {
    if (!_dictionary[model]) {
        _dictionary[model] = [NSMutableSet set];;
    }
    
    GKRunLoopObserverListenerInfo *info = [GKRunLoopObserverListenerInfo new];
    info.listener = listener;
    info.action = action;
    
    [_dictionary[model] addObject:info];
}

- (void)start {
//    CFRunLoopObserverRef defaultObserver = CFRunLoopObserverCreateWithHandler(kCFAllocatorDefault, kCFRunLoopAllActivities, YES, 0, ^(CFRunLoopObserverRef observer, CFRunLoopActivity activity) {
//        NSLog(@"NSDefaultRunLoopMode Activity: %lu", activity);
//        for (GKRunLoopObserverListenerInfo *info in self.dictionary[NSDefaultRunLoopMode]) {
//            if ([info.listener respondsToSelector:info.action]) {
//                [info.listener performSelector:info.action withObject:@(activity)];
//            }
//        }
//    });
    
//    CFRunLoopObserverRef commonObserver = CFRunLoopObserverCreateWithHandler(kCFAllocatorDefault, kCFRunLoopAllActivities, YES, 0, ^(CFRunLoopObserverRef observer, CFRunLoopActivity activity) {
//        CFRunLoopMode model = CFRunLoopCopyCurrentMode(CFRunLoopGetCurrent());
//        NSLog(@"kCFRunLoopCommonModes Activity: %f, %@", log2(activity), model);
//        for (GKRunLoopObserverListenerInfo *info in self.dictionary[NSRunLoopCommonModes]) {
//            if ([info.listener respondsToSelector:info.action]) {
//                [info.listener performSelector:info.action withObject:@(activity)];
//            }
//        }
//    });
   
//    CFRunLoopAddObserver([_runLoop getCFRunLoop], defaultObserver, kCFRunLoopDefaultMode);
//    CFRunLoopAddObserver([_runLoop getCFRunLoop], commonObserver, kCFRunLoopCommonModes);
}
@end
