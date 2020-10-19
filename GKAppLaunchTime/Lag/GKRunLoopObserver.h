//
//  GKRunLoopObserver.h
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/10/12.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface GKRunLoopObserver : NSObject

- (instancetype)initWithRunLoop:(NSRunLoop *)runLoop;

- (void)addListener:(id)listener action:(SEL)action forRunLoopModel:(NSRunLoopMode)model;

- (void)start;
@end

NS_ASSUME_NONNULL_END
