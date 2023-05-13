#pragma once
#include <ntddk.h>

//  Detect this CPU is Intel or not.
BOOLEAN VmrDetectVMXSupportAndEnableCr4();

//
// 1. Detect IA32_FEATURE_CONTROL(0x3A) lock(bit0), EnableSMX(bit1), EnableVmxon(bit2) and set.
// 2. If bit X is 1 in FIXED0, then set that bits of CR0 and CR4 to 1.
// 3. If bit X is 0 in FIXED1, then set that bits of CR0 and CR4 to 0.
//
BOOLEAN VmrSetEntryVMXoperationCondition();



BOOLEAN VmxTerminalVmxOff(IN VIRTUAL_MACHINE_STATE* CurrentGuestState);

BOOLEAN VrGetSegmentDescriptor(PSEGMENT_SELECTOR SegmentSelector, USHORT Selector, PUCHAR GdtBase);

BOOLEAN VrGetTrSegmentDescriptorBase(IN USHORT TrSelector, IN ULONG64 GdtBase);

/* Returns the Cpu Based and Secondary Processor Based Controls and other controls based on hardware support */
ULONG VrAdjustControls(ULONG uInitialValue, ULONG Msr);

/* Fill the guest's selector data */
VOID VrFillGuestSelectorData(PVOID GdtBase, ULONG Index, USHORT Selector);

/* The broadcast function which initialize the guest. */
VOID VrDpcBroadcastInitializeGuest(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);


