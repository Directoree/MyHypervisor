;EXTERN function_B:PROC
EXTERN VmxEnableVirtualization:PROC
EXTERN g_GuestRegisters:qword


.CODE


; function_A
AsmVmxSaveRegisters PROC
	; Save registers
	pushfq;
	push rax ;
	push rcx ;
	push rdx ;
	push rbx ;
	push rbp ;
	push rsi ;
	push rdi ;
	push r8 ;
	push r9 ;
	push r10 ;
	push r11 ;
	push r12 ;
	push r13 ;
	push r14 ;
	push r15 ;

	int 3;

	sub rsp, 0100h ;
	mov rcx, rsp ;
	call VmxEnableVirtualization ; call function_B

	int 3 ; we should never reach here as we execute vmlaunch in the above function.
	         ; if rax is FALSE then it's an indication of error

	jmp AsmVmxEntryHandler ;

	ret
AsmVmxSaveRegisters ENDP

; GUEST_RIP
AsmVmxEntryHandler PROC
	; Restore registers

	add rsp, 0100h ;
	
	pop r15 ;
	pop r14	;
	pop r13	;
	pop r12	;
	pop r11	;
	pop r10	;
	pop r9 ;
	pop r8 ;
	pop rdi ;
	pop rsi ;
	pop rbp	;
	pop rbx	;
	pop rdx	;
	pop rcx ;
	pop rax ;
	
	popfq	; 

	int 3 ;
	;vmcall;

	ret
AsmVmxEntryHandler ENDP



END