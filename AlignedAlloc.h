#ifndef __ALIGNEDMALLOC_H_
#define __ALIGNEDMALLOC_H_

/*
   Does page-aligned allocations for machines that depend on
   DMA to maximize network performance
*/

void *AlignedAlloc(int size,int alignment,int offset);

#endif

