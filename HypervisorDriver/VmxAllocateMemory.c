#include <ntddk.h>
#include "Common.h"
#include "Msr.h"
#include "VmxAllocateMemory.h"


//
// 1. 通过 IA32_VMX_BASIC[44:32] 得到 VMXON region 所需空间大小。
// 2. 通过 IA32_VMX_BASIC[53:50] 得到 VMXON region 所需内存类型，如 write-back。
// 3. 调用 MmAllocateContiguousMemorySpecifyCache 函数申请连续指定大小和类型的物理内存。
// 4. 判断返回地址是否在边界对齐 4KB（bit 11:0 为 0）。
// 5. 将 IA32_VMX_BASIC[31:0] 的 VMCS revision identifier 写入 VMXON region 前 4 字节。
// 6. 将 VMXON region 的 bit 31 = 0。
// 5. Write VMXON region lowest 4 bytes from IA32_VMX_BASIC[31:0] that VMCS revision identifier.
// 6. Set VMXON region bit 31 to 0.
// 7. 使用 VMXON 进入 VMX root operation（使用 __vmx_on 函数）。
// 8. 判断 __vmx_on 函数的返回值是否是 TRUE。
//
BOOLEAN VmxAllocateVmxonRegion()
{
	ULONG VmxRegionSize = 0;
	UCHAR VmxRegionType = 0;
	PVOID pVmxRegion_VA = NULL;
	IA32_VMX_BASIC_MSR MsrVmxBasic = { 0 };
	IA32_FEATURE_CONTROL_MSR MsrControl = { 0 };
	PHYSICAL_ADDRESS pVmxRegion_Phy = { 0 };
	PHYSICAL_ADDRESS Lowphys = { 0 }, Highphys = { 0 };
	ULONG64 uAlignVirtualAddress = 0;
	ULONG64 uAlignPhysicalAddress = 0;


	// get the Vmx region size and type.
	MsrVmxBasic.All = __readmsr(MSR_IA32_VMX_BASIC);
	if (MsrVmxBasic.All)
	{
		VmxRegionSize = (MsrVmxBasic.All >> 32) & 0x1FFF;
		VmxRegionType = (MsrVmxBasic.All >> 50) & 0xF;
	}

	if (VmxRegionSize != PAGE_SIZE)
	{
		DbgPrint("[*] Error : The VMX region size is error getting from IA32_VMX_BASIC[44:32].");
		return FALSE;
	}

	do
	{

		// PVOID MmAllocateContiguousMemorySpecifyCache(
		// [in]           SIZE_T              NumberOfBytes,
		// [in]           PHYSICAL_ADDRESS    LowestAcceptableAddress,
		// [in]           PHYSICAL_ADDRESS    HighestAcceptableAddress,
		// [in, optional] PHYSICAL_ADDRESS    BoundaryAddressMultiple,
		// [in]           MEMORY_CACHING_TYPE CacheType
		// );
		Lowphys.QuadPart = 0;
		Highphys.QuadPart = -1;

		switch (VmxRegionType)
		{
		case 0:	// uncache
			pVmxRegion_VA = MmAllocateContiguousMemorySpecifyCache(VmxRegionSize * 2, Lowphys, Highphys, Lowphys, MmNonCached);
			break;
		case 6:	// write-back
			pVmxRegion_VA = MmAllocateContiguousMemorySpecifyCache(VmxRegionSize * 2, Lowphys, Highphys, Lowphys, MmCached);
			break;
		}

		if (pVmxRegion_VA == NULL)
		{
			DbgPrint("[*] Error : Couldn't Allocate Buffer for VMXON Region.");
			return FALSE;
		}

		RtlSecureZeroMemory(pVmxRegion_VA, VmxRegionSize);
		pVmxRegion_Phy = MmGetPhysicalAddress(pVmxRegion_VA);
		uAlignVirtualAddress = (ULONG64)pVmxRegion_VA;
		uAlignPhysicalAddress = (ULONG64)pVmxRegion_Phy.QuadPart;

		// align the physical address to 0x1000(bit 11:0 to 0)
		// the address align formula: if align to n bytes,(address + n-1) & (~(n-1))
		// https://catecat.top/post/WinXP-Struct/
		uAlignPhysicalAddress = ((ULONG64)uAlignPhysicalAddress + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

		while (((uAlignPhysicalAddress & 0xFFF) != 0) && ((ULONG64)uAlignPhysicalAddress - (ULONG64)pVmxRegion_Phy.QuadPart <= PAGE_SIZE))
		{
			uAlignPhysicalAddress++;
			uAlignPhysicalAddress = ((ULONG64)uAlignPhysicalAddress + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));
		}
	} while ((uAlignPhysicalAddress & 0xFFF) != 0);


	if (uAlignVirtualAddress == 0)
	{
		DbgPrint("[*] Error : MmGetVirtualForPhysical have a fault in VmxAllocateVmxonRegion function.\n");
		return FALSE;
	}

	pVmxRegion_Phy.QuadPart = uAlignPhysicalAddress;
	uAlignVirtualAddress = (ULONG64)MmGetVirtualForPhysical(pVmxRegion_Phy);


	g_pGuestState->VmvsRegionAllocVirtualAddress = (ULONG64)pVmxRegion_VA;
	g_pGuestState->VmxonRegionAlignedVirtualAddress = uAlignVirtualAddress;
	g_pGuestState->VmxonRegionAlignedPhysicalAddress = uAlignPhysicalAddress;

	DbgPrint("[*] Virtual allocated buffer for VMXON at %llx\n", (ULONG64)pVmxRegion_VA);
	DbgPrint("[*] Virtual aligned allocated buffer for VMXON at %llx\n", uAlignVirtualAddress);
	DbgPrint("[*] Aligned physical buffer allocated for VMXON at %llx\n", uAlignPhysicalAddress);

	*(PULONG64)g_pGuestState->VmxonRegionAlignedVirtualAddress = MsrVmxBasic.Fields.RevisionIdentifier;
	_bittestandreset64((PULONG64)g_pGuestState->VmxonRegionAlignedVirtualAddress, 31);

	return TRUE;
}

// 
// 1. 通过 IA32_VMX_BASIC[44:32] 得到 VMCS 所需空间大小（0x1000，4096 bytes）。
// 2. 通过 IA32_VMX_BASIC[53:50] 得到 VMCS 所需内存类型，如 write-back。
// 3. 调用 MmAllocateContiguousMemorySpecifyCache 函数申请连续指定大小和类型的物理内存。
// 4. 判断返回地址是否在边界对齐 4KB（bit 11:0 为 0）。
// 5. 将 IA32_VMX_BASIC[31:0] 的 VMCS revision identifier 写入 VMCS 前 4 字节的 [bit30:0]。
// 6. 将 VMCS 的 bit 31 = 0（不是 shadow VMCS）。
// 7. 使用 VMCLEAR 使 VMCS region 处于 clear 状态。
// 8. 使用 VMPTRLD 使 VMCS 变成 active、current。
// 
BOOLEAN VmxAllocateVmcsRegion()
{
	IA32_VMX_BASIC_MSR MsrVmxBasic = { 0 };
	ULONG VmcsRegionSize = 0;
	UCHAR VmcsRegionType = 0;
	PVOID pVmcsRegion_VA = NULL;
	PHYSICAL_ADDRESS pVmcsRegion_Phy = { 0 };
	PHYSICAL_ADDRESS Lowphys = { 0 }, Highphys = { 0 };
	ULONG64 uAlignVirtualAddress = 0;
	ULONG64 uAlignPhysicalAddress = 0;

	// get the Vmcs region size and type.
	MsrVmxBasic.All = __readmsr(MSR_IA32_VMX_BASIC);
	if (MsrVmxBasic.All)
	{
		VmcsRegionSize = (MsrVmxBasic.All >> 32) & 0x1FFF;
		VmcsRegionType = (MsrVmxBasic.All >> 50) & 0xF;
	}

	if (VmcsRegionSize != PAGE_SIZE)
	{
		DbgPrint("[*] Error : The VMCS region size is error getting from IA32_VMX_BASIC[44:32].");
		return FALSE;
	}

	do
	{
		Lowphys.QuadPart = 0;
		Highphys.QuadPart = -1;

		switch (VmcsRegionType)
		{
		case 0:	// uncache
			pVmcsRegion_VA = MmAllocateContiguousMemorySpecifyCache(VmcsRegionSize * 2, Lowphys, Highphys, Lowphys, MmNonCached);
			break;
		case 6:	// write-back
			pVmcsRegion_VA = MmAllocateContiguousMemorySpecifyCache(VmcsRegionSize * 2, Lowphys, Highphys, Lowphys, MmCached);
			break;
		}

		if (pVmcsRegion_VA == NULL)
		{
			DbgPrint("[*] Error : Couldn't Allocate Buffer for VMCS Region.");
			return FALSE;
		}

		RtlSecureZeroMemory(pVmcsRegion_VA, VmcsRegionSize);
		pVmcsRegion_Phy = MmGetPhysicalAddress(pVmcsRegion_VA);
		uAlignVirtualAddress = (ULONG64)pVmcsRegion_VA;
		uAlignPhysicalAddress = (ULONG64)pVmcsRegion_Phy.QuadPart;

		// align the physical address to 0x1000(bit 11:0 to 0)
		// the address align formula: if align to n bytes,(address + n-1) & (~(n-1))
		// https://catecat.top/post/WinXP-Struct/
		uAlignPhysicalAddress = ((ULONG64)uAlignPhysicalAddress + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

		while (((uAlignPhysicalAddress & 0xFFF) != 0) && ((ULONG64)uAlignPhysicalAddress - (ULONG64)pVmcsRegion_Phy.QuadPart <= PAGE_SIZE))
		{
			uAlignPhysicalAddress++;
			uAlignPhysicalAddress = ((ULONG64)uAlignPhysicalAddress + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));
		}
	} while ((uAlignPhysicalAddress & 0xFFF) != 0);

	if (uAlignVirtualAddress == 0)
	{
		DbgPrint("[*] Error : MmGetVirtualForPhysical have a fault in VmxAllocateVmcsRegion function.\n");
		return FALSE;
	}

	pVmcsRegion_Phy.QuadPart = uAlignPhysicalAddress;
	uAlignVirtualAddress = (ULONG64)MmGetVirtualForPhysical(pVmcsRegion_Phy);

	g_pGuestState->VmvsRegionAllocVirtualAddress = (ULONG64)pVmcsRegion_VA;
	g_pGuestState->VmcsRegionAlignedVirtualAddress = uAlignVirtualAddress;
	g_pGuestState->VmcsRegionAlignedPhysicalAddress = uAlignPhysicalAddress;

	DbgPrint("[*] Virtual allocated buffer for VMCS at %llx\n", (ULONG64)pVmcsRegion_VA);
	DbgPrint("[*] Virtual aligned allocated buffer for VMCS at %llx\n", uAlignVirtualAddress);
	DbgPrint("[*] Aligned physical buffer allocated for VMCS at %llx\n", uAlignPhysicalAddress);

	*(PULONG64)g_pGuestState->VmcsRegionAlignedVirtualAddress = MsrVmxBasic.Fields.RevisionIdentifier;
	_bittestandreset64((PULONG64)g_pGuestState->VmcsRegionAlignedVirtualAddress, 31);


	return TRUE;
}


/* Allocate a buffer forr Msr Bitmap */
BOOLEAN VmxAllocateMsrBitmap()
{
	PHYSICAL_ADDRESS PhyMsrBitmap = { 0 };

	g_pGuestState->MsrBitmapVirtualAddress = (ULONG64)ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, 'MsrB');
	if (g_pGuestState->MsrBitmapVirtualAddress == 0)
	{
		DbgPrint("[*] Error : Can't allocate buffer for MsrBitmap.\n");
		return FALSE;
	}

	RtlSecureZeroMemory((PVOID)g_pGuestState->MsrBitmapVirtualAddress, PAGE_SIZE);

	PhyMsrBitmap = MmGetPhysicalAddress((PVOID)g_pGuestState->MsrBitmapVirtualAddress);
	g_pGuestState->MsrBitmapPhysicalAddress = (ULONG64)PhyMsrBitmap.QuadPart;

	return TRUE;
}

/* Allocate a buffer for Host(VMM Stack) */
BOOLEAN VmxAllocateVmmStack()
{
	g_pGuestState->VmmStack = (ULONG64)ExAllocatePoolWithTag(NonPagedPool, VMM_STACK_SIZE, 'VmmS');
	if (g_pGuestState->VmmStack == 0)
	{
		DbgPrint("[*] Error : Can't allocate buffer for VmmStack.\n");
		return FALSE;
	}

	RtlSecureZeroMemory((PVOID)g_pGuestState->VmmStack, VMM_STACK_SIZE);

	return TRUE;
}