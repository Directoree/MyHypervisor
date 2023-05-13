EXTERN VehVmExitHandler:PROC
EXTERN g_GuestRegisters:qword
EXTERN g_ResumeRIP:qword
EXTERN VehLowerIrql:PROC

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; constants
;
.CONST

VMX_OK                      EQU     0
VMX_ERROR_WITH_STATUS       EQU     1
VMX_ERROR_WITHOUT_STATUS    EQU     2

.CODE

AsmVmxExitHandler PROC
	
	push 0;	为了和 pushfq 进行 16 字节对齐

	pushfq;
	push r15;
	push r14;
	push r13;
	push r12;
	push r11;
	push r10;
	push r9;
	push r8;
	push rdi;
	push rsi;
	push rbp;
	push rbp; rsp
	push rbx;
	push rdx;
	push rcx;
	push rax;

	sub rsp, 60h;
	movaps xmmword ptr [rsp +  0h], xmm0  ;
	movaps xmmword ptr [rsp + 10h], xmm1 ;
    movaps xmmword ptr [rsp + 20h], xmm2 ;
    movaps xmmword ptr [rsp + 30h], xmm3 ;
    movaps xmmword ptr [rsp + 40h], xmm4 ;
    movaps xmmword ptr [rsp + 50h], xmm5 ;


	mov rcx, rsp;
	add rcx, 060h; rcx ====>GUEST_REGS

	sub rsp, 0300h;
	call VehVmExitHandler;
	add rsp, 0300h;

	test rax, rax;
	jnz Resume;
	ret;

Resume:
	; restore XMM registers
	movaps xmm0, xmmword ptr [rsp +  0h];
    movaps xmm1, xmmword ptr [rsp + 10h];
    movaps xmm2, xmmword ptr [rsp + 20h];
    movaps xmm3, xmmword ptr [rsp + 30h];
    movaps xmm4, xmmword ptr [rsp + 40h];
    movaps xmm5, xmmword ptr [rsp + 50h];

	add rsp, 060h;
	pop rax;
	pop rcx;
	pop rdx;
	pop rbx;
	pop rbp; rsp
	pop rbp;
	pop rsi;
	pop rdi;
	pop r8;
	pop r9;
	pop r10;
	pop r11;
	pop r12;
	pop r13;
	pop r14;
	pop r15;
	popfq;
	add rsp, 08h;
	

	sub rsp, 0100h      ; to avoid error in future functions

	vmresume;  when vmresume was execuesed, the VM's IRQL restore from previous VM state? It's not the VMM's IRQL?

	ret
AsmVmxExitHandler ENDP

AsmResumeGuest PROC

	mov rcx, qword ptr [g_GuestRegisters];
	
	mov rdx,qword ptr [rcx+010h] ;
	mov rbx,qword ptr [rcx+018h] ;
	mov rsp ,qword ptr [rcx+020h] ;
	mov rbp, qword ptr [rcx+028h] ;
	mov rsi ,qword ptr [rcx+030h] ;
	mov rdi, qword ptr [rcx+038h] ;
	mov r8 ,qword ptr [rcx+040h] ;
	mov r9 ,qword ptr [rcx+048h] ;
	mov r10, qword ptr [rcx+050h] ;
	mov r11, qword ptr [rcx+058h] ;
	mov r12, qword ptr [rcx+060h] ;
	mov r13, qword ptr [rcx+068h] ;
	mov r14, qword ptr [rcx+070h] ;
	mov r15, qword ptr [rcx+078h] ;
	mov rax, qword ptr [rcx+080h] ;
	push rax;
	popfq;

	mov  rax, qword ptr [rcx];
	mov  rcx, qword ptr [rcx+8h];

	push qword ptr [g_ResumeRIP];
	ret;
AsmResumeGuest ENDP

AsmInvd PROC
	invd;
	ret;
AsmInvd ENDP;

; https://github.com/tandasat/HyperPlatform/blob/421d8d5efcc5a3d026e7cc3cca82c5c35430750e/HyperPlatform/Arch/x64/x64.asm#L374
; EXTERN_C UCHAR AsmInvept(
;     IN INEPT_TYPE invept_type,
;     IN INVEPT_DESCRIPTOR *invept_descriptor);
AsmInvept PROC
    invept rcx, oword ptr [rdx]
    jz errorWithCode        ; if (ZF) jmp
    jc errorWithoutCode     ; if (CF) jmp
    xor rax, rax            ; return VMX_OK
    ret

errorWithoutCode:
    mov rax, VMX_ERROR_WITHOUT_STATUS
    ret

errorWithCode:
    mov rax, VMX_ERROR_WITH_STATUS
    ret
AsmInvept ENDP


END