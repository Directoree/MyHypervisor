#pragma once


#define PAGE_SIZE 0x1000
#define VMM_STACK_SIZE      0x8000

BOOLEAN VmxAllocateVmxonRegion();

BOOLEAN VmxAllocateVmcsRegion();


/* Allocate a buffer forr Msr Bitmap */
BOOLEAN VmxAllocateMsrBitmap();

/* Allocate a buffer for Host(VMM Stack) */
BOOLEAN VmxAllocateVmmStack();