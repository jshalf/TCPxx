#include <stdio.h>
#include <stdlib.h>

#include "ProtocolV3.hh"

#include <RawTCP.hh>
#include <RawUDP.hh>
#include <PortMux.hh>

#define SERVER_PORT 7777
#define PX 16;
#define PY 16;
#define PZ 4;

void PrintMsgToVisapult(char *msg,int nprocs){
  int idx=0,i=0;
  puts("PrintMsgToVisapult===========");
  puts(msg); idx+=16;
  printf("%c%c\n",msg[16],msg[17]); idx+=2;
  puts(msg+idx); idx++;
  for(i=0;i<nprocs;i++,idx+=128)
    puts(msg+idx);
}
void PrintMsgFromVisapult(char *msg,int nprocs){
  int idx=0,i=0;
  puts("PrintMsgFromVisapult==========");
  for(i=0;i<nprocs;i++,idx+=128)
    printf("\tPE[%u] hostmapping is [%s]\n",msg+idx);
}
#define PERRORS
int TCPMustRead(RawTCPport *sock,char *buffer,int buflen){
  /* do errno only if types.h has been included */
    /* how can I tell if this has been included ? */
    register int n,accum=0;
    while((n=sock->read(buffer,buflen)) > 0){
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

void PrintCactusInfo(CactusInfo *info){
  printf("nprocs=%u gdims=%u,%u,%u topo=%u,%u,%u",
	 info->nprocs,
	 info->globaldims[0],info->globaldims[1],info->globaldims[2],
	 info->proc_topology[0],info->proc_topology[1],info->proc_topology[2]);
  for(int i=0;i<info->nprocs;i++)
    printf("\tPE[%u] origin=%u,%u,%u dims=%u,%u,%u\n",
	   i,
	   info->pinfo[i].origin[0],
	   info->pinfo[i].origin[1],
	   info->pinfo[i].origin[2],
	   info->pinfo[i].dims[0],
	   info->pinfo[i].dims[1],
	   info->pinfo[i].dims[2]);
}

int main(int argc,char *argv[]){
  char *hostname,hname[]="localhost";
  int port=7777;
  int i;
	 
  hostname=hname;
  if(argc>1) hostname=argv[1];
  if(argc>2) port=atoi(argv[2]);
  RawTCPclient *sock=new RawTCPclient(hostname,port);
  /* get a message from sock */
  // char *hdr,*body;
  char *msg;
  char tmphostname[128];
  int size;
  // hdr=new char[PV3_SizeOfMsgHdrToVisapult()];
  CactusInfo *info = PV3_ReadCactusMessage(sock->getPortNum());
  PrintCactusInfo(info);
  /* reply to cactus */
  size=PV3_SizeOfMsgToCactus(info->nprocs);
  msg = new char[size];
  /* OK, we map all of the procs to a single host/port for single thread vers*/
  RawUDPserver *udp = new RawUDPserver(7778);
  gethostname(tmphostname,128);
  strcpy(tmphostname,"localhost");
  for(i=0;i<size;i+=128)
    sprintf(msg+i,"%s:7778",tmphostname);
    //sprintf(msg+i,"localhost:7778");
  sock->write(msg,size);
  /* we will get stuff for a while and then die */
  /* should try to sense TCP drop */
  for(i=0;i<info->nprocs*10000;i++){
    char buffer[1500];
    int packettype;
    int *ibuf=(int*)((void*)buffer);
    udp->read(buffer,sizeof(buffer));
    /* now route the buffer to proper dest */
    /* for now, lets just print headers */
    packettype=ntohl(ibuf[0]);
    if(packettype==-1)
      printf("PacketHeader ptype=%d orig=%u,%u dims=%u,%u index=%u\n",
	     ntohl(ibuf[0]),ntohl(ibuf[1]),ntohl(ibuf[2]),
	     ntohl(ibuf[3]),ntohl(ibuf[4]),ntohl(ibuf[5])
	     );
    else if(packettype==-3){
      int index=ntohl(ibuf[2]);
      printf("PacketHeader ptype=%d proc=%u index=%u\n",
	     ntohl(ibuf[0]),ntohl(ibuf[1]),ntohl(ibuf[2]));
      if(index>=0 && index<info->nprocs){
	printf("\tcorresponds to z-slice=%u in block with dims %u,%u,%u\n",
	       index/(info->pinfo[index].dims[0]*
		      info->pinfo[index].dims[1]),
	       info->pinfo[index].dims[0],
	       info->pinfo[index].dims[1],
	       info->pinfo[index].dims[2]);
      }
    }
    else if(packettype==-2){
      printf("this is an info packet.  I can't parse it yet\n");
    }
    else
      printf("Unknown packet type %d\n",packettype);
  }
}
