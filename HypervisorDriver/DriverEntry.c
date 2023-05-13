#include <ntddk.h>
#include "Vmx.h"
#include "Common.h"
#include "Msr.h"
#include "VmxAllocateMemory.h"
#include "VmxRoutines.h"
#include "AsmVmxEntryHandler.h"
#include "VmxVMExitHandler.h"


VOID DriverUnload(PDRIVER_OBJECT pDriver)
{
	UNREFERENCED_PARAMETER(pDriver);
	DbgPrint("Goodbye~\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pRegPath)
{
	UNREFERENCED_PARAMETER(pRegPath);
	pDriver->DriverUnload = DriverUnload;
	NTSTATUS Status = STATUS_SUCCESS;
	

	g_pGuestState = ExAllocatePoolWithTag(NonPagedPool, sizeof(VIRTUAL_MACHINE_STATE), 'GueS');
	if (!g_pGuestState)
	{
		// we use DbgPrint as the vmx-root or non-root is not initialized
		DbgPrint("[*] Error : We can not allocate buffer for VIRTUAL_MACHINE_STATE.\n");
		return FALSE;
	}
	g_pCurrentGuestRigisters = ExAllocatePoolWithTag(NonPagedPool, sizeof(CURRENT_REGISTERS), 'Creg');
	if (!g_pCurrentGuestRigisters)
	{
		// we use DbgPrint as the vmx-root or non-root is not initialized
		DbgPrint("[*] Error : We can not allocate buffer for CURRENT_REGISTERS.\n");
		return FALSE;
	}

	g_GuestRegisters = ExAllocatePoolWithTag(NonPagedPool, sizeof(GUEST_REGS), 'Gueg');
	if (!g_GuestRegisters)
	{
		// we use DbgPrint as the vmx-root or non-root is not initialized
		DbgPrint("[*] Error : We can not allocate buffer for GUEST_REGS.\n");
		return FALSE;
	}
	
	RtlSecureZeroMemory((PVOID)g_pGuestState, sizeof(VIRTUAL_MACHINE_STATE));
	RtlSecureZeroMemory((PVOID)g_pCurrentGuestRigisters, sizeof(CURRENT_REGISTERS));
	RtlSecureZeroMemory((PVOID)g_GuestRegisters, sizeof(GUEST_REGS));

	DbgPrint("[*] g_pGuestState address = %llx.\n", (ULONG64)g_pGuestState);
	DbgPrint("[*] g_HostRegisters address = %llx.\n", (ULONG64)g_GuestRegisters);

	if(VmxInitVirtualization() == FALSE)
	{
		DbgPrint("[*] Error : Init Virtualization failed.\n");
		return FALSE;
	}


	DbgBreakPoint();
	
	AsmVmxSaveRegisters();


	// As we want to support more than 32 processor (64 logical-core) we let windows execute our routine for us
	// KeIpiGenericCall(VrDpcBroadcastInitializeGuest, 0x0);

	// VmxEnableVirtualization(pGuestState);
	// VmxTerminalVmxOff(g_pGuestState);

	// KeLowerIrql(g_PreviousIRQL);

	// DbgPrint("Helo Driver!\n");

	

	return STATUS_SUCCESS;
}