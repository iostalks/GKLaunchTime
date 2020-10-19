//
//  ViewController.m
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/9/7.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#import "ViewController.h"
#import "GKStackCapturer.h"
#import "GKBacktrace.h"
#import <mach-o/dyld.h>
#import <pthread/pthread.h>
//#import "fishhook.h"
#import <objc/message.h>

#import "GKRunLoopObserver.h"

@interface ViewController ()
@property (nonatomic, assign) NSUInteger idx;
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
}

- (IBAction)onClick:(id)sender {
    [self test];
}

- (IBAction)capture:(id)sender {
    [GKBacktrace saveAndClean];
}

- (void)test {
    if (_idx == 5) {
        _idx = 0;
        return;
    }
    sleep(2);
    
    _idx++;
    [self test];
}

@end
