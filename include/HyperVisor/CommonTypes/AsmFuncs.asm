.CODE

; Store LDTR:
ASMsldt PROC PUBLIC
    sldt WORD PTR [rcx]
    ret
ASMsldt ENDP

; Store TR:
ASMstr PROC PUBLIC
    str WORD PTR [rcx]
    ret
ASMstr ENDP

ASMinvd PROC PUBLIC
    invd
    ret
ASMinvd ENDP

; VMX-only:
ASMhyperv_vmcall PROC PUBLIC
    ; RCX - HYPERCALL_INPUT_VALUE
    ; RDX - Input parameters GPA when the Fast flag is 0, otherwise input parameter
    ; R8  - Output parameters GPA when the Fast flag is 0, otherwise output parameter
    ; XMM0..XMM5 can be used in hypervisors with XMM Fast input support
    vmcall

    ; RAX - HYPERCALL_RESULT_VALUE
    ret
ASMhyperv_vmcall ENDP

; VMX-only:
ASMkb_vmcall PROC PUBLIC
    ; RCX, RDX, R8, R9 - args
    ; RAX - result
    push r10
    mov r10, 01EE7C0DEh
    vmcall
    pop r10
    ret
ASMkb_vmcall ENDP

ASMinvept PROC PUBLIC
    ; RCX - INVEPT_TYPE
    ; RDX - INVEPT_DESCRIPTOR
    invept rcx, OWORD PTR [rdx]
    ret
ASMinvept ENDP

ASMinvvpid PROC PUBLIC
    ; RCX - INVVPID_TYPE
    ; RDX - INVVPID_DESCRIPTOR
    invvpid rcx, OWORD PTR [rdx]
    ret
ASMinvvpid ENDP

END