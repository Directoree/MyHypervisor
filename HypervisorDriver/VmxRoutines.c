#include <ntddk.h>
#include "Common.h"
#include "Vmx.h"
#include "Msr.h"
#include "VmxAllocateMemory.h"
#include "AsmVmxEntryHandler.h"


//
// 1. Detect this CPU is Intel or not.
// 2. If CPU support VME: CPUID.01H:ECX.VMX[bit5] == 1, set CR4.VMXE(bit13) to 1. 
// 
BOOLEAN VmrDetectVMXSupportAndEnableCr4()
{
	CPUID Cpuid = { 0 };
	ULONG64 Cr4 = 0;

	// detect cpu is intel or not.
	__cpuid(&Cpuid, 0x0);

	// Intel: Genuntelinel
	// test CPUID.00H, RBX+RDX+RCX = Genuntelinel ?
	if (Cpuid.EBX == 0x756E6547 && Cpuid.ECX == 0x6C65746E && Cpuid.EDX == 0x49656E69)
	{
		RtlSecureZeroMemory((PVOID)&Cpuid, sizeof(CPUID));

		__cpuid(&Cpuid, 0x1);

		// test CPUID.01H:ECX.VMX[bit 5] = 1 ?
		if (_bittest(&Cpuid.ECX, 5) == 1)
		{
			DbgPrint("[*] This CPU support VMX!\n");

			// Enable cr4 register, set cr4.VMXE(bit 13) = 1
			Cr4 = __readcr4();
			_bittestandset((PLONG)&Cr4, 13);
			__writecr4(Cr4);
			return TRUE;
		}
		else DbgPrint("[*] This CPU Not support VMX!\n");
	}
	else DbgPrint("[*] This CPU Not Intel!\n");

	return FALSE;
}


//
// 1. Detect IA32_FEATURE_CONTROL(0x3A) lock(bit0), EnableSMX(bit1), EnableVmxon(bit2) and set.
// 2. If bit X is 1 in FIXED0, then set that bits of CR0 and CR4 to 1.
// 3. If bit X is 0 in FIXED1, then set that bits of CR0 and CR4 to 0.
//
BOOLEAN VmrSetEntryVMXoperationCondition()
{
	IA32_FEATURE_CONTROL_MSR MsrControl = { 0 };
	CPUID Cpuid = { 0 };
	ULONG64 Cr0, Cr4 = 0;
	BOOLEAN InSMXmode = FALSE;
	ULONG64 Cr0Fixed0, Cr0Fixed1, Cr4Fixed0, Cr4Fixed1 = 0;

	// detect CPU in SMX mode or not.
	__cpuid(&Cpuid, 0x1);

	if (_bittest((PLONG)&Cpuid.ECX, 6) == 1) // support SMX mode
	{
		Cr4 = __readcr4();
		if (_bittest((PLONG)&Cr4, 14) == 1)
		{
			InSMXmode = TRUE;
			DbgPrint("[*] This CPU in SMX mode.\n");
		}
		else
		{
			InSMXmode = FALSE;
			DbgPrint("[*] This CPU NOT in SMX mode.\n");
		}
	}

	// read MSR_IA32_FEATURE_CONTROL MSR register.
	// set Lock, EnableSMX, EnableVmxon.
	MsrControl.All = __readmsr(MSR_IA32_FEATURE_CONTROL);
	if (MsrControl.Fields.Lock == 0)
	{
		if (InSMXmode)
		{
			MsrControl.Fields.Lock = 1;
			MsrControl.Fields.EnableSMX = 1;
			__writemsr(MSR_IA32_FEATURE_CONTROL, MsrControl.All);
			DbgPrint("[*] Set entry VMX operation condition inside SMX mode OK.\n");
		}
		else
		{
			MsrControl.Fields.Lock = 1;
			MsrControl.Fields.EnableVmxon = 1;
			__writemsr(MSR_IA32_FEATURE_CONTROL, MsrControl.All);
			DbgPrint("[*] Set entry VMX operation condition outside SMX mode OK.\n");
		}
	}
	else if (MsrControl.Fields.EnableVmxon == FALSE)	// lock = 1, but EnableVmxon = 0
	{
		DbgPrint("[*] VMX locked off in BIOS\n");
		return FALSE;
	}

	// set CR0, CR4 according to FIXED0 and FIXED1.
	Cr0Fixed0 = __readmsr(MSR_IA32_VMX_CR0_FIXED0);
	Cr0Fixed1 = __readmsr(MSR_IA32_VMX_CR0_FIXED1);
	Cr4Fixed0 = __readmsr(MSR_IA32_VMX_CR4_FIXED0);
	Cr4Fixed1 = __readmsr(MSR_IA32_VMX_CR4_FIXED1);
	Cr0 = __readcr0();
	Cr4 = __readcr4();

	Cr0 |= Cr0Fixed0;
	Cr0 &= Cr0Fixed1;
	Cr4 |= Cr4Fixed0;
	Cr4 &= Cr4Fixed1;

	__writecr0(Cr0);
	__writecr4(Cr4);

	DbgPrint("[*] Now you can entry VMX operation.\n");

	return TRUE;
}



BOOLEAN VmxTerminalVmxOff(IN VIRTUAL_MACHINE_STATE* CurrentGuestState)
{
	__vmx_off();

	// 内存已经释放了，不需要再释放了
	// DbgBreakPoint();
	// 
	// DbgPrint("[*]CurrentGuestState->VmxonRegionAllocVirtualAddress = %llx\n", CurrentGuestState->VmxonRegionAllocVirtualAddress);
	// DbgPrint("[*](PVOID)CurrentGuestState->VmxonRegionAllocVirtualAddress = %llx\n", (PVOID)CurrentGuestState->VmxonRegionAllocVirtualAddress);
	// 
	// 
	// MmFreeContiguousMemorySpecifyCache((PVOID)(ULONG64)CurrentGuestState->VmxonRegionAllocVirtualAddress, 0x2000, MmCached);
	// MmFreeContiguousMemorySpecifyCache((PVOID)(ULONG64)CurrentGuestState->VmvsRegionAllocVirtualAddress, 0x2000, MmCached);
	// ExFreePoolWithTag((PVOID)g_pGuestState, 'GOSS');

	return TRUE;
}

/* Get Segment Descriptor */
BOOLEAN VrGetSegmentDescriptor(PSEGMENT_SELECTOR SegmentSelector, USHORT Selector, PUCHAR GdtBase)
{
	PSEGMENT_DESCRIPTOR SegDesc;

	if (!SegmentSelector)
		return FALSE;

	if (Selector & 0x4) {
		return FALSE;
	}

	SegDesc = (PSEGMENT_DESCRIPTOR)((PUCHAR)GdtBase + (Selector & ~0x7));

	SegmentSelector->SELECTOR = Selector;
	SegmentSelector->BASE = SegDesc->BASE0 | SegDesc->BASE1 << 16 | SegDesc->BASE2 << 24;
	SegmentSelector->LIMIT = SegDesc->LIMIT0 | (SegDesc->LIMIT1ATTR1 & 0xf) << 16;
	SegmentSelector->ATTRIBUTES.UCHARs = SegDesc->ATTR0 | (SegDesc->LIMIT1ATTR1 & 0xf0) << 4;

	// IA-32e mode, deal TR register.
	if (!(SegDesc->ATTR0 & 0x10)) { // LA_ACCESSED
		ULONG64 tmp;
		// this is a TSS or callgate etc, save the base high part
		tmp = (*(PULONG64)((PUCHAR)SegDesc + 8));
		SegmentSelector->BASE = (SegmentSelector->BASE & 0xffffffff) | (tmp << 32);
	}

	if (SegmentSelector->ATTRIBUTES.Fields.G) {
		// 4096-bit granularity is enabled for this segment, scale the limit
		SegmentSelector->LIMIT = (SegmentSelector->LIMIT << 12) + 0xfff;
	}

	return TRUE;
}

/* Fill the guest's selector data */
VOID VrFillGuestSelectorData(PVOID GdtBase, ULONG Index, USHORT Selector)
{
	SEGMENT_SELECTOR SegmentSelector = { 0 };
	ULONG AccessRights;

	VrGetSegmentDescriptor(&SegmentSelector, Selector, GdtBase);

	// 原段描述符中的低 8 位还在低 8 位，高 4 位放在 bit-12～bit-16
	AccessRights = ((PUCHAR)&SegmentSelector.ATTRIBUTES)[0] + (((PUCHAR)&SegmentSelector.ATTRIBUTES)[1] << 12);

	// ES, bit-16 = 1. O = usable, 1 =  unusable
	if (!Selector)
		AccessRights |= 0x10000;

	// 这几个段寄存器的 ID 值是连着的，如GUEST_ES_SELECTOR = 0x00000800, GUEST_CS_SELECTOR = 0x00000802
	__vmx_vmwrite(GUEST_ES_SELECTOR + Index * 2, Selector & 0xFFF8);
	__vmx_vmwrite(GUEST_ES_LIMIT + Index * 2, SegmentSelector.LIMIT);
	__vmx_vmwrite(GUEST_ES_AR_BYTES + Index * 2, AccessRights);
	__vmx_vmwrite(GUEST_ES_BASE + Index * 2, SegmentSelector.BASE);

}

// /* Get TR Segment Descriptor */
// ULONG64 VrGetTrSegmentDescriptorBase(IN USHORT TrSelector, IN ULONG64 GdtBase)
// {
// 	ULONG64 TrSegmentDescriptor = 0;
// 	ULONG64 BASE0_16bits = 0;
// 	ULONG64 BASE1_8bits = 0;
// 	ULONG64 BASE2_8bits = 0;
// 	ULONG64 BASE3_32bits = 0;
// 
// 	TrSegmentDescriptor = (ULONG64)GdtBase + (TrSelector & ~0x7);
// 
// 	BASE0_16bits = *(PUSHORT)(TrSegmentDescriptor + 2);
// 	BASE1_8bits = *(PUCHAR)(TrSegmentDescriptor + 4);
// 	BASE2_8bits = *(PUCHAR)(TrSegmentDescriptor + 7);
// 	BASE3_32bits = *(PULONG)(TrSegmentDescriptor + 8);
// 
// 	// DbgPrint("[*] BASE0_16bits = %llx\n", BASE0_16bits);
// 	// DbgPrint("[*] BASE1_8bits = %llx\n", BASE1_8bits);
// 	// DbgPrint("[*] BASE2_8bits = %llx\n", BASE2_8bits);
// 	// DbgPrint("[*] BASE3_32bits = %llx\n", BASE3_32bits);
// 
// 	TrSegmentDescriptor = BASE0_16bits | (BASE1_8bits << 16) |
// 			(BASE2_8bits << 24) | (BASE3_32bits << 32);
// 
// 	DbgPrint("[*] TR base = %llx\n", TrSegmentDescriptor);
// 
// 	return TrSegmentDescriptor;
// }

/* Returns the Cpu Based and Secondary Processor Based Controls and other controls based on hardware support */
ULONG VrAdjustControls(ULONG uInitialValue, ULONG Msr)
{
	MSR MsrValue = { 0 };

	MsrValue.Content = __readmsr(Msr);
	uInitialValue &= MsrValue.High;     /* bit == 0 in high word ==> must be zero */
	uInitialValue |= MsrValue.Low;      /* bit == 1 in low word  ==> must be one  */
	return uInitialValue;
}

typedef ULONG64 (*KESIGNALCALLDPCSYNCHRONIZE)(_In_ PVOID SystemArgument2);
typedef ULONG64(*KESIGCALLDPCDONE)(_In_ PVOID SystemArgument1);

/* The broadcast function which initialize the guest. */
VOID VrDpcBroadcastInitializeGuest(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
	UNICODE_STRING sKeSignalCallDpcSynchronize = { 0 };
	KESIGNALCALLDPCSYNCHRONIZE pKeSignalCallDpcSynchronize = NULL;
	KESIGCALLDPCDONE pKeSignalCallDpcDone = NULL;

	RtlInitUnicodeString(&sKeSignalCallDpcSynchronize, L"KeSignalCallDpcSynchronize");
	pKeSignalCallDpcSynchronize = (PVOID)MmGetSystemRoutineAddress(&sKeSignalCallDpcSynchronize);

	RtlInitUnicodeString(&sKeSignalCallDpcSynchronize, L"KeSignalCallDpcDone");
	pKeSignalCallDpcDone = (PVOID)MmGetSystemRoutineAddress(&sKeSignalCallDpcSynchronize);

	// Save the vmx state and prepare vmcs setup and finally execute vmlaunch instruction
	AsmVmxSaveRegisters();

	// Wait for all DPCs to synchronize at this point
	pKeSignalCallDpcSynchronize(SystemArgument2);

	// Mark the DPC as being complete
	pKeSignalCallDpcDone(SystemArgument1);
}

