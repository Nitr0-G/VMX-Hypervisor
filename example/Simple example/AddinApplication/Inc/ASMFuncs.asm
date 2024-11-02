.CODE
; VMX-only:
ASMmv_vmcall PROC PUBLIC
    ; RCX, RDX, R8, R9 - args
    ; RAX - result
    push r10
    mov r10, 01EE7C0DEh
    vmcall
    pop r10
    ret
ASMmv_vmcall ENDP
END