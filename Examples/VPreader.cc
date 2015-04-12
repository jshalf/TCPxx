#include <stdio.h>
#include <string.h>
#include <RawUDP.hh>

extern "C" { // make it callable from a C program 
  void UDPReflectiveMemory(char *mem,int dims[3],int port);
}
void NetByteSwap(void *a,int nelements){
  long *src=(long *)a;
  register int i;
  if(sizeof(long)!=8) /* watch out for those tricky SGI systems */
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
    memcpy(dst+globalindex,src+i,seglen*sizeof(float));
    NetByteSwap(dst+globalindex,seglen*sizeof(float));
    fprintf(stderr,"this strides glob[%u] %u:%u:%u for %u elements local[%u] %u:%u:%u v:%f\n",
	    globalindex,x,y,z,seglen,localindex,lx,ly,lz,dst[globalindex]);
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
void ExtractDataHeader(char *message, int *globaldims,
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

void ExtractInfoHeader(void *message,int *proc,int *nprocs,
		       int *globaldims,int *localorigin,int *localdims,
		       int *nghost,int *slicenum){
  int *header = (int*)message;
  if(sizeof(long)==8){
    proc[0]=header[0];
    nprocs[0]=header[1];
    globaldims[0]=header[2];
    globaldims[1]=header[3];
    globaldims[2]=header[4];
    localorigin[0]=header[5];
    localorigin[1]=header[6];
    localorigin[2]=header[7];
    localdims[0]=header[8];
    localdims[1]=header[9];
    localdims[2]=header[10];
    nghost[0]=header[11];
    nghost[1]=header[12];
    nghost[2]=header[13];
    slicenum[0]=header[14];
  }
  else {
    // printf("Extract from message msg=%x header=%x\n",
    //	   message,header);
    proc[0]=ntohl(header[0]);
    nprocs[0]=ntohl(header[1]);
    globaldims[0]=ntohl(header[2]);
    globaldims[1]=ntohl(header[3]);
    globaldims[2]=ntohl(header[4]);
    localorigin[0]=ntohl(header[5]);
    localorigin[1]=ntohl(header[6]);
    localorigin[2]=ntohl(header[7]);
    localdims[0]=ntohl(header[8]);
    localdims[1]=ntohl(header[9]);
    localdims[2]=ntohl(header[10]);
    nghost[0]=ntohl(header[11]);
    nghost[1]=ntohl(header[12]);
    nghost[2]=ntohl(header[13]);
    slicenum[0]=ntohl(header[14]);
  }
}

void PrintInfoHeader(int proc,int nprocs,
		       int *globaldims,int *localorigin,int *localdims,
		     int *nghost,int slicenum){
  fprintf(stderr,"Proc[%u of %u]: gdims{%u:%u:%u} ldims{%u:%u:%u} origin{%u:%u:%u}\n\tghost{%u:%u:%u} slice[%u]\n",
	 proc,nprocs,globaldims[0],globaldims[1],globaldims[2],
	 localdims[0],localdims[1],localdims[2],
	 localorigin[0],localorigin[1],localorigin[2],
	 nghost[0],nghost[1],nghost[2],slicenum);
}

void PrintHeader(void *head){
  int *h = (int*)head;
  printf("Raw Header = %d %d %d %d %d\n",
	 ntohl(h[0]),
	 ntohl(h[1]),
	 ntohl(h[2]),
	 ntohl(h[3]),
	 ntohl(h[4]));
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
  int *head = (int*)message;
  int msgtype;
  if(sizeof(long)==8) msgtype=head[0];
  else msgtype=ntohl(head[0]);
  //PrintHeader(message);
  if(msgtype==-1){ /* protocol 2 data message */
    // puts("Protocol 2 data message");
    ExtractDataHeader(message+sizeof(int),dims,origin,localdims,localindex,globalindex);
    fprintf(stderr,"ProcessMsg origin[%2u][%2u] localdims[%2u][%2u] idx[%2u:%2u]\n",
	    origin[0],origin[1],
    	    localdims[0],localdims[1],
    	    localindex,globalindex);
    CopyDataToMem(message+6*sizeof(int),msgsize-6*sizeof(int),localindex,
		  origin,localdims,mem,globalindex,dims);
  }
  else if(msgtype==-2){ /* protocol 2 info message */
    int gdims[3],nghost[3],slicenum,proc,nprocs;
    int lorigin[3],ldims[3];
    //puts("Protocol 2 Info Message!");
    ExtractInfoHeader(message+4,&proc,&nprocs,
		      gdims,lorigin,ldims,
		      nghost,&slicenum);
    //printf("proc is %u nproc=%u msgtype=%d\n",proc,nprocs,msgtype);
    PrintInfoHeader(proc,nprocs,
		    gdims,lorigin,ldims,
		    nghost,slicenum);
    /* no data to extract... the rest is junk */
    //fprintf(stderr,"Buf 0x%X\n",message);
  }
  else if(msgtype>=0){
    /* protocol 1 data message */
    //puts("Protocol 1 data message");
    ExtractDataHeader(message,dims,origin,localdims,localindex,globalindex);
    fprintf(stderr,"ProcessMsg origin[%2u][%2u] localdims[%2u][%2u] idx[%2u:%2u]\n",
	    origin[0],origin[1],
	    localdims[0],localdims[1],
	    localindex,globalindex);
    CopyDataToMem(message+5*sizeof(int),msgsize-5*sizeof(int),localindex,
		  origin,localdims,mem,globalindex,dims);
  }
}


/*============================================
  Just read from the UDP socket and copy the 
  received messages into the proper place in the
  slab.
  ==============================================*/
void UDPReflectiveMemory(float *mem,int dims[3],int port){
  char buffer[1500];
  RawUDPserver udp(port);

  while(1){ /* stupid-mode (loop ad-nauseum) */
    //    fprintf(stderr,"startloop buf=0x%X\n",buffer);
    int sz = udp.recv(buffer,sizeof(buffer));
    // fprintf(stderr,"message size is %u\n",sz);
    ProcessMessage(buffer,sz,mem,dims);
  }
}

/*
  Setup a rudimentary main for testing purposes.
 */
int main(int argc,char *argv[]){
  int port;
  if(argc<2) {
    fprintf(stderr,"Usage: %s <portnumber>\n",argv[0]);
    exit(0);
  }
  else port=atoi(argv[1]);
  /* Hardcoded to overall domain size (made to match VPtester */
  int dims[3]={4*16,4*16,1*8};
  float *mem=new float[dims[0]*dims[1]*dims[2]];
  for(int i=0;i<dims[0]*dims[1]*dims[2];i++) mem[i]=-1.0; // init memory
  UDPReflectiveMemory(mem,dims,port);
}
