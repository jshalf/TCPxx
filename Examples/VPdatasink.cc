#include <stdio.h>
#include <string.h>
#include <RawUDP.hh>
#include <pthread.h>

extern "C" { // make it callable from a C program 
  void UDPReflectiveMemory(char *mem,int dims[3],int port);
}

void NetByteSwap(void *a,int nelements){
  int *src=(int *)a;
  register int i;
  for(i=0;i<nelements;i++)
    src[i]=ntohl(src[i]);
}

void FindRange(float *f,size_t nelem,float *range){
  int i;
  for(i=1,range[0]=f[0],range[1]=f[0];i<nelem;i++){
    if(range[0]>f[i]) range[0]=f[i];
    if(range[1]<f[i]) range[1]=f[i];
  }
}

/*--------------------------------------
  Mapping function converts a single data index into coordinate
  location within the dataset.
 ----------------------------------------*/
inline void IndexToXYZ(int index,int *dims,
		     int &x,int &y,int &z){
  z=index/(dims[0]*dims[1]);
  register int remainder=index%(dims[0]*dims[1]);
  y=remainder/dims[0];
  x=remainder%(dims[0]);
}

/*----------------------------------------------
  This is just a mapping function that maps indices from the 
  local domain (the domain-decomposed block of data) to a
  location within the slab (the non-domain-decomposed data).
 -----------------------------------------------*/
inline void LocalXYZToGlobalIndex(int x,int y,int z,
				  int *localdims,int *globaldims,int *origin,
				  int &globalindex){
  globalindex = z*globaldims[0]*globaldims[1] +
    (y+origin[1])*globaldims[0] +
    x + origin[0];
}
/*=============================================================
Now that you know where the contents of the message should be placed
within the slab: this copies the message contents (which are
necessarily a sub-block of the slab) into its proper place in the
non-domain-decomposed slab).

Argh... Need to fix this so that it byte-swaps the data...
Its in the comment, but needs to be done more elegantly.
Really, the entire packet should be byte-order converted
in one shot rather than doing it piecemeal...

Its possible that we won't byteswap the payload on the
sending side in order to save us a buffer copy.  Will have
to discuss this later...
===============================================================*/

void CopyDataToMem(void *buffer,int bufsize,int localindex,
		     int *origin,int *localdims,
		     void *mem, int globalindex, int *globaldims){
  int i=0;
  float *src=(float*)buffer,*dst=(float*)mem;
  bufsize/=sizeof(float);
  while(i<bufsize){
    int x,y,z,lx,ly,lz;
    int seglen=localdims[0]-localindex%localdims[0]; 
    /* thats the largest contiguous segment */
    if(seglen+i>bufsize) seglen=(bufsize-i);
    IndexToXYZ(globalindex,globaldims,x,y,z);
    IndexToXYZ(localindex,localdims,lx,ly,lz);
    fprintf(stderr,"this strides glob[%u] %u:%u:%u for %u elements local[%u] %u:%u:%u v:%f\n",
	    globalindex,x,y,z,seglen,localindex,lx,ly,lz,src[i]);
    memcpy(dst+globalindex,src+i,seglen*sizeof(float));
    NetByteSwap(dst+globalindex,seglen*sizeof(float));
    /* ugh... gotta byteswap from network order */
    // for(float *bs=dst+globalindex;dst<dst+globalindex+seglen;dst++)
    //  *dst=ntohl((*dst));
    /* now increment the counts for each */
    /* stride through the global index space */
    globalindex += seglen;
    /* then increment to the next segment */
    globalindex += (globaldims[0]-localdims[0]); /* increment to next seg */
    if(ly==(localdims[1]-1)) 
      globalindex+=(globaldims[0]*globaldims[1]-localdims[1]*globaldims[0]);
    /* next seg in localindex is contiguous */
    localindex += seglen;
    i+=seglen;
  }
}

/*==================================================
  This just extracts info from the first 5*sizeof(int) bytes of the
  message.  The header contains (in order)
      32bit int: origin[0]
      32bit int: origin[1]
      32bit int: localdims[0]
      32bit int: localdims[1]
      32bit int: localindex
  Where these things are defined as
      localdims[]: The size of the x and y dimensions of the 
        local/domain-decomposed block of data.  This is the
	 processor-local block of data that needs to get copied
	 into the slab.
      origin[]: The location in x and y dimensions of the 
        domain-decomposed block of data with respect to the
	 slab's i,j,k indices.  So this places this block of
	 memory within the overall slab.
      localindex: This is the index within the local block
        of data that this segment starts at.  It is assumed to
	 be a contiguous copy of data out of that domain-decomposed
	 block of data, so the packetlength tells you implictly
	 what the end-index is (this is just the start).
  The above indices neglect Z because it is implicitly with
  respect to the coordinates of the slab (ie. it never changes).
  All values are in network byte order (big-endian)
  ===================================================*/
void ExtractHeader(char *message, int *globaldims,
		   int *origin,int *localdims, int &localindex,int &globalindex){
  int buffer[5],i;
  memcpy(buffer,message,sizeof(int)*5); /* fix memory alignment */
  if(sizeof(long)<8){
    origin[0]=ntohl(buffer[0]); origin[1]=ntohl(buffer[1]);
    localdims[0]=ntohl(buffer[2]); localdims[1]=ntohl(buffer[3]);
    localindex=ntohl(buffer[4]);
  }
  else { // kludge for T3E and SGI 
    // (wouldn't need to convert byte order anyways)
    origin[0]=buffer[0]; origin[1]=buffer[1];
    localdims[0]=buffer[2]; localdims[1]=buffer[3];
    localindex=buffer[4];
  }
  int x,y,z; 
  // take the index and convert to i,j,k location in 
  // the local block of memory
  IndexToXYZ(localindex,localdims,x,y,z);
  // convert the i,j,k (xyz) location in local-domain and
  // convert to the index in the global memory
  LocalXYZToGlobalIndex(x,y,z,
			localdims,globaldims,origin,
			globalindex);
  // just as a sanity check, lets see what the xyz location
  // is within the global domain.
  IndexToXYZ(globalindex,globaldims,x,y,z);
}

/*=======================================
  These are the operations performed on
  each received message.  ExtractHeader
  reads the description of the message from
  the first 5 words and the CopyToMem uses
  the info extracted from the header to 
  copy the data into the correct part
  of the memory slab.
  ========================================*/
void ProcessMessage(char *message,int msgsize,float *mem,int dims[3]){
  /* now we extract the message header */
  int origin[2],localdims[2],localindex;
  int globalindex;
  ExtractHeader(message,dims,origin,localdims,localindex,globalindex);
  fprintf(stderr,"ProcessMsg origin[%2u][%2u] localdims[%2u][%2u] idx[%2u:%2u]\n",
	  origin[0],origin[1],
	  localdims[0],localdims[1],
	  localindex,globalindex);
  CopyDataToMem(message+5*sizeof(int),msgsize-5*sizeof(int),localindex,
		origin,localdims,mem,globalindex,dims);
}


/*============================================
  Just read from the UDP socket and copy the 
  received messages into the proper place in the
  slab.
  ==============================================*/
void UDPReflectiveMemory(float *mem,int dims[3],int port){
  char buffer[1500],name[128];
  RawUDPserver udp(port);
  IPaddress returnaddr;

  while(1){ /* stupid-mode (loop ad-nauseum) */
    int sz = udp.recv(buffer,sizeof(buffer),returnaddr);
    returnaddr.getHostname(name,127);
    fprintf(stderr,"host[%s] ",name);
    ProcessMessage(buffer,sz,mem,dims);
  }
}

/*
  Setup a rudimentary main for testing purposes.
 */


void *DataSink(void *arg){
  int *port=(int*)arg;
  RawUDPserver udp(((int*)arg)[0]);
  char buffer[1500];
  int *header=(int*)buffer;
  float *data=(float*)(buffer+sizeof(int)*5);
  fprintf(stderr,"Datasink starts port %u\n",*port);
  while(1){
    float range[2];
    char name[128];
    IPaddress returnaddr;
    int nelem;
    int i;
    int sz=udp.recv(buffer,sizeof(buffer),returnaddr);
    if(sz>0){
     returnaddr.getHostname(name,127);
     fprintf(stderr,"host[%s] ",name);
    }
    nelem = sz/4-5;
    // for(i=0;i<nelem;i++) data[i]=ntohl(data[i]);
    NetByteSwap(buffer,sz/4);    
    if(header[0]<0){ /* this is a protocol2 message */
      if(header[0]==-1){ /* its a data message */
	sz--; /* one less byte with header */
	FindRange(data+1,nelem,range);
	fprintf(stderr,"Port[%4u] range=%f:%f\n",*port,range[0],range[1]);
	// watch for out-of-range data
	if(range[0]<-10.0 || range[1]>10.0){
	  // decode the header
	  fprintf(stderr,"====Port[%4u] : Header %3d:%3u:%3u:%3u Range[%f:%f]\n",
		  *port,header[0],header[1],header[2],header[3],
		  range[0],range[1]);
	}
      }
      else if(header[0]==-2){ /* info message */
	fprintf(stderr,"\nInfo p{%u of %u} gd{%u:%u:%u} o{%u:%u:%u} ld{%u:%u:%u}\n",
		header[1],header[2],header[3],header[4],header[5],header[6],
		header[7],header[8],header[9],header[10],header[11]);
	fprintf(stderr,"\tnghost{%u:%u:%u} slabnum %u\n",
		header[12],header[13],header[14],header[15]);
      }
    }
    else { /* protocol version 1 */
      FindRange(data,nelem,range);
      fprintf(stderr,"Port[%4u] range=%f:%f\n",*port,range[0],range[1]);
      // watch for out-of-range data
      if(range[0]<-10.0 || range[1]>10.0){
	// decode the header
	fprintf(stderr,"====Port[%4u] : Header %3d:%3u:%3u:%3u Range[%f:%f]\n",
		*port,header[0],header[1],header[2],header[3],
		range[0],range[1]);
      }
    }
  }
}

int main(int argc,char *argv[]){
  int port,maxport;
  int i;
  int *portlist;
  pthread_t *thread;
  if(argc<3) {
    fprintf(stderr,"Usage: %s <portnumber> <maxportnumber>\n",argv[0]);
    exit(0);
  }
  else {
    port=atoi(argv[1]);
    maxport=atoi(argv[2]);
  }
  portlist = new int[maxport-port];
  thread = new pthread_t[maxport-port];
  for(i=0;i<=(maxport-port);i++){
    portlist[i]=port+i;
    pthread_create(&(thread[i]),NULL,DataSink,(void*)(&(portlist[i])));
  }
  for(i=0;i<=(maxport-port);i++)
    pthread_join(thread[i],NULL); // wait for completion
}

