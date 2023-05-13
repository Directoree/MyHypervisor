#pragma once
#include <ntddk.h>

// https://github.com/SinaKarvandi/Hypervisor-From-Scratch/blob/3eb8dace4a64c3ec51a4c6c09077e53664b38f67/Part%208%20-%20How%20To%20Do%20Magic%20With%20Hypervisor!/Hypervisor%20From%20Scratch/MyHypervisorDriver/Ept.h#L575
// https://github.com/tandasat/HyperPlatform/blob/421d8d5efcc5a3d026e7cc3cca82c5c35430750e/HyperPlatform/ia32_type.h#L1430
typedef union _EPTP
{
	struct
	{
		/**
		 * [Bits 2:0] EPT paging-structure memory type:
		 * - 0 = Uncacheable (UC)
		 * - 6 = Write-back (WB)
		 * Other values are reserved.
		 *
		 * @see Vol3C[28.2.6(EPT and memory Typing)]
		 */
		ULONG64 MemoryType : 3;

		/**
		 * [Bits 5:3] This value is 1 less than the EPT page-walk length.
		 *
		 * @see Vol3C[28.2.6(EPT and memory Typing)]
		 */
		ULONG64 PageWalkLength : 3;

		/**
		 * [Bit 6] Setting this control to 1 enables accessed and dirty flags for EPT.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		ULONG64 EnableAccessAndDirtyFlags : 1;
		ULONG64 Reserved1 : 5;

		/**
		 * [Bits 47:12] Bits N-1:12 of the physical address of the 4-KByte aligned EPT PML4 table.
		 */
		ULONG64 PageFrameNumber : 36;
		ULONG64 Reserved2 : 16;
	};

	ULONG64 Flags;
} EPTP, * PEPTP;

// https://github.com/SinaKarvandi/Hypervisor-From-Scratch/blob/3eb8dace4a64c3ec51a4c6c09077e53664b38f67/Part%208%20-%20How%20To%20Do%20Magic%20With%20Hypervisor!/Hypervisor%20From%20Scratch/MyHypervisorDriver/Ept.h#LL866C34-L866C34
// https://github.com/tandasat/HyperPlatform/blob/421d8d5efcc5a3d026e7cc3cca82c5c35430750e/HyperPlatform/ia32_type.h#L1601
typedef struct _INVEPT_DESCRIPTOR
{
	EPTP EptPointer;
	ULONG64 Reserved; // Must be zero.
} INVEPT_DESCRIPTOR, * PINVEPT_DESCRIPTOR;

// https://github.com/tandasat/HyperPlatform/blob/421d8d5efcc5a3d026e7cc3cca82c5c35430750e/HyperPlatform/ia32_type.h#LL1608C12-L1608C22
typedef enum _INEPT_TYPE
{
	enum_Inepttype_SingleContextInvalidation = 1,
	enum_Inepttype_GlobalInvalidation = 2,
}INEPT_TYPE, *PINEPT_TYPE;
