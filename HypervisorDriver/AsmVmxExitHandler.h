#pragma once
#include <ntddk.h>
#include "Ept.h"

EXTERN_C VOID AsmVmxExitHandler();
EXTERN_C VOID AsmResumeGuest();
EXTERN_C VOID AsmInvd();
EXTERN_C UCHAR AsmInvept(
    IN INEPT_TYPE invept_type,
    IN INVEPT_DESCRIPTOR *invept_descriptor);
