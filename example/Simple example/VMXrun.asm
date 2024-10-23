.CODE

EXTERN VmxVmExitHandler: PROC

GPR_CONTEXT_ENTRIES equ 15 ; rax, rbx, rcx, rdx, rsi, rdi, rbp, r8..r15
GPR_CONTEXT_SIZE    equ GPR_CONTEXT_ENTRIES * sizeof(QWORD)
XMM_YMM_CONTEXT_ENTRIES equ 16 ; xmm0..xmm15 ymm0..ymm15
XMM_CONTEXT_SIZE    equ XMM_YMM_CONTEXT_ENTRIES * sizeof(OWORD)
YMM_CONTEXT_SIZE    equ XMM_YMM_CONTEXT_ENTRIES * sizeof(YMMWORD)

SSE_SUPPORT equ 0
AVX_SUPPORT equ 0

CPUID_VMM_SHUTDOWN equ 01EE7C0DEh

; Without RSP saving:
PUSHAQ MACRO
    sub rsp, GPR_CONTEXT_SIZE
    mov [rsp + 0  * sizeof(QWORD)], rax
    mov [rsp + 1  * sizeof(QWORD)], rbx
    mov [rsp + 2  * sizeof(QWORD)], rcx
    mov [rsp + 3  * sizeof(QWORD)], rdx
    mov [rsp + 4  * sizeof(QWORD)], rsi
    mov [rsp + 5  * sizeof(QWORD)], rdi
    mov [rsp + 6  * sizeof(QWORD)], rbp
    mov [rsp + 7  * sizeof(QWORD)], r8
    mov [rsp + 8  * sizeof(QWORD)], r9
    mov [rsp + 9  * sizeof(QWORD)], r10
    mov [rsp + 10 * sizeof(QWORD)], r11
    mov [rsp + 11 * sizeof(QWORD)], r12
    mov [rsp + 12 * sizeof(QWORD)], r13
    mov [rsp + 13 * sizeof(QWORD)], r14
    mov [rsp + 14 * sizeof(QWORD)], r15
ENDM

; Without RSP restoring:
POPAQ MACRO
    mov rax, [rsp + 0  * sizeof(QWORD)]
    mov rbx, [rsp + 1  * sizeof(QWORD)]
    mov rcx, [rsp + 2  * sizeof(QWORD)]
    mov rdx, [rsp + 3  * sizeof(QWORD)]
    mov rsi, [rsp + 4  * sizeof(QWORD)]
    mov rdi, [rsp + 5  * sizeof(QWORD)]
    mov rbp, [rsp + 6  * sizeof(QWORD)]
    mov r8 , [rsp + 7  * sizeof(QWORD)]
    mov r9 , [rsp + 8  * sizeof(QWORD)]
    mov r10, [rsp + 9  * sizeof(QWORD)]
    mov r11, [rsp + 10 * sizeof(QWORD)]
    mov r12, [rsp + 11 * sizeof(QWORD)]
    mov r13, [rsp + 12 * sizeof(QWORD)]
    mov r14, [rsp + 13 * sizeof(QWORD)]
    mov r15, [rsp + 14 * sizeof(QWORD)]
    add rsp, GPR_CONTEXT_SIZE
ENDM

PUSHAXMM MACRO
    sub rsp, XMM_CONTEXT_SIZE
    movaps [rsp + 0 * sizeof(OWORD)], xmm0
    movaps [rsp + 1 * sizeof(OWORD)], xmm1
    movaps [rsp + 2 * sizeof(OWORD)], xmm2
    movaps [rsp + 3 * sizeof(OWORD)], xmm3
    movaps [rsp + 4 * sizeof(OWORD)], xmm4
    movaps [rsp + 5 * sizeof(OWORD)], xmm5
    movaps [rsp + 6 * sizeof(OWORD)], xmm6
    movaps [rsp + 7 * sizeof(OWORD)], xmm7
    movaps [rsp + 8 * sizeof(OWORD)], xmm8
    movaps [rsp + 9 * sizeof(OWORD)], xmm9
    movaps [rsp + 10 * sizeof(OWORD)], xmm10
    movaps [rsp + 11 * sizeof(OWORD)], xmm11
    movaps [rsp + 12 * sizeof(OWORD)], xmm12
    movaps [rsp + 13 * sizeof(OWORD)], xmm13
    movaps [rsp + 14 * sizeof(OWORD)], xmm14
    movaps [rsp + 15 * sizeof(OWORD)], xmm15
ENDM

POPAXMM MACRO
    movaps xmm0, [rsp + 0 * sizeof(OWORD)]
    movaps xmm1, [rsp + 1 * sizeof(OWORD)]
    movaps xmm2, [rsp + 2 * sizeof(OWORD)]
    movaps xmm3, [rsp + 3 * sizeof(OWORD)]
    movaps xmm4, [rsp + 4 * sizeof(OWORD)]
    movaps xmm5, [rsp + 5 * sizeof(OWORD)]
    movaps xmm6, [rsp + 0 * sizeof(OWORD)]
    movaps xmm7, [rsp + 1 * sizeof(OWORD)]
    movaps xmm8, [rsp + 2 * sizeof(OWORD)]
    movaps xmm9, [rsp + 3 * sizeof(OWORD)]
    movaps xmm10, [rsp + 4 * sizeof(OWORD)]
    movaps xmm11, [rsp + 5 * sizeof(OWORD)]
    movaps xmm12, [rsp + 0 * sizeof(OWORD)]
    movaps xmm13, [rsp + 1 * sizeof(OWORD)]
    movaps xmm14, [rsp + 2 * sizeof(OWORD)]
    movaps xmm15, [rsp + 3 * sizeof(OWORD)]
    add rsp, XMM_CONTEXT_SIZE
ENDM

PUSHAYMM MACRO
    sub rsp, YMM_CONTEXT_SIZE
    vmovdqu ymmword ptr [rsp + 0 * sizeof(YMMWORD)], ymm0
    vmovdqu ymmword ptr [rsp + 1 * sizeof(YMMWORD)], ymm1
    vmovdqu ymmword ptr [rsp + 2 * sizeof(YMMWORD)], ymm2
    vmovdqu ymmword ptr [rsp + 3 * sizeof(YMMWORD)], ymm3
    vmovdqu ymmword ptr [rsp + 4 * sizeof(YMMWORD)], ymm4
    vmovdqu ymmword ptr [rsp + 5 * sizeof(YMMWORD)], ymm5
    vmovdqu ymmword ptr [rsp + 6 * sizeof(YMMWORD)], ymm6
    vmovdqu ymmword ptr [rsp + 7 * sizeof(YMMWORD)], ymm7
    vmovdqu ymmword ptr [rsp + 8 * sizeof(YMMWORD)], ymm8
    vmovdqu ymmword ptr [rsp + 9 * sizeof(YMMWORD)], ymm9
    vmovdqu ymmword ptr [rsp + 10 * sizeof(YMMWORD)], ymm10
    vmovdqu ymmword ptr [rsp + 11 * sizeof(YMMWORD)], ymm11
    vmovdqu ymmword ptr [rsp + 12 * sizeof(YMMWORD)], ymm12
    vmovdqu ymmword ptr [rsp + 13 * sizeof(YMMWORD)], ymm13
    vmovdqu ymmword ptr [rsp + 14 * sizeof(YMMWORD)], ymm14
    vmovdqu ymmword ptr [rsp + 15 * sizeof(YMMWORD)], ymm15
ENDM

POPAYMM MACRO
    vmovdqu ymm0, ymmword ptr [rsp + 0 * sizeof(YMMWORD)]
    vmovdqu ymm1, ymmword ptr [rsp + 1 * sizeof(YMMWORD)]
    vmovdqu ymm2, ymmword ptr [rsp + 2 * sizeof(YMMWORD)]
    vmovdqu ymm3, ymmword ptr [rsp + 3 * sizeof(YMMWORD)]
    vmovdqu ymm4, ymmword ptr [rsp + 4 * sizeof(YMMWORD)]
    vmovdqu ymm5, ymmword ptr [rsp + 5 * sizeof(YMMWORD)]
    vmovdqu ymm6, ymmword ptr [rsp + 0 * sizeof(YMMWORD)]
    vmovdqu ymm7, ymmword ptr [rsp + 1 * sizeof(YMMWORD)]
    vmovdqu ymm8, ymmword ptr [rsp + 2 * sizeof(YMMWORD)]
    vmovdqu ymm9, ymmword ptr [rsp + 3 * sizeof(YMMWORD)]
    vmovdqu ymm10, ymmword ptr [rsp + 4 * sizeof(YMMWORD)]
    vmovdqu ymm11, ymmword ptr [rsp + 5 * sizeof(YMMWORD)]
    vmovdqu ymm12, ymmword ptr [rsp + 0 * sizeof(YMMWORD)]
    vmovdqu ymm13, ymmword ptr [rsp + 1 * sizeof(YMMWORD)]
    vmovdqu ymm14, ymmword ptr [rsp + 2 * sizeof(YMMWORD)]
    vmovdqu ymm15, ymmword ptr [rsp + 3 * sizeof(YMMWORD)]
    add rsp, YMM_CONTEXT_SIZE
ENDM

MULTIPUSH MACRO
    PUSHAQ
    mov rcx, [rsp + GPR_CONTEXT_SIZE + 16] ; RCX -> PRIVATE_VM_DATA* Private
    mov rdx, rsp ; RDX -> Guest context
ENDM

PROLOGUE MACRO
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rsp + 0 * sizeof(QWORD)], rcx
    mov [rsp + 1 * sizeof(QWORD)], rdx
    mov [rsp + 2 * sizeof(QWORD)], r8
    mov [rsp + 3 * sizeof(QWORD)], r9
ENDM

EPILOGUE MACRO
    mov rcx, [rsp + 0 * sizeof(QWORD)]
    mov rdx, [rsp + 1 * sizeof(QWORD)]
    mov r8 , [rsp + 2 * sizeof(QWORD)]
    mov r9 , [rsp + 3 * sizeof(QWORD)]
    add rsp, 32
    pop rbp
    ret
ENDM

; void VmxVmmRun(_In_ void* InitialVmmStackPointer);
VmxVmmRun PROC PUBLIC
    PUSHAQ
    mov rcx, [rsp + GPR_CONTEXT_SIZE + 16]
    mov rdx, rsp

    ;PUSHAXMM
    sub rsp, 32 ; Homing space for the x64 call convention
    call VmxVmExitHandler ; VMM_STATUS VmxVmexitHandler(PRIVATE_VM_DATA* Private, GUEST_CONTEXT* Context)
    add rsp, 32
    ;POPAXMM

    test al, al ; if (!VmxVmexitHandler(...)) break;
    jz VmmExit

    POPAQ
    vmresume

VmmExit:
    POPAQ

    ; Exiting the virtual state:
    ; This context is setted up in the SvmVmexitHandler:
    ;  RBX -> Guest's RIP
    ;  RCX -> Guest's RSP
    ;  EDX:EAX -> Address of the PRIVATE_VM_DATA to free

    mov rsp, rcx
    mov ecx, CPUID_VMM_SHUTDOWN ; Signature that says about the VM shutdown
    jmp rbx
VmxVmmRun ENDP

END
