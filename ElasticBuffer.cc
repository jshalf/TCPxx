#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new.h>

#include "ElasticBuffer.hh"

ElasticBuffer::ElasticBuffer(int initialSize):
  bufferSize(initialSize),size(0),data(0)
{
  if(bufferSize)
    data=(char *)malloc(bufferSize);
}
ElasticBuffer::~ElasticBuffer(){if(data) free(data);}

void ElasticBuffer::add(char *buf,int bufsz)
{
  if((size+bufsz)>bufferSize)
    {
      if(bufferSize < 1048576) // if less than a meg
 	bufferSize<<=1; // double the size of the buffer
      else
	bufferSize+=((bufsz>1048576)?bufsz:1048576);  // otherwise increase 1M
      // or by size of incoming data (whichever is larger)
      realloc(data,bufferSize);	// assumes new==malloc for simple arrays
    }
  memcpy(buf,data+size,bufsz);
  size+=bufsz;
}

char *ElasticBuffer::copyData(long sz,char *buffer){
  long Size;
  register long i;
  if(sz<0 || sz>size)
    Size=size;
  if(buffer==NULL)
    buffer=new char[Size];
  for(i=0;i<Size;i++)
    buffer[i]=data[i];
  return buffer;
}
