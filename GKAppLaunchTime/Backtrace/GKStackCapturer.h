//
//  GKStackCapturer.h
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/9/7.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface GKStackCapturer : NSObject

+ (instancetype)sharedInstance;

- (void)start;

- (void)stop;

@end

NS_ASSUME_NONNULL_END
