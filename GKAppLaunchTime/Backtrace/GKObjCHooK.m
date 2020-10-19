//
//  GKObjCHooK.m
//  GKAppLaunchTime
//
//  Created by Smallfly on 2020/9/14.
//  Copyright © 2020 Smallfly. All rights reserved.
//

#import "GKObjCHooK.h"

__unused
static void replacementObjc_msgSend() {
  __asm__ volatile (
    // sp 是堆栈寄存器，存放栈的偏移地址，每次都指向栈顶。
    // 保存 {q0-q7} 偏移地址到 sp 寄存器
      "stp q6, q7, [sp, #-32]!\n"
      "stp q4, q5, [sp, #-32]!\n"
      "stp q2, q3, [sp, #-32]!\n"
      "stp q0, q1, [sp, #-32]!\n"
    // 保存 {x0-x8, lr}
      "stp x8, lr, [sp, #-16]!\n"
      "stp x6, x7, [sp, #-16]!\n"
      "stp x4, x5, [sp, #-16]!\n"
      "stp x2, x3, [sp, #-16]!\n"
      "stp x0, x1, [sp, #-16]!\n"
    // 交换参数.
      "mov x2, x1\n"
      "mov x1, lr\n"
      "mov x3, sp\n"
    // 调用 preObjc_msgSend，使用 bl label 语法。bl 执行一个分支链接操作，label 是无条件分支的，是和本指令的地址偏移，范围是 -128MB 到 +128MB
      "bl __Z15preObjc_msgSendP11objc_objectmP13objc_selectorP9RegState_\n"
      "mov x9, x0\n"
      "mov x10, x1\n"
      "tst x10, x10\n"
    // 读取 {x0-x8, lr} 从保存到 sp 栈顶的偏移地址读起
      "ldp x0, x1, [sp], #16\n"
      "ldp x2, x3, [sp], #16\n"
      "ldp x4, x5, [sp], #16\n"
      "ldp x6, x7, [sp], #16\n"
      "ldp x8, lr, [sp], #16\n"
    // 读取 {q0-q7}
      "ldp q0, q1, [sp], #32\n"
      "ldp q2, q3, [sp], #32\n"
      "ldp q4, q5, [sp], #32\n"
      "ldp q6, q7, [sp], #32\n"
      "b.eq Lpassthrough\n"
    // 调用原始 objc_msgSend。使用 blr xn 语法。blr 除了从指定寄存器读取新的 PC 值外效果和 bl 一样。xn 是通用寄存器的64位名称分支地址，范围是0到31
      "blr x9\n"
    // 保存 {x0-x9}
      "stp x0, x1, [sp, #-16]!\n"
      "stp x2, x3, [sp, #-16]!\n"
      "stp x4, x5, [sp, #-16]!\n"
      "stp x6, x7, [sp, #-16]!\n"
      "stp x8, x9, [sp, #-16]!\n"
    // 保存 {q0-q7}
      "stp q0, q1, [sp, #-32]!\n"
      "stp q2, q3, [sp, #-32]!\n"
      "stp q4, q5, [sp, #-32]!\n"
      "stp q6, q7, [sp, #-32]!\n"
    // 调用 postObjc_msgSend hook.
      "bl __Z16postObjc_msgSendv\n"
      "mov lr, x0\n"
    // 读取 {q0-q7}
      "ldp q6, q7, [sp], #32\n"
      "ldp q4, q5, [sp], #32\n"
      "ldp q2, q3, [sp], #32\n"
      "ldp q0, q1, [sp], #32\n"
    // 读取 {x0-x9}
      "ldp x8, x9, [sp], #16\n"
      "ldp x6, x7, [sp], #16\n"
      "ldp x4, x5, [sp], #16\n"
      "ldp x2, x3, [sp], #16\n"
      "ldp x0, x1, [sp], #16\n"
      "ret\n"
      "Lpassthrough:\n"
    // br 无条件分支到寄存器中的地址
      "br x9"
    );
}

@implementation GKObjCHooK

@end
