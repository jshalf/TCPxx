#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>

#ifdef USE_MPI
#include <mpi.h>
#endif
#include "ProtocolV3.hh"

/*=========================================

This is the autonegotiation part of the ProtocolV3.
Procedure to communicate to Cactus is
1) open a TCP socket to a cactus PE (host:port)
2) use PV3_ReadCactusMessage(int socketFD)
   to read the cactus message.  This will create a
   CactusInfo datastructure that contains all of the 
   info about the Cactus data payload.  If you have
   defined USE_MPI, then the datastructure will be on 
   all of the processors of the visapult backEnd.
   It is intended that MustRead() use your own 
   TCP sockets read routine, but you can certainly
   use this one for an open socket file descriptor.
3) the BackEnd must now reply to Cactus to inform
   each PE where to send its data. So you must fill
   out an array of HostList datastructures (the number
   of HostList datastructs == number of Cactus PE's).
   Use PV3_SizeOfMsgToCactus(nprocs_cactus) to compute
   the size of the message buffer to send to cactus.
4) Malloc that message buffer and send a message back
   to cactus using
   PV3_GenMsgToCactus()
5) Cactus should now start sending UDP packets to Visapult
   as specified.  Currently we will send PV2 packets, but
   we can switch easily to a PV3 packet format if you would
   like.  But the autonegotiation is done at this point.

*/

/*=================================================================*/
/*                     Visapult Parsing                            */
/*  Aquiring messages from Cactus and formulating a response       */
/*=================================================================*/
 /*  Cactus Message format (all ASCII)
	 msg[0-15] = number of processors used by cactus
	 msg[16]= number of bytes per element (4 bytes for float)
	 msg[17]= endianness of the words ('b' big 'l' little endian)
	 msg[18+145] (128 bytes) = global dims(gn<N>) 
	             and processor arrangement (pn<N>)
	             formatted as "gnx,gny,gnz:pnx,pny,pnz"
	 for each proc
	   msg[146+<procnm>*128] = local dims (n<N>) and origin (o<N>)
	             formatted as nx,ny,nz:ox,oy,oz
	 end of message!
    */

static int PV3_ParseProcInfo(char *msg,int *origin,int *dims){
  /* 
     using conservative strchr() parsing because strtok is being
     obsoleted on some systems but not others.
     sscanf too flakey
  */
  char *c;
  int i;
  for(i=0;i<2;i++){
    c=strchr(msg,',');
    if(!c) { perror("Parse Procinfo Failed"); return -1;}
    *c='\0';
    origin[i]=atoi(msg);
    msg=c+1;
  }
  c=strchr(msg,':');
  if(!c) return -1;
  *c='\0';
  origin[2]=atoi(msg);
  msg=c+1;
  for(i=0;i<2;i++){
    c=strchr(msg,',');
    if(!c) { perror("Parse Procinfo Failed"); return -1;}
    *c='\0';
    dims[i]=atoi(msg);
    msg=c+1;
  }
  dims[2]=atoi(msg);
  return 128;
}
/* The cactus header is always 
   msg[0-15]=numprocs
   msg[16]= <num bytes per data element '4' or '8'  
               (currently will always be 4)
   msg[17]='b' for big-endian or 'l' for little-endian
   msg[18-145] global dims for the entire domain followed by
               the processor topology
	       format is %u,%u,%u:%u,%u,%u
*/
int PV3_ParseCactusHeader(char *header,
			    int *nprocs,int *datasize,int *big_endian,
			    int *globaldims,int *p_topology){
  char *msg=header+18;
  char *c;
  int i;
  /* printf("Parse Header [%s][%s]\n",header,header+16);*/
  header[15]='\0';
  *nprocs = atoi(header);
  *datasize = (int)(header[17]-'0');
  if(header[16]=='b' || header[16]=='B') *big_endian=1;
  else if(header[16]=='l' || header[16]=='L') *big_endian=0;
  else { *big_endian=-1; perror("failed to pars endian"); return 0;}  /* error */

  for(i=0;i<2;i++){
    c=strchr(msg,',');
    if(!c) { perror("Parse CactusHeader Failed"); return 0;}
    *c='\0';
    globaldims[i]=atoi(msg);
    msg=c+1;
  }
  c=strchr(msg,':');
  if(!c) return 0;
  *c='\0';
  globaldims[2]=atoi(msg);
  msg=c+1;
  for(i=0;i<2;i++){
    c=strchr(msg,',');
    if(!c) { perror("Parse CactusHeader Failed"); return 0;}
    *c='\0';
    p_topology[i]=atoi(msg);
    msg=c+1;
  }
  p_topology[2]=atoi(msg);
  /*printf("topo %u globaldims[%u,%u,%u] nprocs=%u\n",
    p_topology[0],globaldims[0],globaldims[1],globaldims[2],*nprocs);*/
  return (*nprocs)*128; /* computed length for the body of the message */
}

/*
  The body of the cactus message is <nprocs> elements
  Each element is 128 bytes and contains the following info
    origin: origin of this processor's local domain of data
    dims: the dimensions of this local domain
  The format of this message is
     <ox>,<oy>,<oz> : <nx>,<ny>,<nz>
  
 */
void PV3_ParseCactusBody(int nprocs,char *msg_body,
			   ProcInfo *pinfo){
  int i;
  for(i=0;i<nprocs;i++){
    PV3_ParseProcInfo(msg_body+128*i,pinfo[i].origin,pinfo[i].dims);
  }
}

/* Sample of a reader that will read the buf len specified.
   Only fails if the socket is dead
*/
int MustRead(int fd,char *buffer,int buflen){
  /* do errno only if types.h has been included */
    /* how can I tell if this has been included ? */
    register int n,accum=0;
    while((n=read(fd,buffer,buflen)) > 0){
      if(n>=0) {
	buffer = buffer + n;
	buflen -= n;
	accum+=n;
      }
    }
#ifdef PERRORS
    if(n<0) switch(errno) /* note use of global variable errno */
      {
      case EBADF:
#ifdef PERRORS
	perror("invalid file descriptor");
#endif
	break;
      case EFAULT:
#ifdef PERRORS
	perror("invalid buffer address");
#endif
	break;
      case EINTR:
#ifdef PERRORS
	perror("operation interrupted by a signal");
	perror("disable signals and try again");
#endif
	break;
      case EWOULDBLOCK:
#ifdef PERRORS
	perror("Non-blocking I/O is specified by ioctl");
	perror("but socket has no data (would have blocked).");
#endif
	break;
      }
#endif
    if(n<0) 
      return n;
    else
      return accum;
}

/* we assume that the master process for this transaction is
   processor 0 */
CactusInfo *PV3_ReadCactusMessage(int sock){
  CactusInfo *info = (CactusInfo*)malloc(sizeof(CactusInfo));
  int bodysize;
  int mypid=0,r;
  char header[146],*body;
#ifdef USE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD,&mypid);
#endif
  if(mypid==0) /* only the one with a TCP socket reads */
    r=MustRead(sock,header,146); /* a dummmy routine */
  printf("MustRead Returned %u\n",r);
#ifdef USE_MPI
  /* must broadcast the buffer to everyone in my COMM_WORLD */
  MPI_Bcast(header,146,MPI_CHAR,0,MPI_COMM_WORLD);
#endif
  /* and then parse the buffer *after* it has been broadcast */
  bodysize=PV3_ParseCactusHeader(header,&(info->nprocs),
				 &(info->elementsize),
				 &(info->bigendian),
				 info->globaldims,info->proc_topology);
  body=(char*)malloc(bodysize);
  if(mypid==0)
    MustRead(sock,body,bodysize);
#ifdef USE_MPI
  MPI_Bcast(body,bodysize,MPI_CHAR,0,MPI_COMM_WORLD);
#endif
  info->pinfo=(ProcInfo*)malloc(sizeof(ProcInfo)*info->nprocs);
  PV3_ParseCactusBody(info->nprocs,body,info->pinfo);
  free(body); /* release the buffer for the message body */
  return info;
}

/*==================================================================*/
/*                     Visapult Message Gen                         */
/*  Cactus Generates a Message to send to Visapult                  */
/*==================================================================*/

/*
  Must collect the list of hosts to send to Cactus
  and then do a round-robin assignment of the host mapping.
 */
void PV3_AssignHostsRoundRobin(char *localhostlist,int nlocalprocs,
			   char *cactushostlist,int ncactusprocs){
  int i;
  if(localhostlist==cactushostlist){
    // skip the first nlocalprocs entries in the list;
    ncactusprocs-=nlocalprocs;
    cactushostlist+=128*nlocalprocs;
  }
  while(ncactusprocs>0){
    /* index into localhostlist and strncpy to cactushostlist */
    memcpy(cactushostlist,localhostlist+128*(i%nlocalprocs),128);
    i++;
    cactushostlist+=128;
    ncactusprocs--;
  }
}


/* must know how large the message to cactus is going to be
   so that we can allocate an appropriately sized buffer
   and send the correct buffer size */
size_t PV3_SizeOfMsgToCactus(int nprocs_cactus){
  return 128*nprocs_cactus;
}
/* 
   Take a host list datastructure and write out a buffer
   to send to Cactus.  The hostlist must provide the host:port
   mapping that tells each Cactus PE where to send its UDP data.

   The buffer "message" must be pre-allocated
*/
void PV3_GenMsgToCactus(int nprocs_cactus,HostList *hostlist,
			char *message){
  int i;
  for(i=0;i<nprocs_cactus;i++){
    sprintf(message,"%s:%u",
	    hostlist[i].host,hostlist[i].port);
  }
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*==================================================================*/
/*                     Cactus Parsing                               */
/*  Aquiring messages from Visapul to determine how to connect back */
/*==================================================================*/

/* Visapult Message Format (all ASCII)
   msg[128*<procnum>] = hostname:portnum
   
   The length of the message is == the number of cactus processes.
   (not the number of visapult processes)
*/
static void PV3_ParseHostPort(char *msg,char *host,int *port){
  char *s=strchr(msg,':');
  *s='\0';
  *port=atoi(s+1);
  strcpy(host,msg);
}

/* after getting the entire message, we can extract from it our 
   local host and port.  So each proc gets its own unique host:port
   pair to send its message to. */
void PV3_ProcessVisapultMessage(int master,char *message, /* inputs */
				char *host,int *port){ /* outputs */
  char mymessage[128]; /* the local message */
  host=(char*)malloc(128);
#ifdef USE_MPI
  MPI_Scatter(mymessage, 128, MPI_CHAR, message, 128,
	      MPI_CHAR,master,MPI_COMM_WORLD);
#endif
  PV3_ParseHostPort(mymessage,host,port);
}


/*==================================================================*/
/*                     Cactus Message Gen                           */
/*  Cactus Generates a Message to send to Visapult                  */
/*==================================================================*/

/* Must pre-allocate a buffer */
size_t PV3_SizeOfMsgToVisapult(int nprocs){
  return (size_t)(16+2+128+128*nprocs);
}
size_t PV3_SizeOfMsgHdrToVisapult(void){
  return (size_t)(16+2+128);
}
size_t PV3_SizeOfMsgBodyToVisapult(int nprocs){
  return (size_t)(128*nprocs);
}

void PV3_GenMsgHdrToVisapult(int nprocs, 
			     int bigendian,int elementsize,
			     int *globaldims,int *p_topo,
			     char *message){
  int i;
  sprintf(message,"%u",
	  nprocs); message+=16;
  printf("Elementsize = %u charelem=%c\n",elementsize,'0'+elementsize);
  sprintf(message,"%c%c",
	  bigendian?'b':'l',(char)('0'+elementsize)); message+=2;
  sprintf(message,"%u,%u,%u:%u,%u,%u",
	  globaldims[0],globaldims[1],globaldims[2],
	  p_topo[0],p_topo[1],p_topo[2]); message+=128;
}

void PV3_GenMsgBodyToVisapult(int nprocs, 
				      ProcInfo *pinfo,
				      char *message){
  int i;
  for(i=0;i<nprocs;i++){
    sprintf(message,"%u,%u,%u:%u,%u,%u",
	    pinfo[i].origin[0],pinfo[i].origin[1],pinfo[i].origin[2],
	    pinfo[i].dims[0],pinfo[i].dims[1],pinfo[i].dims[2]);
    message+=128;
  }
}
/*==================================================================*/
/*                     UDP Message Formats                          */
/*  Cactus Generates a UDP Data Packet to send to Visapult          */
/*==================================================================*/

int PV3_SizeOfHdrV2() {return 4*6;} /* 4*6 */
int PV3_SerializeDataV2(int *origin,int *dims,int *idx,
			int swapdata,float *data,
			char *msg,int nbytes){
  int *hdr = (int*)((void*)msg);
  int *idata = (int*)((void*)data);
  int over=nbytes%4;
  int oldidx=*idx/4;
  nbytes-=over; /* msg size must be multiples of word size */
  hdr[0]=htonl(-1);
  hdr[1]=htonl(origin[0]);
  hdr[2]=htonl(origin[1]);
  hdr[3]=htonl(dims[0]);
  hdr[4]=htonl(dims[1]);
  hdr[5]=htonl((*idx/sizeof(float)));
  /* I'm going to first bound the msg size */
  int maxbytes = sizeof(float)*dims[0]*dims[1]*dims[2];
  if(*idx+nbytes>=maxbytes){ /* curtail the msg size if oversized */
    nbytes=maxbytes-*idx;
    *idx=0;
  }
  else {
    *idx+=(nbytes-12);
  }
  if(swapdata){
    int nwords=nbytes>>2;
    for(int i=0;i<nwords-6;i++)
      hdr[i+6]=htonl(idata[i+oldidx]);
  }
  else
    memcpy(msg+4*6,data+oldidx,nbytes-4*6);
  return nbytes; /* this is the total length of the resulting message */
}
int PV3_SizeOfHdrV3() {return 4*2;} /* 4*2 */
int PV3_SerializeDataV3(int proc,int *idx,
		       int *dims,int swapdata,float *data,
		       char *msg,int nbytes){
  int *hdr = (int*)((void*)msg);
  int *idata = (int*)((void*)data);
  int over=nbytes%sizeof(float);
  int oldidx=*idx/sizeof(float);
  nbytes-=over; /* msg size must be multiples of word size */
  hdr[0]=htonl(-3); /* msg type if v3 data msg */
  hdr[1]=htonl(proc);
  hdr[2]=htonl((*idx/sizeof(float))); // /4 so that index counts elements
  /* I'm going to first bound the msg size */
  int maxbytes = sizeof(float)*dims[0]*dims[1]*dims[2];
  if(*idx+nbytes>=maxbytes){ /* curtail the msg size if oversized */
    nbytes=maxbytes-*idx+12;
    *idx=0; // its the end, so lets recycle the idx (modulo)
  }
  else {
    *idx+=(nbytes-12); // its in the middle, so keep incrementing
  }
  if(swapdata){
    int nwords=nbytes>>2;
    for(int i=0;i<nwords-3;i++)
      hdr[i+3]=htonl(idata[i+oldidx]);
  }
  else
    memcpy(msg+sizeof(float)*3,data+oldidx,nbytes-sizeof(float)*3);
  return nbytes; /* this is the total length of the resulting message */
}

/* must read the data first and then serialize into buffer */
int PV3_ReadDataHdrV2(int *origin,int *dims,int *idx,char *msg){
  /* assume packet type is already correct */
  int *hdr = (int*)((void*)msg);
  origin[0]=ntohl(hdr[1]);
  origin[1]=ntohl(hdr[2]);
  dims[0]=ntohl(hdr[3]);
  dims[1]=ntohl(hdr[4]);
  *idx=ntohl(hdr[5]);
  return 6*sizeof(int);
}

/* returns size of remaining data payload */
int PV3_ReadDataHdrV3(int *proc,int *idx,char *msg){
    /* assume packet type is already correct */
  int *hdr = (int*)((void*)msg);
  *proc=ntohl(hdr[1]);
  *idx=ntohl(hdr[2]);
  return 3*sizeof(int);
}

int PV3_ReadDataV2(char *msg,int hdrsize,int nbytes,
		 int swapdata,int idx,float *data){
  int *idata=(int*)((void*)data);
  int *hdr =(int*)((void*)msg);
  int oldidx=idx/4;
  if(swapdata){
    int nwords=nbytes>>2;
    for(int i=0;i<nwords-6;i++)
      idata[i+oldidx]=htonl(hdr[i+6]);
  }
  else
    memcpy(data+oldidx,msg+4*6,nbytes-4*6);
  return nbytes-4*6;
}

int PV3_ReadDataV3(char *msg,int hdrsize,int nbytes,
		 int swapdata,int idx,float *data){
  int *idata=(int*)((void*)data);
  int *hdr =(int*)((void*)msg);
  int oldidx=idx/4;
  if(swapdata){
    int nwords=nbytes>>2;
    for(int i=0;i<nwords-3;i++)
      idata[i+oldidx]=htonl(hdr[i+6]);
  }
  else
    memcpy(data+oldidx,msg+4*3,nbytes-4*3);
  return nbytes-4*3;
}
