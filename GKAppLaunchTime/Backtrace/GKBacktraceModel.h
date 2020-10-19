//
//  GKBacktraceModel.h
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/9/24.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface GKBacktraceModel : NSObject

@property (nonatomic, copy) NSString *className;
@property (nonatomic, copy) NSString *methodName;
@property (nonatomic, assign) BOOL isClassMethod;
@property (nonatomic, assign) NSTimeInterval cost; // s
@property (nonatomic, assign) NSUInteger depth;
@property (nonatomic, copy) NSString *path;
@property (nonatomic, assign) BOOL isLastCall;
@property (nonatomic, assign) NSUInteger frequency;
@property (nonatomic, strong) NSArray<GKBacktraceModel *> *subs;

@end

NS_ASSUME_NONNULL_END
