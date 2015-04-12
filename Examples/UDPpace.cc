#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif

#include "RawUDP.hh"
#include "Timer.hh"
#include "Delay.hh"
#include "RateControl.hh"


/*
  Usage: UDPpace hostname npackets packets_per_second
*/
void main(int argc,char *argv[]){
  if(argc<4) { 
    printf("Usage:  %s host npackets packets_per_second\n",
	   argv[0]);
    exit(0);
  }
  long npackets=atoi(argv[2]),count=0;
  long packets_per_second=atoi(argv[3]);
  char buffer[1500];
  long *ibuffer = (long*)((void*)buffer);
  Timer t;
  int single_send=0,more_send=0,max_send_size=1,max_wait=0;
  double packetrate = (double)packets_per_second; /* double precision computed from packets_per_second */
  double *packetinfo=new double[npackets*8],*info;
  info=packetinfo;

  RateControl r(packetrate*1500.0);
  t.reset(); t.start();
  r.setRateBytes(packetrate*1500.0);
  r.checkAccuracy(npackets,1500.0);
  r.reset();
  timeval tn,tl,ti,td;
  double dti,maxdti=0.0,mindti=100000.0;
  ZeroTimeval(tn);
  ZeroTimeval(tn);
  DoubleToTimeval(0.00002,ti);
  for(count=0;count<npackets;count++){
    AddTimeval(tl,ti,tn);
    SubtractTimeval(tn,tl,td);
    TimevalToDouble(td,dti);
    if(dti>maxdti) maxdti=dti;
    if(dti<mindti) mindti=dti;
    CopyTimeval(tn,tl);
  }
  printf("DTI Max/Min=%f:%f for intended %f\n",maxdti,mindti,0.00002);
  for(count=0;count<npackets;count++){
    int v,w;
    /* find out rate control issues */
    w=0;
    while((v=r.checkIntervalBytes(1500l))<=0){
      // puts("\t wait");
      w++;
    } /* my packet */
    if(w>max_wait) max_wait=w;
    info+=r.getState(info);
    /* check interval and sentdata : netierh returns proper values */
    if(v<=1){ /* this payload is good */
      // puts("v==1");
      single_send++;
      r.sentDataBytes(1500.0);
    }
    else if(v==2){
      int sendfactor;
      sendfactor=0;
      more_send++;
      //puts("v==2");
      /* OK, lets increase payload until its happy */
      while(v=r.sentDataBytes(1500.0)>1){
	sendfactor++;
	if(sendfactor>=16){
	  //  r.dumpState();
	  break;
	}
      }
      if(sendfactor>max_send_size) max_send_size=sendfactor;
      count=count+sendfactor-1;
    }
    else {
      printf("what?  v=%u\n",v);
    }
    r.newInterval();
    //printf("sent %u:%u\n",count,npackets);
  }
  t.stop();
  t.print("output is");
  printf("singles=%u mores=%u max_coalesce=%u max_wait=%u cycles\n",
	 single_send,more_send,max_send_size,max_wait);
  /* FILE *file=fopen("log.txt","w");
  int cols=r.getState(info);
  for(count=0;count<npackets;count++){
    for(int c=0;c<cols-1;c++){
      fprintf(file,"%f,",*packetinfo); packetinfo++;
    }
    fprintf(file,"%f\n",*packetinfo); packetinfo++;
  }
  fclose(file); */
  exit(0);
  // we will use delay between each packet for this right now 
  // (but really want to use direct timing of the pacing instead!)
  // You can modify Delay.hh Delay.cc to do timing for you.
  // Please refer to example Examples/TestDelay.cc for all the
  // parameters for Delay and how to use it.
  SpinDelay delay(1.0/packetrate);
  RawUDPclient udp(argv[1],7000); // implict connect
  ibuffer[1] = npackets; // write total number of packets into buffer

  for(count=0;count<npackets;count++){
    ibuffer[0]=count; // write current count in the sequence to buffer
    udp.write(buffer,1024); /* write in 1024 byte chunks for easy computation */
    /* now do a poll-delay until time to deliver next packet.
       first measurement is assumed time of last delivery.
    */
    delay.instantaneousWait(); 
  }

  ibuffer[0]=npackets; // send completion packet many times to ensure delivery
  for(int i=0;i<10;i++){
    udp.write(buffer,16);
    usleep(i);
  }
}

