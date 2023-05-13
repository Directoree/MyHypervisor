#include <ntddk.h>
#include "Common.h"
#include "Vmx.h"
#include "Ept.h"
#include "VmxRoutines.h"
#include "VmxVMExitHandler.h"
#include "AsmVmxExitHandler.h"

EXTERN_C ULONG64 g_OldIRQL;

/* Main Vmexit events handler */
BOOLEAN VehVmExitHandler(PGUEST_REGS GuestRegs)
{
	ULONG ErrorCode = 0;
	ULONG ExitReason = 0;	/* 32 bits */
	ULONG GuestActivityState = 0;

	DbgBreakPoint();
	ULONG64 uCR8 = 0;
	g_pGuestState->ExitRip = 0;
	g_pGuestState->ExitInstructionLength = 0;

	uCR8 = __readcr8();
	// DbgPrint("[*] In VMX IRQL =  %llx.\n", uCR8);
	/**
	 * @brief 
	 * 
	 * VM-exit 中，提升 IRQL 前，不能使用 DbgPrint 输出，不然会导致 VM 从
	 * Windbg 中失去控制，类似于调试器重入异常
	 * 
	 * Before raise the IRQL we can't use DbgPrint, it encounting error just like debuger trap again.
	 * 
	 */

	g_pGuestState->bLowerIRQL = FALSE;
	if (uCR8 < HIGH_LEVEL) 
	{ 
		VehRaiseIrql(); 
		g_pGuestState->bLowerIRQL = TRUE;
	}

	__vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
	// DbgPrint("[*]  : VM-exit failed with error code = %x.\n", ErrorCode);

	__vmx_vmread(VM_EXIT_REASON, &ExitReason);
	// DbgPrint("[*]  : VM-exit failed with exit reason = %x.\n", ExitReason);

	__vmx_vmread(GUEST_RIP, &g_pGuestState->ExitRip);
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &g_pGuestState->ExitInstructionLength);
	// DbgPrint("[*]  Error : VM-exit failed with CurrentRIP = %llx.\n", CurrentRIP);

	__vmx_vmread(GUEST_ACTIVITY_STATE, &GuestActivityState);
	// DbgPrint("[*]  : VM-exit failed with error code = %x,\n VM-exit failed with exit reason = %x\n, \
	// 	VM-exit failed with CurrentRIP = %llx,\nGuestActivityState = %x.\n", ErrorCode, ExitReason, CurrentRIP, GuestActivityState);


	switch (ExitReason & 0xFFFFFFFF)
    {
		DbgBreakPoint();
		/**
		 * @brief 
		 * 
		 * 25.1.2  Instructions That Cause VM Exits Unconditionally
		 * The following instructions cause VM exits when they are executed in VMX non-root operation: 
		 * CPUID, GETSEC,INVD, and XSETBV. This is also true of instructions introduced with VMX, which include: INVEPT, INVVPID,
		 * VMCALL, VMCLEAR, VMLAUNCH, VMPTRLD, VMPTRST, VMRESUME, VMXOFF, and VMXON.
		 * 
		 */

		case EXIT_REASON_VMCLEAR:
		case EXIT_REASON_VMPTRLD:
		case EXIT_REASON_VMPTRST:
		case EXIT_REASON_VMREAD:
		case EXIT_REASON_VMRESUME:
		case EXIT_REASON_VMWRITE:
		case EXIT_REASON_VMXOFF:
		case EXIT_REASON_VMXON:
		case EXIT_REASON_VMLAUNCH:
		{
       		// DbgBreakPoint();

       		/*	DbgPrint("\n [*] Target guest tries to execute VM Instruction ,"
       		     "it probably causes a fatal error or system halt as the system might"
       		     " think it has VMX feature enabled while it's not available due to our use of hypervisor.\n");
       		*/

       		ULONG RFLAGS = 0;
       		__vmx_vmread(GUEST_RFLAGS, &RFLAGS);
       		__vmx_vmwrite(GUEST_RFLAGS, RFLAGS | 0x1); // cf=1 indicate vm instructions fail
       		break;
		}
		case EXIT_REASON_CPUID:
		{
			VehHandleCpuid(GuestRegs);
			g_pGuestState->ResumeToVm = TRUE;
			break;
		}
		case EXIT_REASON_GETSEC:
		{
			VehHanldeGETSEC(GuestRegs);
			g_pGuestState->ResumeToVm = TRUE;
			break;
		}
		case EXIT_REASON_INVD:
		{
			AsmInvd();
			g_pGuestState->ResumeToVm = TRUE;
			break;
		}
		case EXIT_REASON_XSETBV:
		{
			VehHanldeXSETBV(GuestRegs);
			g_pGuestState->ResumeToVm = TRUE;
			break;
		}
		case EXIT_REASON_INVEPT:
		{
			break;
		}		
		case EXIT_REASON_RDTSC:
		{
			VehHanldeRDTSC(GuestRegs);
			g_pGuestState->ResumeToVm = TRUE;
			break;
		}	
		case EXIT_REASON_RDTSCP:
		{
			VehHanldeRDTSCP(GuestRegs);
			g_pGuestState->ResumeToVm = TRUE;
			break;
		}
		case EXIT_REASON_TRIPLE_FAULT:
		{
			//DbgPrint("[*]  Error : Triple fault error occured.");
			break;
		}
		case EXIT_REASON_HLT:
		{
		    break;
		}
		case EXIT_REASON_EXCEPTION_NMI:
		{
		    break;
		}

		case EXIT_REASON_VMCALL:
		{
		    break;
		}

		case EXIT_REASON_CR_ACCESS:
		{
		    break;
		}

		case EXIT_REASON_MSR_READ:
		{
		    break;
		}

		case EXIT_REASON_MSR_WRITE:
		{
		    break;
		}

		case EXIT_REASON_EPT_VIOLATION:
		{
		    break;
		}

		default:
		{
		    DbgBreakPoint();
		    break;
		}
			
    }

	 

	if(g_pGuestState->ResumeToVm == TRUE)
	{
		VehAdjustGuestRip();
		g_pGuestState->ResumeToVm = FALSE;
	}

	if (g_pGuestState->bLowerIRQL == TRUE)
	{
		VehLowerIrql();
		g_pGuestState->bLowerIRQL = FALSE;
	}

	uCR8 = __readcr8();

	return TRUE;
}


/* Handle Cpuid Vmexits*/
VOID VehHandleCpuid(PGUEST_REGS RegistersState)
{
	INT32 cpu_info[4];

	DbgBreakPoint();
	// DbgPrint("[*] CPUID instruction exed in VMM.\n");

	if (RegistersState->rax == 0x6666)
	{
		RegistersState->rax = 0x11111111;
		RegistersState->rbx = 0x22222222;
		RegistersState->rcx = 0x33333333;
		RegistersState->rdx = 0x44444444;
	}
	else
	{
		__cpuidex(cpu_info, (INT32)RegistersState->rax, (INT32)RegistersState->rcx);
		RegistersState->rax = cpu_info[0];
		RegistersState->rbx = cpu_info[1];
		RegistersState->rcx = cpu_info[2];
		RegistersState->rdx = cpu_info[3];
	}

	return;
}

VOID VehHanldeGETSEC(PGUEST_REGS RegistersState)
{
	DbgBreakPoint();
	DbgPrint("EXIT_REASON_GETSEC  rip:%llx\n", g_pGuestState->ExitRip);
	return;


}

VOID VehHanldeXSETBV(PGUEST_REGS RegistersState)
{
	/**
	 * @brief 
	 * 
	 * XSETBV. It is executed at the time of system resuming
	 * https://github.com/tandasat/HyperPlatform/blob/421d8d5efcc5a3d026e7cc3cca82c5c35430750e/HyperPlatform/vmm.cpp#L474
	 * https://github.com/smallzhong/myvt/blob/c3d063c18087be05781fe24ccd709379cd32f2b3/VT/vmxHandler.c#L425
	 * 
	 */
	ULARGE_INTEGER value = { 0 };

	value.LowPart = (RegistersState->rax & 0xFFFFFFFF);
	value.HighPart = (RegistersState->rdx & 0xFFFFFFFF);
	_xsetbv(RegistersState->rcx, value.QuadPart);

	return;
}

VOID VehHanldeINVEPT(PGUEST_REGS RegistersState)
{
	/**
	 * @brief 
	 * https://github.com/tandasat/HyperPlatform/blob/421d8d5efcc5a3d026e7cc3cca82c5c35430750e/HyperPlatform/Arch/x64/x64.asm#LL377C15-L377C15
	 * https://github.com/tandasat/HyperPlatform/blob/421d8d5efcc5a3d026e7cc3cca82c5c35430750e/HyperPlatform/util.cpp#LL818C3-L818C30
	 * 
	 */
	INVEPT_DESCRIPTOR Inveptdesc = { 0 };
	AsmInvept(enum_Inepttype_GlobalInvalidation, &Inveptdesc);
	return;
}

VOID VehHanldeRDTSC(PGUEST_REGS RegistersState)
{
	/**
	 * @brief 
	 * https://github.com/tandasat/HyperPlatform/blob/421d8d5efcc5a3d026e7cc3cca82c5c35430750e/HyperPlatform/vmm.cpp#L448
	 */
	int aunx = 0;
	ULARGE_INTEGER tsc = { 0 };

	tsc.QuadPart =  __rdtsc();
	RegistersState->rax = tsc.LowPart;
	RegistersState->rdx = tsc.HighPart;

	return;
}

VOID VehHanldeRDTSCP(PGUEST_REGS RegistersState)
{
	/**
	 * @brief 
	 * https://github.com/Bareflank/hypervisor/issues/518
	 * https://github.com/smallzhong/myvt/blob/c3d063c18087be05781fe24ccd709379cd32f2b3/VT/vmxHandler.c#L414
	 * https://github.com/tandasat/HyperPlatform/blob/421d8d5efcc5a3d026e7cc3cca82c5c35430750e/HyperPlatform/vmm.cpp#L460
	 */
	int aunx = 0;
	ULARGE_INTEGER tsc = { 0 };

	tsc.QuadPart =  __rdtscp(&aunx);
	RegistersState->rax = tsc.LowPart;
	RegistersState->rdx = tsc.HighPart;
	RegistersState->rcx = aunx;

	return;
}


VOID VehRaiseIrql()
{ 
	KeRaiseIrql(HIGH_LEVEL, &g_PreviousIRQL);
	return;
}

VOID VehLowerIrql()
{
	KeLowerIrql(g_PreviousIRQL);
	__writecr8((ULONG64)g_PreviousIRQL);

	return;
}

BOOLEAN VehAdjustGuestRip()
{
	if ((g_pGuestState->ExitRip != 0) && (g_pGuestState->ExitInstructionLength != 0))
	{
		g_pGuestState->ExitRip += g_pGuestState->ExitInstructionLength;
		__vmx_vmwrite(GUEST_RIP, g_pGuestState->bLowerIRQL);

		return TRUE;
	}

	return FALSE;

}

