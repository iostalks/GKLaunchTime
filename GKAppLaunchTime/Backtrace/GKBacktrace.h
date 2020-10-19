//
//  GKCallStack.h
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/9/14.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#import <Foundation/Foundation.h>

#include "GKBacktraceCore.h"

NS_ASSUME_NONNULL_BEGIN

@interface GKBacktrace : NSObject

+ (void)start;
+ (void)startWithMaxDepth:(int)depth;

+ (void)stop;

+ (void)save;
+ (void)saveAndClean;

@end

NS_ASSUME_NONNULL_END
