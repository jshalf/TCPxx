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

struct ProcData {
  ProcInfo &pinfo;
  int size,nbytes,current_byte;
  float *data;
  char *bytes;
  ProcData(float val,ProcInfo &info):pinfo(info),size(info.dims[0]*info.dims[1]*info.dims[2]),current_byte(0),nbytes(size*sizeof(float)){
    data=new float[size];
    bytes=(char*)((void*)data);
    printf("Allocat ProcData val=%f size=%u\n",val,size);
    for(int i=0;i<size;i++) data[i]=val;
  }
  ~ProcData(){delete data;}
  int initSeg(){ return current_byte=0;}
  int copySeg(char *buf,int bufsize){
    int over=bufsize%sizeof(float);
    bufsize-=over;
    printf("Over was %u bufsize=%u\n",over,bufsize);
    if((current_byte+bufsize)>=nbytes-1){
      printf("Would Overflow current_byte=%u Bufsize=%u > nbytes=%u\n",
	     current_byte,bufsize,nbytes);
      bufsize = nbytes - current_byte;
      printf("new Bufsize=%u\n",bufsize);
    }
    printf("copying to buf at location + %u amount=%u\n",
	   current_byte,bufsize);
    memcpy(buf,bytes+current_byte,bufsize);
    current_byte+=bufsize;
    printf("currentbyte is now %u bufsize=%u\n",current_byte,bufsize);
    if(current_byte>=nbytes-1) current_byte=0;
    printf("Correct for wraparound=%d current_byte=%u\n",
	   nbytes,current_byte);
    return bufsize; /* return actual transfer size sent */
  }
};

struct CactusData {
  int nprocs;
  typedef ProcData* ProcDataP;
  ProcDataP *pdata;
  CactusData(CactusInfo *info):nprocs(info->nprocs){
    pdata=new ProcDataP[nprocs];
    for(int i=0;i<nprocs;i++)
      pdata[i]=new ProcData((float)i,info->pinfo[i]);
  }
  ~CactusData(){
    for(int i=0;i<nprocs;i++) delete pdata;
  }
  float *getData(int i){return pdata[i]->data;}
  ProcInfo &getInfo(int i){return pdata[i]->pinfo;}
};

int ProcLayoutSeive(int np,int decomp[3]){
  // select minimum surface area processor arrangement
  int minarea = np*np*np; // impossibly high value
  for(int i=1;i<=np;i++){
    for(int j=1;i*j<=np;j++){
      for(int k=1;i*j*k<=np;k++){
        if(i*j*k!=np) continue;
        int area = i+j+k; // area metric 
	// (optimizing this metric weakly minimizes surface area)
        if(area<minarea) {
          minarea=area;
          decomp[0]=i;
          decomp[1]=j;
          decomp[2]=k;
        }
      }
    }
  }
  return minarea;
}

int *EdgeDecomp(int ndiv,int npoints){
  int *edge=new int[ndiv+1];
  int incr=npoints/ndiv;
  edge[0]=0;
  for(int i=1;i<ndiv;i++) edge[i]=edge[i-1]+incr;
  edge[ndiv]=npoints;
  printf("make edge ndif=%u npoints=%u incr=%u last=%u\n",
	 ndiv,npoints,incr,edge[ndiv]);
  return edge;
}

static int SenseByteOrder(void){
  typedef union byteorder {
    int  i;
    char c[4];
  } byteorder;
  byteorder b;
  b.i=0x01020304;
  if(b.c[0]==0x01) return 1;
  return 0;
}

CactusInfo *AutoGenCactusInfo(int nprocs,int *globaldims){
  CactusInfo *info = new CactusInfo;
  int i;
  info->nprocs=nprocs;
  info->bigendian=SenseByteOrder();
  info->elementsize=sizeof(float);
  for(i=0;i<3;i++) info->globaldims[i]=globaldims[i];
  ProcLayoutSeive(nprocs,info->proc_topology);
  info->pinfo=new ProcInfo[nprocs];
  /* now we have to domain decomp */
  int counter,p[3];
  int *topo=info->proc_topology;
  typedef int *intP;
  intP edge[3];
  printf("Proc Decomp=%u,%u,%u\n",info->proc_topology[0],
	 info->proc_topology[1],
	 info->proc_topology[2]);
  /* compute the edge coords for a uniform decomp (+ remainder on the ends */
  for(i=0;i<3;i++) edge[i]=EdgeDecomp(topo[i],globaldims[i]);
  /* now compute the actual origin and dims for each block of that decomp */
  for(counter=0,p[2]=0;p[2]<topo[2];p[2]++){
    for(p[1]=0;p[1]<topo[1];p[1]++){
      for(p[0]=0;p[0]<topo[0];p[0]++,counter++){
	for(i=0;i<3;i++){ /* compute origin and dims for each proc */
	  info->pinfo[counter].origin[i]=(edge[i])[p[i]];
	  info->pinfo[counter].dims[i]=(edge[i])[p[i]+1]-(edge[i])[p[i]];
	}
      }
    }
  }
  for(i=0;i<3;i++) delete edge[i];
  return info;
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
    printf("\tPE[%u] hostmapping is [%s]\n",i,msg+idx);
}

#define PERRORS
int TCPMustRead(RawTCPport *sock,char *buffer,int buflen){
  /* do errno only if types.h has been included */
    /* how can I tell if this has been included ? */
    int n,accum=0;
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

int main(int argc,char *argv[]){
  /* define the params for our decomp */
  int gdims[3]={30,32,35};
  int nprocs=8;
  int i;
  CactusInfo *info = AutoGenCactusInfo(nprocs,gdims);
  PrintCactusInfo(info);
  /* ======= OK, now we get into the thick of it ======== */
  char *msg;
  int size;
 /* OK, now we serialize our domain to the visapult backEnd */
  size=PV3_SizeOfMsgToVisapult(info->nprocs);
  msg=new char[size];
  PV3_GenMsgHdrToVisapult(info->nprocs,/* int nprocs,*/
			  info->bigendian,/* int bigendian,*/
			  info->elementsize,/* int elementsize, */
			  info->globaldims,/* int *globaldims, */
			  info->proc_topology,/* int *p_topo, */
			  msg /* char *message */ );
  PV3_GenMsgBodyToVisapult(info->nprocs, 
			   info->pinfo,
			   msg+PV3_SizeOfMsgHdrToVisapult());
  PrintMsgToVisapult(msg,nprocs);
  CactusData *cdata=new CactusData(info);
  /* =========== Socket stuff ========*/
  RawTCPserver *server;
  RawTCPport *client;
  PortMux server_ready;
  int port=7777;
  int r;
  if(argc==2) port=atoi(argv[1]);
  
  printf("opening server socket on %u\n",port);
  server = new RawTCPserver(port);
  server_ready.addInport(server);
  server_ready.setTimeout(1l,0l);
  do { 
    puts("waiting for client connection"); 
    /* ---- do work loop ----- */
  } while(server_ready.select()!=server);
  client = server->accept();
  /* send procinfo to visapult */
  client->write(msg,size);
  size=PV3_SizeOfMsgToCactus(nprocs);
  r=MustRead(client->getPortNum(),msg,size);
  printf("MustRead returned %u\n",r);
  PrintMsgFromVisapult(msg,info->nprocs);
  puts("======  Now we start connecting and sending UDP =====");
  typedef RawUDPclient *RawUDPportP;
  RawUDPportP *udp = new RawUDPportP[info->nprocs];
  for(i=0;i<nprocs;i++){
    char *host=msg+i*128;
    char *s;
    s=strchr(host,':');
    *s='\0';
    port=atoi(s+1);
    printf("connecting to host=%s port=%u\n",host,port);
    udp[i] = new RawUDPclient(host,port);
  }
  sleep(2);
  for(i=0;i<100;i++){
    char buffer[1496];
    int sendsize,n;
    for(n=0;n<nprocs;n++){
#if 0
      sendsize=PV3_SerializeDataV2(cdata->pdata[n]->pinfo.origin,
				   cdata->pdata[n]->pinfo.dims,
				   &(cdata->pdata[n]->current_byte),
				   0, /* do not swap data (1=swap) */
				   cdata->pdata[n]->data,
				   buffer,sizeof(buffer));

      udp[n]->send(buffer,sendsize);
#endif
      /* or in the case of sending PV3 packets */
      
      printf("Proc[%u] send at index %u", n, (cdata->pdata[n]->current_byte));
      sendsize = PV3_SerializeDataV3(n, /* processor number */
				     &(cdata->pdata[n]->current_byte),
				     cdata->pdata[n]->pinfo.dims,
				     0, /* do not swap data (1=swap) */
				     cdata->pdata[n]->data,
				     buffer,sizeof(buffer));
      //printf("Proc[%u] offset=%u sendsize=%u\n",n,sendsize);
      printf(" sendsize=%u\n",sendsize);
      udp[n]->send(buffer,sendsize);
      usleep(1000);
    }
  }
}

