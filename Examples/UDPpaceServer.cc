#include <stdio.h>
#include <stdlib.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif

#include "RawUDP.hh"
#include "Timer.hh"

/* 
   Implements connectionless UDP paced delivery service 
*/
inline int ParseHeader(char *s, long &item, long &of_items){
  // item=atoi(s); do cross-platform in ASCII
  // of_items=atoi(s+128);
  long *i = (long*)((void*)s); // convert pointer type
  item = i[0];
  of_items = i[1];  // this will not work cross-platform 
  // printf("Got %u of %u\n",item,of_items); debug
  if(item==of_items) return 1; // this is the end of a stream of packets
  else return 0; // this is the beginning or middle of a stream of packets
}

void main(int argc,char *argv[]){
  RawUDPserver udp(7000);
  long last_seq,seq,done;
  Timer timer;

  while(1){ // loop forever
    int sz;
    double duration;
    long lost,npackets,total,out_of_order;
    char buffer[1500],machine_name[128];
    IPaddress returnaddress;

    sz=udp.recv(buffer,1500,returnaddress);
    if(ParseHeader(buffer,seq,total)) continue;  // still end of stream
    printf("Starting new stream of %u packets\n",total);
    lost=npackets=out_of_order=0;
    last_seq=seq;
    timer.reset();
    timer.start(); /* start timing immediately */
    
    do {
      sz=udp.recv(buffer,1500,returnaddress);
      done = ParseHeader(buffer,seq,total);
      if(seq-last_seq!=1) out_of_order++; // non-consecutive packet arrival
      last_seq=seq;
      npackets++;
    } while(!done);
    
    timer.stop(); // stop timing (its the end of the packet stream
    puts("Stream Completed");
    duration = timer.realTime();
    /* now compute an print statistics */
    printf("Observed %u packets in %g seconds.  Rate = %g pps\n",
	   npackets,
	   duration,
	   ((double)npackets)/duration);
    lost = total-npackets; /* number of lost packets is total expected 
			      packets - the number that actually arrived */
    printf("\tNumber of lost packets = %u of %u\n\tPercent Loss=%g\n\tNumber out of order = %u\n",
	   
	   lost,
	   total,
	   ((double)lost)*100.0/((double)total),
	   out_of_order);
  }
}
