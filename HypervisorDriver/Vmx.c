#include <ntddk.h>
#include "Common.h"
#include "Msr.h"
#include "Vmx.h"
#include "VmxAllocateMemory.h"
#include "VmxRoutines.h"
#include "AsmVmxExitHandler.h"
#include "AsmSegmentHandler.h"
#include "AsmVmxEntryHandler.h"
#include "VmxVMExitHandler.h"



//
// 1. Call DetectVMXSupportAndEnableCr4, SetEntryVMXoperationCondition functions.
// 2. Check MSR_IA32_FEATURE_CONTROL lock, EnableSMX, EnableVmxon bits.
// 3. Check FIXED0 and FIXED1 correspondingly bits in CR0, CR4.
// 4. Call VmxAllocateVmxonRegion function.
// 5. Call VmxAllocateVmcsRegion function.
// 6. Use VMXON instruction into VMX root operation(use the __vmx_on function)
// 7. Check the return value is TRUE about __vmx_on function or not.
//
BOOLEAN VmxInitVirtualization()
{
	ULONG64 Cr0, Cr4 = 0;
	ULONG64 Cr0Fixed0 = 0, Cr0Fixed1 = 0, Cr4Fixed0 = 0, Cr4Fixed1 = 0;
	IA32_FEATURE_CONTROL_MSR MsrFeatureControl = { 0 };
	ULONG64 uMsrFeatureControl = 0;


	g_pGuestState->IsOnVmxRootMode = FALSE;

	if (VmrDetectVMXSupportAndEnableCr4() == FALSE)
	{
		DbgPrint("[*] Error : VmrDetectVMXSupportAndEnableCr4 have fault.\n");
		return FALSE;
	}
	if (VmrSetEntryVMXoperationCondition() == FALSE)
	{
		DbgPrint("[*] Error : VmrSetEntryVMXoperationCondition have fault.\n");
		return FALSE;
	}

	MsrFeatureControl.All = __readmsr(MSR_IA32_FEATURE_CONTROL);
	uMsrFeatureControl = MsrFeatureControl.All;

	DbgPrint("[*] MsrFeatureControl.All  = %llx\n", uMsrFeatureControl);

	if ((uMsrFeatureControl != 5) && (uMsrFeatureControl != 3))
	{
		DbgPrint("[*] Error : The MSR_IA32_FEATURE_CONTROL MSR lock, EnableSMX/EnableVmxon bits have fault.\n");
		return FALSE;
	}

	Cr0Fixed0 = __readmsr(MSR_IA32_VMX_CR0_FIXED0);
	Cr0Fixed1 = __readmsr(MSR_IA32_VMX_CR0_FIXED1);
	Cr4Fixed0 = __readmsr(MSR_IA32_VMX_CR4_FIXED0);
	Cr4Fixed1 = __readmsr(MSR_IA32_VMX_CR4_FIXED1);
	Cr0 = __readcr0();
	Cr4 = __readcr4();
	if (((Cr0Fixed0 & Cr0) != Cr0Fixed0) || ((Cr4Fixed0 & Cr4) != Cr4Fixed0) || ((Cr0Fixed1 | Cr0) != Cr0Fixed1) || ((Cr4Fixed1 | Cr4) != Cr4Fixed1))
	{
		DbgPrint("[*] Error : FIXED0 and FIXED1 correspondingly bits in CR0, CR4.\n");
		return FALSE;
	}


	if (VmxAllocateVmxonRegion() == FALSE)
	{
		DbgPrint("[*] Error : VmxAllocateVmxonRegion have fault.\n");
		return FALSE;
	}



	if (VmxAllocateVmcsRegion() == FALSE)
	{
		DbgPrint("[*] Error : VmxAllocateVmcsRegion have fault.\n");
		return FALSE;
	}

	DbgPrint("[*] Begin exe __vmx_on.\n");


	if (__vmx_on((PULONG64)&g_pGuestState->VmxonRegionAlignedPhysicalAddress) != VMX_SUCCESS)
	{
		DbgPrint("[*] Error : VMXON launch failed.\n");
		return FALSE;
	}

	g_pGuestState->IsOnVmxRootMode = TRUE;
	DbgPrint("[*] Now CPU in VMX root operation.\n");

	// Allocate MsrBitmap
	if (VmxAllocateMsrBitmap() == FALSE)
	{
		DbgPrint("[*] Error : Can't allocate buffer for Msrbitmaps.\n");
		return FALSE;
	}

	/* Allocate a buffer for Host(VMM Stack) */
	if (VmxAllocateVmmStack() == FALSE)
	{
		DbgPrint("[*] Error : Can't allocate buffer for VmmStack.\n");
		return FALSE;
	}

	// DbgPrint("[*] Now will exe __vmx_vmlaunch.\n");

	// DbgBreakPoint();

	// __vmx_vmlaunch();

	/* if Vmlaunch succeed will never be here ! */

	// __vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
	// DbgPrint("[*] Error : __vmx_vmlaunch failed with error code = %x.\n", ErrorCode);

	return TRUE;
}

BOOLEAN VmxEnableVirtualization(ULONG64 uGuestRSP)
{
	ULONG ErrorCode = 0;

	if (__vmx_vmclear(&g_pGuestState->VmcsRegionAlignedPhysicalAddress) != VMX_SUCCESS)
	{
		DbgPrint("[*] Error : __vmx_vmclear enable failed.\n");
		return FALSE;
	}

	if (__vmx_vmptrld(&g_pGuestState->VmcsRegionAlignedPhysicalAddress) != VMX_SUCCESS)
	{
		DbgPrint("[*] Error : __vmx_vmptrld enable failed.\n");
		return FALSE;
	}

	DbgBreakPoint();

	ULONG64 g_OldIRQL = 0;
	g_OldIRQL = __readcr8();

	if (g_OldIRQL < HIGH_LEVEL)
	{
		VehRaiseIrql();
	}

	VmxSetupVmcs(uGuestRSP);

	DbgPrint("[*] Now will exe __vmx_vmlaunch.\n");

	DbgBreakPoint();

	VehLowerIrql();
	
	__vmx_vmlaunch();

	/* if Vmlaunch succeed will never be here ! */

	__vmx_vmread(VM_INSTRUCTION_ERROR, &ErrorCode);
	DbgPrint("[*] Error : __vmx_vmlaunch failed with error code = %x.\n", ErrorCode);

	return TRUE;
}


// **============================================================================================================**//
// 1. Setup VM-execution control fields
//       控制域: （1）Pin-Based VM-Execution Control (32 bits)
//                       （2）Processor-Based VM-Execution Control
//	                           （2-1）primary processor-based VM-execution controls（32 bits）
//	                           （2-2）如果 primary processor-based[bit 31] = 1，才会有 secondary processor-based 字段，此时先先设置为 0。
//	                           （2-3）如果 primary processor-based[bit 17] = 1，才会有 tertiary VM-execution controls 字段，此时先先设置为 0。
//	                       则只需要将 Processor-Based 和 primary processor-based 两个控制域的保留位设置起来就好了
//	                      1、Pin-Based
//                                IA32_VMX_BASIC[55] = 0，IA32_VMX_PINBASED_CTLS（0x481）
//                                IA32_VMX_BASIC[55] = 1，IA32_VMX_TRUE_PINBASED_CTLS（0x48D）
//                        2、Processor-Based——primary processor-based
//                                IA32_VMX_BASIC[55] = 0，IA32_VMX_PROCBASED_CTLS（0x482）
//                                IA32_VMX_BASIC[55] = 1，IA32_VMX_TRUE_PROCBASED_CTLS（0x48E）
//       数据域：暂时不用设置
// **============================================================================================================**//
// 2. Setup VM-entry control fields
//   控制域：VM-entry Controls[bit 9] = 1，表示进入 VM 时，处理器运行在 IA-32e 模式。
//          IA32_VMX_BASIC[55] = 0，IA32_VMX_ENTRY_CTLS（0x484）
//          IA32_VMX_BASIC[55] = 1，IA32_VMX_TRUE_ENTRY_CTLS（0x490）
//   数据域：暂时不用设置
// **============================================================================================================**//
// 3. VM-exit control fields
//   控制域：VM-exit control[bit 9] = 1，表示退出 VM，进入 VVM 时，处理器运行在 IA-32e 模式。
//          IA32_VMX_BASIC[55] = 0，MSR_IA32_VMX_EXIT_CTLS（0x483）
//          IA32_VMX_BASIC[55] = 1，MSR_IA32_VMX_TRUE_EXIT_CTLS （0x48F）
//   数据域：暂时不用设置
// **============================================================================================================**//
// 4. Guest-state area
// **============================================================================================================**//
BOOLEAN VmxSetupVmcs(ULONG64 uGuestRSP)
{
	IA32_VMX_BASIC_MSR MsrVmxBasic = { 0 };

	MsrVmxBasic.All = __readmsr(MSR_IA32_VMX_BASIC);

	// Setup VM-execution control fields
	VmxSetupControls(&MsrVmxBasic);

	//  Setup VM-entry control fields  
	VmxSetupEntry(&MsrVmxBasic);

	// VM-exit control fields
	VmxSetupExit(&MsrVmxBasic);

	// Host-state area
	VmxSetupHost();

	// Guest-state area
	VmxSetupGuest(uGuestRSP);


	return TRUE;
}

VOID VmxSetupControls(PIA32_VMX_BASIC_MSR pMsrVmxBasic)
{	
	ULONG CpuBasedVmExecControls = 0;
	ULONG SecondaryProcBasedVmExecControls = 0;

	// Pin-Based Reserved values
	__vmx_vmwrite(PIN_BASED_VM_EXEC_CONTROL, VrAdjustControls(0, 
		pMsrVmxBasic->Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_PINBASED_CTLS : MSR_IA32_VMX_PINBASED_CTLS));

	// Processor-Based——primary processor-based
	CpuBasedVmExecControls = VrAdjustControls(CPU_BASED_ACTIVATE_MSR_BITMAP | CPU_BASED_ACTIVATE_SECONDARY_CONTROLS, \
		pMsrVmxBasic->Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_PROCBASED_CTLS : MSR_IA32_VMX_PROCBASED_CTLS);
	__vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, CpuBasedVmExecControls);

	// secondary processor-based
	SecondaryProcBasedVmExecControls = VrAdjustControls(CPU_BASED_CTL2_RDTSCP | CPU_BASED_CTL2_ENABLE_INVPCID | \
		CPU_BASED_CTL2_ENABLE_VMFUNC | CPU_BASED_CTL2_ENABLE_XSAVE_XRSTORS | CPU_BASED_CTL2_ENABLE_USER_WAIT_PAUSE | \
		CPU_BASED_CTL2_ENABLE_PCONFIG, MSR_IA32_VMX_PROCBASED_CTLS2);
	__vmx_vmwrite(SECONDARY_VM_EXEC_CONTROL, SecondaryProcBasedVmExecControls);

	// Set MSR Bitmaps
	__vmx_vmwrite(MSR_BITMAP, g_pGuestState->MsrBitmapPhysicalAddress);

	// Don't Hook 0~31 IDT, set to 0.
	__vmx_vmwrite(EXCEPTION_BITMAP, 0);

	// read shadow CR0, CR4. Protect these.
	__vmx_vmwrite(CR0_READ_SHADOW, __readcr0());
	__vmx_vmwrite(CR4_READ_SHADOW, __readcr4());
	

	return;
}

VOID VmxSetupEntry(PIA32_VMX_BASIC_MSR pMsrVmxBasic)
{
	__vmx_vmwrite(VM_ENTRY_CONTROLS, VrAdjustControls(VM_ENTRY_IA32E_MODE,
		pMsrVmxBasic->Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_ENTRY_CTLS : MSR_IA32_VMX_ENTRY_CTLS));

	__vmx_vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
	__vmx_vmwrite(VM_ENTRY_INTR_INFO, 0);

	return;
}

VOID VmxSetupExit(PIA32_VMX_BASIC_MSR pMsrVmxBasic)
{
	__vmx_vmwrite(VM_EXIT_CONTROLS, VrAdjustControls(VM_EXIT_IA32E_MODE,
		pMsrVmxBasic->Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_EXIT_CTLS : MSR_IA32_VMX_EXIT_CTLS));

	__vmx_vmwrite(VM_EXIT_MSR_STORE_COUNT, 0);
	__vmx_vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);

	return;
}



VOID VmxSetupHost()
{
	SEGMENT_SELECTOR SegmentSelector = { 0 };
	ULONG64 TrSegmentRegDescriptorBase = 0;

	// 设置控制寄存器
	__vmx_vmwrite(HOST_CR0, __readcr0());
	__vmx_vmwrite(HOST_CR3, __readcr3());
	__vmx_vmwrite(HOST_CR4, __readcr4());

	// 设置 HOST_RSP、HOST_RIP
	__vmx_vmwrite(HOST_RSP, (ULONG64)g_pGuestState->VmmStack + VMM_STACK_SIZE);
	__vmx_vmwrite(HOST_RIP, (ULONG64)&AsmVmxExitHandler);

	DbgPrint("[*] (ULONG64)CurrentGuestState->VmmStack = %llx\n", (ULONG64)g_pGuestState->VmmStack);
	DbgPrint("[*] (ULONG64)CurrentGuestState->VmmStack + VMM_STACK_SIZE = %llx\n", (ULONG64)g_pGuestState->VmmStack + VMM_STACK_SIZE);
	

	// 段寄存器选择子
	__vmx_vmwrite(HOST_ES_SELECTOR, AsmGetEs() & 0xFFF8);
	__vmx_vmwrite(HOST_CS_SELECTOR, AsmGetCs() & 0xFFF8);
	__vmx_vmwrite(HOST_SS_SELECTOR, AsmGetSs() & 0xFFF8);
	__vmx_vmwrite(HOST_DS_SELECTOR, AsmGetDs() & 0xFFF8);
	__vmx_vmwrite(HOST_FS_SELECTOR, AsmGetFs() & 0xFFF8);
	__vmx_vmwrite(HOST_GS_SELECTOR, AsmGetGs() & 0xFFF8);
	__vmx_vmwrite(HOST_TR_SELECTOR, AsmGetTr() & 0xFFF8);

	// TR 基址
	VrGetSegmentDescriptor(&SegmentSelector, (USHORT)AsmGetTr(), (PUCHAR)AsmGetGdtBase());
	__vmx_vmwrite(HOST_TR_BASE, SegmentSelector.BASE);


	__vmx_vmwrite(HOST_GDTR_BASE, AsmGetGdtBase());
	__vmx_vmwrite(HOST_IDTR_BASE, AsmGetIdtBase());

	// FS、GS
	__vmx_vmwrite(HOST_FS_BASE, __readmsr(MSR_FS_BASE));
	__vmx_vmwrite(HOST_GS_BASE, __readmsr(MSR_GS_BASE));

	// HOST_SYSENTER_CS、HOST_SYSENTER_EIP、HOST_SYSENTER_ESP
	__vmx_vmwrite(HOST_SYSENTER_CS, __readmsr(MSR_IA32_SYSENTER_CS));
	__vmx_vmwrite(HOST_SYSENTER_EIP, __readmsr(MSR_IA32_SYSENTER_EIP));
	__vmx_vmwrite(HOST_SYSENTER_ESP, __readmsr(MSR_IA32_SYSENTER_ESP));


	// __vmx_vmwrite(HOST_IA32_PAT, __readmsr(IA32_MSR_PAT));
	// __vmx_vmwrite(HOST_IA32_EFER, __readmsr(IA32_MSR_EFER));

	return;
}

VOID VmxSetupGuest(ULONG64 uGuestRSP)
{
	ULONG64 uGdtBase = 0;
	ULONG64 uIdtBase = 0;
	// 设置控制寄存器
	__vmx_vmwrite(GUEST_CR0, __readcr0());
	__vmx_vmwrite(GUEST_CR3, __readcr3());
	__vmx_vmwrite(GUEST_CR4, __readcr4());

	// 调试寄存器 DR7
	__vmx_vmwrite(GUEST_DR7, __readdr(7));

	// RFLAGS
	__vmx_vmwrite(GUEST_RFLAGS, __readeflags());

	// RIP, RSP
	__vmx_vmwrite(GUEST_RIP, (ULONG64)AsmVmxEntryHandler);
	__vmx_vmwrite(GUEST_RSP, (ULONG64)uGuestRSP);

	// 段寄存器 CS, SS, DS, ES, FS, GS, TR, LDTR
	uGdtBase = AsmGetGdtBase();
	uIdtBase = AsmGetIdtBase();

	VrFillGuestSelectorData((PVOID)uGdtBase, 0, AsmGetEs());
	VrFillGuestSelectorData((PVOID)uGdtBase, 1, AsmGetCs());
	VrFillGuestSelectorData((PVOID)uGdtBase, 2, AsmGetSs());
	VrFillGuestSelectorData((PVOID)uGdtBase, 3, AsmGetDs());
	VrFillGuestSelectorData((PVOID)uGdtBase, 4, AsmGetFs());
	VrFillGuestSelectorData((PVOID)uGdtBase, 5, AsmGetGs());
	VrFillGuestSelectorData((PVOID)uGdtBase, 6, AsmGetLdtr());
	VrFillGuestSelectorData((PVOID)uGdtBase, 7, AsmGetTr());

	// FS, GS Base
	__vmx_vmwrite(GUEST_FS_BASE, __readmsr(MSR_FS_BASE));
	__vmx_vmwrite(GUEST_GS_BASE, __readmsr(MSR_GS_BASE));

	// GDTR, IDTR
	__vmx_vmwrite(GUEST_GDTR_BASE, uGdtBase);
	__vmx_vmwrite(GUEST_IDTR_BASE, uIdtBase);
	__vmx_vmwrite(GUEST_GDTR_LIMIT, AsmGetGdtLimit());
	__vmx_vmwrite(GUEST_IDTR_LIMIT, AsmGetIdtLimit());

	// GUEST_IA32_DEBUGCTL, 64 bits
	__vmx_vmwrite(GUEST_IA32_DEBUGCTL, __readmsr(MSR_IA32_DEBUGCTL) & 0xFFFFFFFF);
	__vmx_vmwrite(GUEST_IA32_DEBUGCTL_HIGH, __readmsr(MSR_IA32_DEBUGCTL) >> 32);

	// GUEST_SYSENTER_CS、GUEST_SYSENTER_EIP、GUEST_SYSENTER_ESP
	__vmx_vmwrite(GUEST_SYSENTER_CS, __readmsr(MSR_IA32_SYSENTER_CS));
	__vmx_vmwrite(GUEST_SYSENTER_EIP, __readmsr(MSR_IA32_SYSENTER_EIP));
	__vmx_vmwrite(GUEST_SYSENTER_ESP, __readmsr(MSR_IA32_SYSENTER_ESP));

	// Setting the link pointer to the required value for 4KB VMCS.
	__vmx_vmwrite(VMCS_LINK_POINTER, -1);	// ~0ULL

	

	return;
}

