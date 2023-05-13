#pragma once
#include <ntddk.h>


EXTERN_C USHORT  AsmGetEs() ;
EXTERN_C USHORT  AsmGetCs() ;
EXTERN_C USHORT  AsmGetSs() ;
EXTERN_C USHORT  AsmGetDs();
EXTERN_C USHORT  AsmGetFs() ;
EXTERN_C USHORT  AsmGetGs();
EXTERN_C USHORT  AsmGetTr();
EXTERN_C USHORT AsmGetLdtr();
EXTERN_C ULONG64 AsmGetGdtBase();
EXTERN_C ULONG64 AsmGetIdtBase();
EXTERN_C USHORT AsmGetGdtLimit();
EXTERN_C USHORT AsmGetIdtLimit();

