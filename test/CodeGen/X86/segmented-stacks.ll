; RUN: llc < %s -march=x86 -segmented-stacks | FileCheck %s -check-prefix=X32
; RUN: llc < %s -march=x86_64 -segmented-stacks | FileCheck %s -check-prefix=X64

; Just to prevent the alloca from being optimized away
define void @dummy(i32* %val, i32 %len) {
        ret void
}
        
define i32 @test(i32 %l) {
        %mem = alloca i32, i32 %l
        call void @dummy (i32* %mem, i32 %l)
        %terminate = icmp eq i32 %l, 0
        br i1 %terminate, label %true, label %false

true:
        ret i32 0

false:
        %newlen = sub i32 %l, 1
        %retvalue = call i32 @test(i32 %newlen)
        ret i32 %retvalue

; X32: test:
; X32: leal	-12(%esp), %eax
; X32: cmpl	%gs:48, %eax
; X32: addl	$8, %esp
; X32: pushl	$4
; X32: pushl	$12
; X32: callq	__morestack
; X32: subl	$16, %esp
; X32: ret

; X32: movl	%esp, %ecx
; X32: subl	%eax, %ecx
; X32: cmpl	%ecx, %gs:48
; X32: subl	%eax, %esp
; X32: movl	%esp, %eax
; X32: addl	$12, %esp
; X32: pushl	%eax
; X32: calll	__morestack_allocate_stack_space
; X32: subl	$16, %esp

; X64: test:
; X64: leaq	-24(%rsp), %r10
; X64: cmpq	%fs:112, %r10
; X64: movabsq	$24, %r10
; X64: movabsq	$0, %r11
; X64: callq	__morestack
; X64: ret

; X64: movq	%rsp, %rax
; X64: subq	%rcx, %rax
; X64: cmpq	%rax, %fs:112
; X64: subq	%rcx, %rsp
; X64: movq	%rsp, %rdi
; X64: movq	%rcx, %rdi
; X64: callq	__morestack_allocate_stack_space
; X64: movq	%rax, %rdi

}
