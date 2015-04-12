#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif

#include "RIPP.hh"

/*
  Usage: UDPpace hostname npackets packets_per_second
*/
int main(int argc,char *argv[]){
  if(argc!=4 && argc!=1) { 
    printf("Usage:  %s host npackets packets_per_second\n",
	   argv[0]);
    printf("\tor for server mode use\n\t%s\n",argv[0]);
    exit(0);
  }
  char buffer[1500];
  if(argc==4){ // we are the sender
    RIPP ripp(argv[1],7777); // client connection
    double packetrate = atof(argv[3]);
    double byterate = packetrate * 1480.0;
    int packetcount,npackets = atoi(argv[2]);
    ripp.verifyChecksums(); // enable if system does not perform UDP checksums
    ripp.setByteRate(byterate);
    printf("now start sending %u packets\n",npackets);
    for(packetcount=0;packetcount<npackets;packetcount++){
      sprintf(buffer,"PKT: %04u",packetcount);
      printf("Sending Packetbuffer=[%s]\n",buffer);
      ripp.send(buffer,1460);
    }
    printf("send the completion packet [END]\n");
    // udp.send(buffer,1460);
    ripp.send("END",4);
    ripp.waitForPending(); // wait until everything is acked before closing
  }
  else if(argc==1){ // we are the recvr
    ReliableDatagramServer svr(7777);
    ReliableDatagram *ripp = svr.accept();
    // ripp->verifyChecksums(); // enable if system does not perform UDP checksums
    while(1){
      int sz=ripp->read(buffer,1500); // can be zero length (need blocking rd)
      if(sz>0){
	printf("Packet: [%s] size=%u\n",buffer,sz);
	if(*buffer=='E') break; // end packet arrived
      }
      else {
	printf("Zero Length Packet Arrived\n"); // normally should not happen
      }
    }
    printf("close up the udp socket and then exit\n");
    delete ripp;
  }
  else {
    puts("How the heck did I get here??");
  }
  return 1;
}
