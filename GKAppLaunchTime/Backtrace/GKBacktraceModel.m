//
//  GKBacktraceModel.m
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/9/24.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#import "GKBacktraceModel.h"

@implementation GKBacktraceModel

- (NSString *)description {
    NSMutableString *str = [NSMutableString new];
    [str appendFormat:@"%2d | ", (int)_depth];
    [str appendFormat:@"%6.2f | ", _cost * 1000.0];
    for (NSUInteger i = 0; i < _depth; i++) {
        [str appendString:@"  "];
    }
    [str appendFormat:@"%s[%@ %@]", (_isClassMethod ? "+" : "-"), _className, _methodName];
    return str;
}

- (NSString *)debugDescription {
    return [self description];
}

@end
