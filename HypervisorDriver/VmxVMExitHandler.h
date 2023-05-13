#pragma once
#include <ntddk.h>
#include "Common.h"

BOOLEAN VehVmExitHandler(PGUEST_REGS GuestRegs);

/* Handle Cpuid Vmexits*/
VOID VehHandleCpuid(PGUEST_REGS RegistersState);

/* Handler GETSEC instruction */
VOID VehHanldeGETSEC(PGUEST_REGS RegistersState);

/* Handler XSETBV instruction */
VOID VehHanldeXSETBV(PGUEST_REGS RegistersState);

/* Handler INVEPT instruction */
VOID VehHanldeINVEPT(PGUEST_REGS RegistersState);

/* Handler RDTSC instruction */
VOID VehHanldeRDTSC(PGUEST_REGS RegistersState);

/* Handler RDTSCP instruction */
VOID VehHanldeRDTSCP(PGUEST_REGS RegistersState);

VOID VehRaiseIrql();

VOID VehLowerIrql();

BOOLEAN VehAdjustGuestRip();
