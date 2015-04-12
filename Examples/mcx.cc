#include "RawMCAST.hh"
#include "RawUDP.hh"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
using namespace std;
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <setjmp.h>

static jmp_buf env;

void handle_quit(int i){
	puts("killed");
	longjmp(env,1);
}
/*
Usage: 
   mcx <mcast_ip> <mcaxst_port> <uni_recv_port> <uni_send_ip> <uni_send_port>

   reads packets from uni_recv_port and sends out to 
   uni_send_ip/uni_send_port and mcast_ip/mcast_port
   
   Must be launched in pairs (even_port, odd_port) to handle 
   both main traffic and RTP info.
 */
int main(int argc,char *argv[]){
  RawMCAST *mc=0;
  RawUDPclient *uc=0;
  RawUDPserver *us=0;
  int done=0;
  signal(SIGINT,handle_quit);
	// multicast sender
  if(argc>=3)
  	mc = new RawMCAST(argv[1],atoi(argv[2]));
  else // set port number
  	mc = new RawMCAST("224.2.177.155",55524); 
  mc->setTTL(127);
  // mc->setLoopback(0); ???
  mc->join(); // unbound.  We are a sender
  // do we need to bind mcast for use?
  if(argc>3)
    us = new RawUDPserver(atoi(argv[3])); // unicast receiver
  else
    us = new RawUDPserver(7000); // unicast receiver
	
  if(argc>=6) // unicast sender 
    uc = new RawUDPclient(argv[4],atoi(argv[5])); 
  else // default unicast sender
    uc = 0; // new RawUDPclient("rain.lbl.gov",7002); 
  done=setjmp(env); // returns 0 initially and 1 after the longjmp
  while(!done){
    char buffer[65556];
    int packetsize=0;
    IPaddress returnaddr;
    packetsize = us->recv(buffer,sizeof(buffer),returnaddr);
    mc->send(buffer,packetsize); // send to mc address
    // initially loopback to source
    if(uc) uc->send(buffer,packetsize);
  }
  mc->leave();
  delete mc;
  if(us) delete us;
  if(uc) delete uc;
}

