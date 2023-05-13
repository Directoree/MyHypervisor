
.CODE

;------------------------------------------------------------------------

AsmGetCs PROC
	xor eax, eax
	mov		rax, cs
	ret
AsmGetCs ENDP

;------------------------------------------------------------------------

AsmGetDs PROC
	xor eax, eax
	mov		rax, ds
	ret
AsmGetDs ENDP

;------------------------------------------------------------------------

AsmGetEs PROC
	xor eax, eax
	mov		rax, es
	ret
AsmGetEs ENDP

;------------------------------------------------------------------------

AsmGetSs PROC
	xor eax, eax
	mov		rax, ss
	ret
AsmGetSs ENDP

;------------------------------------------------------------------------

AsmGetFs PROC
	xor eax, eax
	mov		rax, fs
	ret
AsmGetFs ENDP

;------------------------------------------------------------------------

AsmGetGs PROC
	xor eax, eax
	mov		rax, gs
	ret
AsmGetGs ENDP

;------------------------------------------------------------------------

AsmGetTr PROC
	xor eax, eax
	str	rax
	ret
AsmGetTr ENDP

;------------------------------------------------------------------------
AsmGetLdtr PROC
	sldt	rax
	ret
AsmGetLdtr ENDP

;------------------------------------------------------------------------


AsmGetGdtBase PROC

	LOCAL	gdtr[10]:BYTE
	sgdt	gdtr
	mov		rax, QWORD PTR gdtr[2]
	ret
AsmGetGdtBase ENDP

;------------------------------------------------------------------------

AsmGetIdtBase PROC

	LOCAL	idtr[10]:BYTE
	
	sidt	idtr
	mov		rax, QWORD PTR idtr[2]
	ret

AsmGetIdtBase ENDP

;------------------------------------------------------------------------
AsmGetGdtLimit PROC

	LOCAL	gdtr[10]:BYTE

	sgdt	gdtr
	mov		ax, WORD PTR gdtr[0]
	ret

AsmGetGdtLimit ENDP

;------------------------------------------------------------------------

AsmGetIdtLimit PROC
	LOCAL	idtr[10]:BYTE
	
	sidt	idtr
	mov		ax, WORD PTR idtr[0]
	ret

AsmGetIdtLimit ENDP

;------------------------------------------------------------------------

END