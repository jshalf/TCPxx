#include <stdio.h>
#include <stdlib.h>

#include "ProtocolV3.hh"

#include <RawTCP.hh>
#include <RawUDP.hh>
#include <PortMux.hh>
#include <Timeval.hh>
#include "MPIutils.hh"

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
  int i,n;
  char *msg;
  char tmphostname[128];
  int size;
  RawTCPclient *sock;
  RawUDPserver *udp;
  CactusInfo *info;
  MPIenv env(argc,argv);
  char mymsg[128];
  MPIcomm *mpi = env.getComm(); // fork mpi processes and get MPI communicator
  //------------------------------
  hostname=hname;
  if(argc>1) hostname=argv[1];
  if(argc>2) port=atoi(argv[2]);
  sock=new RawTCPclient(hostname,port);
  /* get a message from sock */
  info = PV3_ReadCactusMessage(sock->getPortNum());
  PrintCactusInfo(info);
    /* reply to cactus */
  size=PV3_SizeOfMsgToCactus(info->nprocs);
  msg = new char[size];
    /* OK, we map all of the procs to a single host/port for single thread vers*/
  udp = new RawUDPserver(port);
  i=0;
  // scan for a free port if failure (but only scan 100 locations)
  while(!udp->isValid()){ 
    if(i>100) exit(0);
    delete udp;
    port++;
    udp=new RawUDPserver(port);
  }
  gethostname(tmphostname,128);
  
  sprintf(mymsg,"%s:%u",tmphostname,port);
  mpi->gather(0,128,mymsg,msg);
  PV3_AssignHostsRoundRobin(msg,mpi->numProcs(), /* local hostlist, local nprocs */
			    msg,info->nprocs);/* cactus hostlist, cactus nprocs */
  if(mpi->rank()==0)
    sock->write(msg,size);
  delete msg; // msg is done
  /* we will get stuff for a while and then die */
  /* should try to sense TCP drop */
  timeval lasttime,now;
  double dt,measurement_interval=2.0; /* 2 second measurement interval */
  int64_t npkts_total,*npkts,*last_pkt_count,*this_pkt_count;
  int nlocalprocs;
  int *myprocs,*procmap; // double indirect referencing of processors
  myprocs=new int[1+info->nprocs/mpi->numProcs()];
  npkts = new int64_t[1+info->nprocs/mpi->numProcs()];
  last_pkt_count = new int64_t[1+info->nprocs/mpi->numProcs()];
  this_pkt_count = new int64_t[1+info->nprocs/mpi->numProcs()];
  procmap=new int[info->nprocs];
  /* now we do a round-robin mappings of myprocs to procindex */
  for(i=0,n=0;i<info->nprocs;i++){
    if(i%mpi->numProcs() == mpi->rank()){
      procmap[i]=n;
      myprocs[n]=i;
      npkts[n]=0;
      last_pkt_count[n]=0;
      this_pkt_count[n]=0;
      n++;
    }
    else procmap[i]=-1; // bad mapping (this shouldn't me my proc)
  }
  nlocalprocs=n;
  npkts_total=0;
  // need to have a list of procs
  GetTime(now);
  CopyTimeval(now,lasttime);
  for(i=0;i<info->nprocs*100;i++){
    register char buffer[1500];
    register int packettype;
    register timeval tm;
    register int *ibuf=(int*)((void*)buffer);
 
    udp->read(buffer,sizeof(buffer));
    npkts_total++;
    /* now route the buffer to proper dest */
    /* for now, lets just print headers */
    packettype=ntohl(ibuf[0]);
    // record last 
    if(packettype==-3){ // this is a V3 Data Packet
      /* Header format
	 word[0]=-3
	 word[1]=proc
	 word[2]=offset
      */
      register int src; // this is the source
      // This is an info packet
      src=ntohl(ibuf[1]);
      n=procmap[src]; // map PID to densly packed vector
      npkts[n]++;
    }
    if(packettype==-2){ // V2 info packet
      /* Format
	 word[0]=-2
	 word[1]=proc
	 word[2]=numprocs
	 word[3-5]=globaldims
	 word[6-8]=localorigin
	 word[9-11]=localdims
	 word[12-14]=nghost
	 word[15]=slabnum (bogus for protocol V3)
	 word[16]--> doubleword = packetcount (dword aligned access)
      register int src; // this is the source
      int64_t *p = (int64_t*)((void*)(ibuf+16));
      // This is an info packet
      src=ntohl(ibuf[1]);
      n=procmap[src]; // map PID to densly packed vector
      npkts[n]++;
      // must extract the most recent packet counter from it
      this_pkt_count[n]=ntohl64(p[0]);
    }
    else if(packettype==-4){ // V3 info packet (obsoleted)
      /* Format
	 word[0] = -4
	 word[1] = proc
	 word[2] = numprocs
	 word[3] = ---empty---
	 word[4-5]=doubleword packetcount (dword aligned access)
       */
      register int src;
      int64_t *p = (int64_t*)((void*)(ibuf+4));
      // This is an info packet
      src=ntohl(ibuf[1]);
      n=procmap[src]; // map PID to densly packed vector
      npkts[n]++;
      // must extract the most recent packet counter from it
      this_pkt_count[n]=ntohl64(p[0]);
    }
    GetTime(now);
    SubtractTimeval(now,lasttime,tm);
    TimevalToDouble(tm,dt);
    if(dt>measurement_interval){
      double rate_mbps,loss_percent,rate_mbps_total;
      rate_mbps_total = (double)npkts_total*8.0*1468.0/(1000000.0*dt); // assumes 1500 MTU packets
      for(n=0;n<nlocalprocs;n++){
	int64_t pktcount = this_pkt_count[n]-last_pkt_count[n];
	// now we compute timing stats
	rate_mbps = (double)npkts[n]*8.0*1468.0/(1000000.0*dt); // assumes 1500 MTU packets	
	
	loss_percent = 100.0*(double)(npkts[n]-pktcount)/(double)pktcount;
	// for independent measurements
	printf("CP[%u] VP[%u] Rate(local,total)=%f:%f Loss=%f percent\n",
	       myprocs[n],mpi->rank(),
	       rate_mbps,rate_mbps_total,loss_percent);
	// for dependent measurements, we will have to put an 
	// mpi gather op here to collect the results
	// but this will itself cause packet loss
	/* otherwise, we need an async way to agregate this info */
	
      }
      CopyTimeval(now,lasttime); // reset timer
      last_pkt_count[n]=this_pkt_count[n]; // reset pkt count
      npkts[n]=0; // reset rcvd. pkt counter
    }
#if PRINT_PKTS
    if(packettype==-1)
      printf("P[%u] PacketHeader ptype=%d orig=%u,%u dims=%u,%u index=%u\n",
	     mpi->rank(),
	     ntohl(ibuf[0]),ntohl(ibuf[1]),ntohl(ibuf[2]),
	     ntohl(ibuf[3]),ntohl(ibuf[4]),ntohl(ibuf[5])
	     );
    else if(packettype==-3){
      int index=ntohl(ibuf[2]);
      printf("P[%u] PacketHeader ptype=%d proc=%u index=%u\n",
	     mpi->rank(),
	     ntohl(ibuf[0]),ntohl(ibuf[1]),nqtohl(ibuf[2]));
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
#endif
  }
}
