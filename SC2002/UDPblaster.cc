#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DARWIN
#include <iostream.h>
#else
#include <stream.h>
#endif
#include "vatoi.hh"
#include "RawUDP.hh"
#include "Timer.hh"
#include "Delay.hh"

typedef long long int64;
typedef int int32;

typedef union bswap_t {
	char c[4];
	unsigned int i;
} bswap_t;

int hton_int32(int x) { int y; char *bx=(char*)((void*)(&x)); 
	return x; }
//	char *by=(char*)((void*)(&y));\
//	by[0]=bx[3]; by[1]=bx[2]; by[2]=bx[1]; by[3]=bx[0]; \
//	return y;}


/*
  byte testnum
  int32 packetnum (big-endian)
  testinfo string (rate, packetsize, testlength)

  must calculate loss, throughput (megabits)

  testing TCP-unfriendly implementation
*/
/* Implements UDP blasting service */
struct CommandLine { 
  char host[64];
  int port;
  int isserver;
  int nodelay; 
  int packetlength,max_packetlength,step_packetlength;
  double bitrate,max_bitrate,step_bitrate;
  double duration;
  int ntrials;
  int use_scavenger;
  void init(){
    strcpy(host,"localhost");
    port=7000;
    isserver=0; //default to client
    nodelay=0;
    packetlength=1400; // under MTU
    max_packetlength=packetlength;
    step_packetlength=packetlength;
    bitrate=1.0*1024.0*1024.0; // 1Mbit
    max_bitrate=bitrate;
    step_bitrate=bitrate;
    duration=5.0; // 5 second burst
    ntrials=5;
    use_scavenger = 0;
  }
  void print(){
	printf("\thost=%s : port %u\n",host,port);
	printf("\tpacketlength from %u to %u step %u\n",
		packetlength,max_packetlength,step_packetlength);
	printf("\tbitrate from %f to %f step %f\n",
		bitrate,max_bitrate,step_bitrate);
  }
  CommandLine(){init();}
  CommandLine(int argc,char *argv[]){
    init();
    parse(argc,argv);
    print();
  }
  void parse(int argc,char *argv[]){
    for(int i=1;i<argc;i++){
      if(*(argv[i])=='-'){
	char *arg=argv[i]+1;
	char *param=argv[i+1];
	if(*arg=='p') { port=atoi(param); i++; } // set port number
	else if(*arg=='h') { strcpy(host,param); i++; }
	else if(*arg=='s') { isserver=1; }
	else if(*arg=='f') { nodelay=1; bitrate=max_bitrate=10.0*1024.0*1024.0*1024.0; }
	else if(*arg=='l') { 
	  if(!strchr(param,':'))
	    packetlength=max_packetlength=step_packetlength=vatoi(param);
	  else {
	    char *s;
	    s=strchr(param,':'); *s=0; 
	    packetlength=vatoi(param); param=s+1;
	    s=strchr(param,':'); *s=0;
	    max_packetlength=vatoi(param); param=s+1;
	    step_packetlength=vatoi(param);
	  }
	  i++;
	}
	else if(*arg=='b' || *arg=='r') { 
	  if(!strchr(param,':'))
	    bitrate=max_bitrate=step_bitrate=vatof(param);
	  else {
	    char *s;
	    s=strchr(param,':'); *s=0; 
	    bitrate=vatof(param); param=s+1;
	    s=strchr(param,':'); *s=0;
	    max_bitrate=vatof(param); param=s+1;
	    step_bitrate=vatof(param);
	  } 
	  i++;
	}
	else if(*arg=='d') { duration=vatof(param); i++;}
	else if(*arg=='t') { ntrials=atoi(param); i++;}
      }
    }
  }
};

/*
  hash
  proc
  seqnumber

resets the counting if hash changes

make the destination be a border router address
 */
void server(int port){
  int n=0;
  int newtest=1;
  unsigned char current_test=0;
  long lastpacket,tmppacket;
  char buffer[1024*64],firstpacket[1024*64];
  RawUDPserver echo(port);
  buffer[0]=0; firstpacket[0]=0;
  while(buffer[0]!=0x7F){
    int sz;
    IPaddress returnaddr;
    sz=echo.recv(buffer,sizeof(buffer),returnaddr); // just to see where it came from
    n++;
    if(*buffer>current_test){
      /* new test*/
      /* print results from the old test */
      if(*firstpacket!=0){
	printf("Test %u: %s\n",(unsigned int)(*firstpacket),firstpacket+5);
	// we can parse that info string later
	printf("\tNumPackets Received=%u\tLastPacket=%u\tLoss=%f\n",
	       n,
	       lastpacket,
	       100.0*(1.0 - (double)(n-1)/(double)lastpacket));
      }
      current_test=*buffer;
      lastpacket=0; // reset count for last packet
      n=0;
      memcpy(firstpacket,buffer,sizeof(buffer)); // preserver first
      /* parse the test info */
      
    }
   if(sizeof(long)==4){
    memcpy(&tmppacket,buffer+1,sizeof(long)); // somewhat unsafe
    tmppacket=ntohl(tmppacket);// watch for out-of-order-packets
   }
   else if(sizeof(int)==4){
    bswap_t bs;
    bs.c[0]=buffer[1];
    bs.c[1]=buffer[2];
    bs.c[2]=buffer[3];
    bs.c[3]=buffer[4];
    tmppacket=bs.i;
    //memcpy(&tmppacket,buffer+1,sizeof(int)); // somewhat unsafe
    /*fprintf(stderr,"buffer [%x.%x.%x.%x] tmppacket %X:%u\n",
	(unsigned int)(buffer[1]),
	(unsigned int)(buffer[2]),
	(unsigned int)(buffer[3]),
	(unsigned int)(buffer[4]),
	tmppacket,
	tmppacket);*/
    //tmppacket=hton_int32(tmppacket);// watch for out-of-order-packets
   }
   else if(sizeof(short)==4){
    memcpy(&tmppacket,buffer+1,sizeof(int)); // somewhat unsafe
    tmppacket=ntohs(tmppacket);// watch for out-of-order-packets
   }
    if(tmppacket>lastpacket) lastpacket=tmppacket;
  }
  fprintf(stderr,"\n**DONE**\n",n-1); // -1 for quit packet
}

void trial(char *host,int port,int packetsize,double bitrate,
	   double duration,int use_scavenger,int ntrials){
  char buffer[1024*64];
  int i,trial;
  Timer t;
  int nodelay;
  HybridDelay d;
  int n=0;
  int packetnumber=0;
  static unsigned char testnumber=1; // monotonicly increasing
  RawUDPclient echo(host,port); // implict connect
  if (use_scavenger) echo.setDSCP(8); 
  // Given the packetsize and target bitrate, what sort
  // of delay do we need here
  double packetrate = bitrate/(8.0*(double)packetsize);
  /* need to set up instantaneous waits */
  /* need to use the Wait class */
  /* for now, lets use delay class though */
  d.setDelay(1.0/packetrate);
  printf("bitrate=%f packetrate = %f and delay is %f\n",
	 bitrate,packetrate,1.0/packetrate);

  if(bitrate>1024.0*1024.0*1024.0*5.0) nodelay=1;
  else nodelay=0;
 
  for(trial=0;trial<ntrials;trial++,testnumber++){
    sprintf(buffer+5,"Packetsize=%u Bitrate=%f Duration=%f Packetrate=%f InterpacketDelay=%f\n",
	    packetsize,bitrate,duration,packetrate,1.0/packetrate);
    buffer[0]=testnumber;
    n=0;
    t.reset();
    t.start();
    do{ // run for fixed duration
      long tempn=htonl(n);
      memcpy(buffer+1,&tempn,sizeof(long));
      i=echo.write(buffer,packetsize);
      if(!nodelay) d.instantaneousWait();    
      n++;
    }while(t.realTime()<duration);
    fprintf(stderr,"\n");
    printf("%f seconds for npackets=%u rate=%f p/sec\n",t.realTime(),n,(double)n/duration);
    t.stop();
  } // end test
}
void client(char *host,int port,
	    int packetsize,int max_packetsize,int step_packetsize,
	    double bitrate,double max_bitrate,double step_bitrate,
	    double duration,int use_scavenger, int ntrials=5){
  printf("client\tbitrate %f to %f step %f\n",
	 bitrate,max_bitrate,step_bitrate);
  printf("\tpacketsize %u to %u step %u\n",
	 packetsize,max_packetsize,step_packetsize);
  for(;packetsize<=max_packetsize;packetsize+=step_packetsize){
	printf("*******Packetsize %u\n",packetsize);
    for(double br=bitrate;br<=max_bitrate;br+=step_bitrate){
	printf("*******Bitrate %f\n",bitrate);
      trial(host,port,packetsize,br,duration,use_scavenger,ntrials);
    }
  }
  printf("**** Completed bitrate=%f packetsize=%u\n",
	bitrate,packetsize);

  
  // send quit command (10 times for redundancy)
  RawUDPclient echo(host,port); // implict connect to server
  if (use_scavenger) echo.setDSCP(8); 
  char buffer[64];
  for(int i=0;i<10;i++){
    buffer[0]=0x7F;
    i=echo.write(buffer,packetsize);
  }
  // connection implicitly closed at end of subr.
}
void main(int argc,char *argv[]){
  CommandLine cmdln(argc,argv);
  printf("sizeof long= %lu sizeof short =%lu sizeof int=%lu\n",
        sizeof(long),sizeof(short),sizeof(int));
  if(cmdln.isserver) server(cmdln.port);
  else client(cmdln.host,cmdln.port,
	      cmdln.packetlength,cmdln.max_packetlength,cmdln.step_packetlength,
	      cmdln.bitrate,cmdln.max_bitrate,cmdln.step_bitrate,
	      cmdln.duration,cmdln.use_scavenger,cmdln.ntrials);
}
