#include "AlignedAlloc.hh"
#include <stdio.h>
#include <stdlib.h>


void *AlignedAlloc(int buflen,int bufalign,int bufoffset){
  char *buf;
  if( (buf = (char *)malloc(buflen+bufalign)) == (char *)NULL)
	perror("malloc");
  if(bufalign != 0)
	buf +=(bufalign - ((long)buf % bufalign) + bufoffset) % bufalign;
  return (void*)buf;
}

