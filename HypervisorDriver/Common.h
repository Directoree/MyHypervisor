#pragma once
#include <ntddk.h>


#define	VMX_SUCCESS	0



typedef struct _CPUID
{
    ULONG EAX;
    ULONG EBX;
    ULONG ECX;
    ULONG EDX;
} CPUID, * PCPUID;

typedef struct _VIRTUAL_MACHINE_STATE
{
	BOOLEAN IsOnVmxRootMode;										// Detects whether the current logical core is on Executing on VMX Root Mode
	BOOLEAN ResumeToVm;
	BOOLEAN IncrementRip;											// Checks whether it has to redo the previous instruction or not (it used mainly in Ept routines)
	BOOLEAN HasLaunched;											// Indicate whether the core is virtualized or not
	ULONG64 ExitRip;
	ULONG64 ExitInstructionLength;
	ULONG64 VmxonRegionAllocVirtualAddress;				// Used to free buffer in vmxoff
	ULONG64 VmxonRegionAlignedPhysicalAddress;								// VMXON region physical address
	ULONG64 VmxonRegionAlignedVirtualAddress;							    // VMXON region virtual address
	ULONG64 VmvsRegionAllocVirtualAddress;				// Used to free buffer in vmxoff
	ULONG64 VmcsRegionAlignedPhysicalAddress;								// VMCS region physical address
	ULONG64 VmcsRegionAlignedVirtualAddress;								// VMCS region virtual address
	ULONG64 VmmStack;												// Stack for VMM in VM-Exit State
	ULONG64 MsrBitmapVirtualAddress;									// Msr Bitmap Virtual Address
	ULONG64 MsrBitmapPhysicalAddress;								// Msr Bitmap Physical Address
	// VMX_VMXOFF_STATE VmxoffState;									// Shows the vmxoff state of the guest
	// PEPT_HOOKED_PAGE_DETAIL MtfEptHookRestorePoint;                 // It shows the detail of the hooked paged that should be restore in MTF vm-exit
	BOOLEAN bLowerIRQL;
} VIRTUAL_MACHINE_STATE, * PVIRTUAL_MACHINE_STATE;

/* Size of GUEST_REGS = 0x88 bytes */
typedef struct _GUEST_REGS
{
	ULONG64 rax;                  // 0x00         
	ULONG64 rcx;
	ULONG64 rdx;                  // 0x10
	ULONG64 rbx;
	ULONG64 rsp;                  // 0x20         // rsp is not stored here
	ULONG64 rbp;
	ULONG64 rsi;                  // 0x30
	ULONG64 rdi;
	ULONG64 r8;                   // 0x40
	ULONG64 r9;
	ULONG64 r10;                  // 0x50
	ULONG64 r11;
	ULONG64 r12;                  // 0x60
	ULONG64 r13;
	ULONG64 r14;                  // 0x70
	ULONG64 r15;
	ULONG64 rflags;              // 0x80
} GUEST_REGS, * PGUEST_REGS;

/* Save current Guest registers */
typedef struct _CURRENT_REGISTERS
{
	ULONG64 Current_RIP;
	ULONG64 Current_RIP_Len;
	ULONG InstructionError;
	ULONG ExitReason;
}CURRENT_REGISTERS, PCURRENT_REGISTERS;

typedef union _SEGMENT_ATTRIBUTES
{
	USHORT UCHARs;
	struct
	{
		USHORT TYPE : 4;              /* 0;  Bit 40-43 */
		USHORT S : 1;                 /* 4;  Bit 44 */
		USHORT DPL : 2;               /* 5;  Bit 45-46 */
		USHORT P : 1;                 /* 7;  Bit 47 */

		USHORT AVL : 1;               /* 8;  Bit 52 */
		USHORT L : 1;                 /* 9;  Bit 53 */
		USHORT DB : 1;                /* 10; Bit 54 */
		USHORT G : 1;                 /* 11; Bit 55 */
		USHORT GAP : 4;

	} Fields;
} SEGMENT_ATTRIBUTES, * PSEGMENT_ATTRIBUTES;

typedef struct _SEGMENT_SELECTOR
{
	USHORT SELECTOR;
	SEGMENT_ATTRIBUTES ATTRIBUTES;
	ULONG32 LIMIT;
	ULONG64 BASE;
} SEGMENT_SELECTOR, * PSEGMENT_SELECTOR;

typedef struct _SEGMENT_DESCRIPTOR
{
	USHORT LIMIT0;
	USHORT BASE0;
	UCHAR  BASE1;
	UCHAR  ATTR0;
	UCHAR  LIMIT1ATTR1;
	UCHAR  BASE2;
} SEGMENT_DESCRIPTOR, * PSEGMENT_DESCRIPTOR;

// 
// typedef union _TR_SEGMENT_ATTRIBUTES
// {
// 	USHORT USHORTs;
// 	struct
// 	{
// 		USHORT TYPE : 4;              /* 0;  Bit 40-43 */
// 		USHORT S : 1;                 /* 4;  Bit 44 */
// 		USHORT DPL : 2;               /* 5;  Bit 45-46 */
// 		USHORT P : 1;                 /* 7;  Bit 47 */
// 
// 		USHORT LIMIT : 4;             /* 8;  Bit 47-51*/
// 
// 		USHORT AVL : 1;               /* 12;  Bit 52 */
// 		USHORT L : 1;                 /* 13;  Bit 53 */
// 		USHORT DB : 1;                /* 14; Bit 54 */
// 		USHORT G : 1;                 /* 15; Bit 55 */
// 
// 	} Fields;
// } TR_SEGMENT_ATTRIBUTES, * PTR_SEGMENT_ATTRIBUTES;
// 
// typedef struct _TR_SEGMENT_DESCRIPTOR
// {
// 	USHORT LIMIT0_16bits;
// 	USHORT BASE0_16bits;
// 	UCHAR  BASE1_8bits;
// 	TR_SEGMENT_ATTRIBUTES  ATTRIBUTES;
// 	UCHAR  BASE2_8bits;
// 	ULONG  BASE3_32bits;
// 	ULONG  Reserved;
// } TR_SEGMENT_DESCRIPTOR, * PTR_SEGMENT_DESCRIPTOR;

VIRTUAL_MACHINE_STATE* g_pGuestState;
CURRENT_REGISTERS* g_pCurrentGuestRigisters;
GUEST_REGS* g_GuestRegisters;
KIRQL g_PreviousIRQL;
ULONG64 g_ResumeRIP;
BOOLEAN g_uFirstIn;


