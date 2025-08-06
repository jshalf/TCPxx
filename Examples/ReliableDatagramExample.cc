#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif

#include "ReliableDatagram.hh"
#include "Timer.hh"
#include "Delay.hh"
#include "RateControl.hh"
#include "PacketPacer.hh"

#define USE_RATE_CONTROL
// #define USE_PACKET_PACER
// #define USE_TIMER

/*
  Usage:
      as client
      ReliableDatagramExample hostname npackets packets_per_second

      as server
      ./ReliableDatagramExample
*/
int main(int argc,char *argv[]){
  if(argc!=4 && argc!=1) { 
    printf("Usage:  %s host <npackets> <packets_per_second>\n",
	   argv[0]);
    printf("\tor for server mode use\n\t%s\n",argv[0]);
    printf("\n\nPackets are assumed to be 1480.0 bytes (not counting UDP and RIPP header) in this example\n");
    exit(0);
  }
  char buffer[1500];
  if(argc==4){ // we are the sender
    ReliableDatagram udp(argv[1],7777); // client connection
   // udp.verifyChecksums(); // turn on manual checksum verification
	// if the OS is configured to verify UDP checksums, then don't turn this on
	// it would be redundant.
    double packetrate = atof(argv[3]);
    double byterate = packetrate * 1480.0;
    double timeout;
    int packetcount,npackets = atoi(argv[2]);
    timeout = 5.0; // 5 second timeout on ping
    if(udp.ping(buffer,1460,timeout)) printf("First Ping RTT=%fs\n",timeout);
    else printf("First Ping failed\n");
    timeout = 5.0;
    if(udp.ping(buffer,1460,timeout)) printf("Second Ping RTT=%fs\n",timeout);
    else printf("Second Ping failed\n");
    timeout = 5.0;
    if(udp.ping(buffer,1460,timeout)) printf("Third Ping RTT=%fs\n",timeout);
    else printf("Third Ping failed\n");
#ifdef USE_RATE_CONTROL
    // RateControl is the most accurate and most efficient
    // method for pacing packets.  It only makes system calls
    // to determine the current time and packet rate when it
    // absolutely needs to in order to reduce overhead for very
    // high packet rates.  It also uses error diffusion to
    // pace packets extremely accurately even at high data
    // rates and poor system clock granularity.
    // The downside is that it looks really complicated to use...
    RateControl rate(byterate);
    rate.setRateBytes(byterate);
    rate.reset();
    printf("now start sending %u packets at %f packets per second\n",npackets,(float)packetrate);
    for(packetcount=0;packetcount<npackets;){
      int w=0,packets_sent=0;
      double plen=1480.0; // we are sending fixed sized packets,
      // but plen can potentially be different for every packet
      // printf("Pause for interval\n");
      rate.pauseIntervalBytes(plen); // pause to pace packets
      //printf("continue now\n");
      // the pauseIntervalBytes will attempt to use a blocking
      // wait if it is available.  Otherwise, for small inter-packet
      // gaps, it has to resort to a spin-wait.
      do { // now, if we over-shot on the pause, then we have to 
	// send a bunch of packets without inter-packet pacing to
	// fill in the gap (but only up to 16 consecutive)
	snprintf(buffer,sizeof(buffer),"PKT: %04u",packetcount);
	printf("Sending Packetbuffer=[%s]\n",buffer);
	udp.send(buffer,1460);
	packets_sent++;
	packetcount++;
      } while(rate.sentDataBytes(plen)>1 && 
	      !(packets_sent>=16 || packetcount>npackets));
      rate.newInterval();
      //printf("set new interval\n");
    }
#elif defined(USE_PACKET_PACER)
    // The PacketPacer is nearly as accurate as the RateControl
    // because it uses error diffusion.  However, it makes heavier
    // use of the system calls to determine timing than RateControl
    // Consequently, the CPU/syscall overhead can be much higher 
    // if used for very high packet rates.
    PacketPacer rate;
    rate.setByteRate(byterate);
    rate.reset();
    printf("now start sending %u packets at %f packets per second\n",npackets,(float)packetrate);
    for(packetcount=0;packetcount<npackets;packetcount++){
      unsigned int plen=1480; // we are sending fixed sized packets,
      // but plen can potentially be different for every packet
      // printf("Pause for interval\n");
      rate.pace(plen); // pause to pace packets
      snprintf(buffer,sizeof(buffer),"PKT: %04u",packetcount);
      printf("Sending Packetbuffer=[%s]\n",buffer);
      udp.send(buffer,1460);
    }
#elif defined(USE_TIMER)
    // using a delay timer is a less accurate
    // but probably will work adequately for low packet rates.
    // SelectDelay uses a select() call to do the wait
    // there is also a HybridDelay and SpinDelay class
    // spindelay offers a more accurate delay, but burns the CPU
    // to do so.  HybridDelay switches between a Select-based delay
    // and a Spin-based-delay.
    // where available, the NanoSleepDelay uses nanosleep() to 
    // implement the delay.  However it is not univerally
    // available so it will use select if not available.
    NanoSleepDelay delay; // or SelectDelay or SpinDelay or HybridDelay
    delay.setDelay(1.0/packetrate);
    printf("now start sending %u packets at %f packets per second\n",npackets,(float)packetrate);
    for(packetcount=0;packetcount<npackets;packetcount++){
      delay.instantaneousWait();
      snprintf(buffer,sizeof(buffer),"PKT: %04u",packetcount);
      printf("Sending Packetbuffer=[%s]\n",buffer);
      udp.send(buffer,1460);
    }
#endif
    snprintf(buffer,sizeof(buffer),"END"); // send completion packet
    printf("send the completion packet [%s]\n",buffer);
    // udp.send(buffer,1460);
    udp.send("END",4);
    udp.waitForPending(); // wait until everything is acked before closing
    // while(udp.check()){} // still have some stuff pending
  }
  else if(argc==1){ // we are the recvr
    ReliableDatagramServer svr(7777);
    ReliableDatagram *udp = svr.accept();
    udp->verifyChecksums(); // turn on checksum verification
	// if the host is set up to do checksum verification, you should not do that
    while(1){
      int sz=udp->read(buffer,1500); // make sure returned size strips extra data
      if(sz>0){
	printf("Packet: [%s] size=%u\n",buffer,sz);
	if(*buffer=='E') break; // end packet arrived
      }
      else {
	printf("Zero Length Packet Arrived\n");
      }
    }
    printf("close up the udp socket and then exit\n");
    delete udp;
  }
  else {
    puts("How the heck did I get here??");
  }
  return 1;
}
