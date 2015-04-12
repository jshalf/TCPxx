#include <stdio.h>
#include <stdlib.h>
#include "RawUDP.hh"
#include "Delay.hh"

/* inefficient due to unnecessary copying,
   but its damned simple.

   Message format (all big-endian)
   
   32bits int : origin[0] (of local domain with respect to global domain)
   32bits int : origin[1]
   32bits int : localimds[0] (dims of the processor-local domain)
   32bits int : localdims[1]
   32bits int : localindex (index of where in the local domain the message payload was copied from.)
   32bit float[<msgsize>] : The message payload (32bit float)

   The header is converted to big-endian, but in this example,
   the payload isn't converted (gotta fix that).
   It *probably* will be network byte order, but there are
   some efficiency reasons for not converting the byteorder on
   the payload though (ie. eliminating a buffer copy to improve
   performance).

*/
#define WDS_BIG_ENDIAN    0x01
#define WDS_LITTLE_ENDIAN 0x02
static int SenseByteOrder(){
  typedef union byteorder {
    int  i;
    char c[4];
  } byteorder;
  byteorder b;
  b.i=0x01020304;
  if(b.c[0]==0x01) return WDS_BIG_ENDIAN;
  return WDS_LITTLE_ENDIAN;
}
void NetByteSwap(void *b,void *a,int nelements){
  long *src=(long *)a;
  long *dst=(long *)b;
  register int i;
  if(SenseByteOrder() == WDS_LITTLE_ENDIAN)
    for(i=0;i<nelements;i++)
      dst[i]=ntohl(src[i]);
  else
    memcpy(dst,src,nelements*4);
}

void SendBlock(RawUDP &udp,float *mem,
	       int *localdims,int *origin,int *globaldims){
  /* just walk through this block of mem
     dna send it out over that UDP socket */
  int buffer[1500]; // 4x oversized...
  Delay delay(0.01); // 0.01 second delay per packet (sloooowww)
  char *cmem=(char*)((void*)mem);
  /* Encodeing the origin[],localdims[] into
     the first 4 words of the message in big-endian format.
     The 5th word (the localindex) will be encoded in the
     interior of the loop that copies in the message payload */
  buffer[0]=htonl(origin[0]);
  buffer[1]=htonl(origin[1]);
  buffer[2]=htonl(localdims[0]);
  buffer[3]=htonl(localdims[1]);
  int pktsize = 1468;
  // blksize is for datablock
  int blksize = pktsize-5*sizeof(int);
  for(long i=0;i<localdims[0]*localdims[1]*localdims[2]*sizeof(float);){
    buffer[4]=htonl(i/4); // the 5th word of the header (the localindex)
    if((i+blksize)>(localdims[0]*localdims[1]*localdims[2]*sizeof(float))){
      /* truncate blk */
      blksize=localdims[0]*localdims[1]*localdims[2]*sizeof(float)-i;
      //printf("truncated blksize=%u\n",blksize);
      // but now what do we do with the truncated data???
    }
    NetByteSwap(buffer+5,cmem+i,blksize);
    udp.write(buffer,blksize+5*sizeof(int)); // send MTU worth of crap + header
    delay.instantaneousWait(); // slow it way down with 0.01 sec wait
    i += blksize;
  }
}
void SendInfoP2(RawUDP &udp,
		int proc,int numprocs,int *globaldims,
		int *localorigin,
		int *localdims,int *nghost,int slabnum){
  int header[1500]; /* send 1469 bytes of mostly bogus info to carry header payload */
  int packetsize = 1496;
  int r;
  if(sizeof(long)==8){
    header[0]=-2; /* info payload type */
    header[1]=proc; /* my processor rank: numbers 0 to n-1 procs */
    header[2]=numprocs; /* the total number of processors involved in this computation */
    header[3]=globaldims[0]; /* global dimensions of the dataset (not accounting for */
    header[4]=globaldims[1]; /* ghost cells in each slab */
    header[5]=globaldims[2];
    header[6]=localorigin[0];    /* localorigin of the local block of data with respect to */
    header[7]=localorigin[1];    /* the global dimensions of the dataset */
    header[8]=localorigin[2];
    header[9]=localdims[0]; /* dimensions of the local block of data */
    header[10]=localdims[1];
    header[11]=localdims[2];
    header[12]=nghost[0]; /* nghost cells in x-direction */
    header[13]=nghost[1]; /* nghost for Y  */
    header[14]=nghost[2]; /* nghost for Z  */
    header[15]=slabnum; /* slab number: numbered from 0 to nslabs-1 */
  }
  else {
    header[0]=htonl(-2);
    header[1]=htonl(proc);
    // printf("proc=%u\n",proc);
    header[2]=htonl(numprocs);
    // printf("nprocs=%u\n",numprocs);
    header[3]=htonl(globaldims[0]);
    header[4]=htonl(globaldims[1]);
    header[5]=htonl(globaldims[2]);
    header[6]=htonl(localorigin[0]);
    header[7]=htonl(localorigin[1]);
    header[8]=htonl(localorigin[2]);
    header[9]=htonl(localdims[0]);
    header[10]=htonl(localdims[1]);
    header[11]=htonl(localdims[2]);
    header[12]=htonl(nghost[0]);
    header[13]=htonl(nghost[1]);
    header[14]=htonl(nghost[2]);
    header[15]=htonl(slabnum);
  }
  r=udp.write(header,packetsize);  /* rest of packet is garbage data */
  if(r<0) perror("packeterrror");
}

void SendBlockP2(RawUDP &udp,float *mem,
	       int *localdims,int *origin,int *globaldims){
  /* just walk through this block of mem
     dna send it out over that UDP socket */
  int buffer[1500]; // 4x oversized...
  Delay delay(0.01); // 0.01 second delay per packet (sloooowww)
  char *cmem=(char*)((void*)mem);
  /* Encodeing the origin[],localdims[] into
     the first 4 words of the message in big-endian format.
     The 5th word (the localindex) will be encoded in the
     interior of the loop that copies in the message payload */
  buffer[0]=-1;
  buffer[1]=htonl(origin[0]);
  buffer[2]=htonl(origin[1]);
  buffer[3]=htonl(localdims[0]);
  buffer[4]=htonl(localdims[1]);
  int pktsize = 1468;
  // blksize is for datablock
  int blksize = pktsize-6*sizeof(int);
  for(long i=0;i<localdims[0]*localdims[1]*localdims[2]*sizeof(float);){
    buffer[5]=htonl(i/4); // the 5th word of the header (the localindex)
    if((i+blksize)>(localdims[0]*localdims[1]*localdims[2]*sizeof(float))){
      /* truncate blk */
      blksize=localdims[0]*localdims[1]*localdims[2]*sizeof(float)-i;
      //printf("truncated blksize=%u\n",blksize);
      // but now what do we do with the truncated data???
    }
    NetByteSwap(buffer+6,cmem+i,blksize);
    udp.write(buffer,blksize+6*sizeof(int)); // send MTU worth of crap + header
    delay.instantaneousWait(); // slow it way down with 0.01 sec wait
    i += blksize;
  }
}

int main(int argc,char *argv[]){
  /* OK, I'm going to hardcode the domain decomposition 
     initially here...  We are going to do 4x4 domains
     that form a 64x64x8 slab.  Next test will try non-power-of
     2 dims */
  int nx=16,ny=16,nz=8;
  int protocol=1;
  if(argc<3){
    fprintf(stderr,"Usage: %s <hostname> <portnum>",argv[0]);
    exit(0);
  }
  if(argc==4) protocol=atoi(argv[3]);

  typedef float *floatP;
  floatP domains[4][4];
  int localdims[3]={nx,ny,nz};
  int nghost[3]={0,0,0};
  int globaldims[3]={4*nx,4*ny,nz};
  int origin[3]={0,0,0};
  int blocksize=1468;
  for(int i=0;i<4;i++){
    for(int j=0;j<4;j++){
      domains[i][j]=new float[nx*ny*nz];
      /* now number the domain by its position in the slab */
      for(int n=0;n<nx*ny*nz;n++) 
	(domains[i][j])[n]=(float)(i+j*4);
    }
  }
  /* OK, now we blast things out from each domain */
  RawUDPclient udp(argv[1],atoi(argv[2])); /* open UDP socket */
  /* RawTCPclient tcp(argv[1],atoi(argv[2]));  open TCP socket */
  /* now start blasting */
  /* stride through each domain */
  // while(1) // do this forever...
  for(int i=0;i<4;i++){
    for(int j=0;j<4;j++){
      origin[0]=i*localdims[0];
      origin[1]=j*localdims[1]; 
      if(protocol==2){
	SendInfoP2(udp,
		   0, /*processor rank */
		   1, /* number of processors */
		   globaldims,
		   origin,localdims,nghost,
		   0/* slabnumber */);
	SendBlockP2(udp,domains[i][j],localdims,origin,globaldims);
      }
      else if(protocol==3){ // use TCP
	/* SendBlockTCP(tcp,domains[i][j],localdims,origin,globaldims); */
      }
      else
	SendBlock(udp,domains[i][j],localdims,origin,globaldims);
      fprintf(stderr,"Domain[%2u][%2u]\r",i,j);
    }
  }
}
