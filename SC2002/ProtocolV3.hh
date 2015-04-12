#ifndef _PROTOCOLV3_HH_
#define _PROTOCOLV3_HH_
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
#if defined(SGI) || defined(OSF)
#include <inttypes.h>
#endif
#include <sys/types.h>

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

typedef struct ProcInfo {
  int origin[3],dims[3];
} ProcInfo;

typedef struct CactusInfo {
  int nprocs;
  int globaldims[3],proc_topology[3];
  int bigendian,elementsize;
  ProcInfo *pinfo;
} CactusInfo;

static int PV3_ParseProcInfo(char *msg,int *origin,int *dims);
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
			  int *globaldims,int *p_topology);
/*
  The body of the cactus message is <nprocs> elements
  Each element is 128 bytes and contains the following info
    origin: origin of this processor's local domain of data
    dims: the dimensions of this local domain
  The format of this message is
     <ox>,<oy>,<oz> : <nx>,<ny>,<nz>
  
 */
void PV3_ParseCactusBody(int nprocs,char *msg_body,
			 ProcInfo *pinfo);

/* Sample of a reader that will read the buf len specified.
   Only fails if the socket is dead
*/
int MustRead(int fd,char *buffer,int buflen);

/* we assume that the master process for this transaction is
   processor 0 */
CactusInfo *PV3_ReadCactusMessage(int sock);
/*==================================================================*/
/*                     Visapult Message Gen                         */
/*  Cactus Generates a Message to send to Visapult                  */
/*==================================================================*/

/* Do round robin assignment of local host/port pairs so as
   to have an assignment for every cactus PE (assumes the
   #PEs on Cactus > local number of PEs */
void PV3_AssignHostsRoundRobin(char *localhostlist,int nlocalprocs,
			       char *cactushostlist,int ncactusprocs);
/* 
   a bogus datastructure created for your convenience.
   An array of these should contain the mapping of Cactus
   PE's to host:port pairs for Visapult.
*/
typedef struct HostList {
  char host[64];
  int port;
} HostList;
/* must know how large the message to cactus is going to be
   so that we can allocate an appropriately sized buffer
   and send the correct buffer size */
size_t PV3_SizeOfMsgToCactus(int nprocs_cactus);
/* 
   Take a host list datastructure and write out a buffer
   to send to Cactus.  The hostlist must provide the host:port
   mapping that tells each Cactus PE where to send its UDP data.

   The buffer "message" must be pre-allocated
*/
void PV3_GenMsgToCactus(int nprocs_cactus,HostList *hostlist,
			char *message);

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
static void PV3_ParseHostPort(char *msg,char *host,int *port);

/* after getting the entire message, we can extract from it our 
   local host and port.  So each proc gets its own unique host:port
   pair to send its message to. */
void PV3_ProcessVisapultMessage(int master,char *message, /* inputs */
				char *host,int *port); /* outputs */
/*==================================================================*/
/*                     Cactus Message Gen                           */
/*  Cactus Generates a Message to send to Visapult                  */
/*==================================================================*/

/* Must pre-allocate a buffer */
size_t PV3_SizeOfMsgToVisapult(int nprocs);
size_t PV3_SizeOfMsgHdrToVisapult(void);
size_t PV3_SizeOfMsgBodyToVisapult(int nprocs);
void PV3_GenMsgHdrToVisapult(int nprocs, 
			     int bigendian,int elementsize,
			     int *globaldims,int *p_topo,
			     char *message);
void PV3_GenMsgBodyToVisapult(int nprocs, 
			     ProcInfo *pinfo,
			     char *message);

/*==================================================================*/
/*                     UDP Message Formats                          */
/*  Cactus Generates a UDP Data Packet to send to Visapult          */
/*==================================================================*/
inline int32_t htonl32(int32_t x){
#ifdef WORDS_BIG_ENDIAN
  return x;
#else
  register int32_t y;
  char *a=(char*)((void*)&x);
  char *b=(char*)((void*)&y);
  b[0]=a[3];
  b[1]=a[2];
  b[2]=a[1];
  b[3]=a[0];
  return y;
#endif
}
inline int64_t htonl64(int64_t x){
#ifdef WORDS_BIG_ENDIAN
    return x;
#else
  register int64_t y;
  char *a=(char*)((void*)&x);
  char *b=(char*)((void*)&y);
  b[0]=a[7];
  b[1]=a[6];
  b[2]=a[5];
  b[3]=a[4];
  b[4]=a[3];
  b[5]=a[2];
  b[6]=a[1];
  b[7]=a[0];
  return y;
#endif
}
#define ntohl64(x) htonl64(x)
#define ntohl32(x) ntohl64(x)

int PV3_SizeOfHdrV2(); /* 4*6 */
int PV3_SerializeDataV2(int *origin,int *dims,int *idx,
			int swapdata,float *data,
			char *msg,int nbytes);
int PV3_SizeOfHdrV3(); /* 4*2 */
int PV3_SerializeDataV3(int proc,int *idx,
			int *dims,int swapdata,float *data,
			char *msg,int nbytes);

int PV3_ReadDataHdrV2(int *origin,int *dims,int *idx,char *msg);
  /* returns size of remaining data payload */
int PV3_ReadDataHdrV3(int *proc,int *idx);
int PV3_ReadDataV2(char *msg,int hdrsize,int nbytes,
		   int swapdata,int idx,float *data);

int PV3_ReadDataV3(char *msg,int hdrsize,int nbytes,
		   int swapdata,int idx,float *data);
#endif
