#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif
#include "Timer.hh"
#include "UDPstream.hh"
#include "RawTCP.hh"
#include "PortMux.hh"

/* Doesn't handle byte-ordering */
struct Header {
  long packetsize;
  double byterate;
  long numpackets;
};

union SerializedHeader {
  Header h;
  char s[sizeof(Header)];
};
void main(int argc,char *argv[]){
  int bufsize=iMega(8);
  char *buffer = new char[bufsize];
  Timer t;
  PortMux mux;
  RawTCPserver tcpserver(7000);
  UDPstreamServer server(7000); // implict connect
  RawTCPport *control=tcpserver.accept();
  
  // so now we get info about the packetsize from the client
  SerializedHeader header;
  int sz;
  sz= control->read(&(header.s),sizeof(header));
  printf("Header[%d] is packetsize=%u byterate=%g Mbyte/sec numpackets=%u\n",
	 sz,
	 header.h.packetsize,header.h.byterate/(1024.0*1024.0),
	 header.h.numpackets);
  server.setPacketSize(header.h.packetsize);
  server.setByteRate(header.h.byterate);
  printf("Required Packet Delay=%g\n",server.getPacketDelay());
  mux.addInport(&server);
  mux.addInport(control);
  printf("serverport=%u controlport=%u\n",
	 server.getPortNum(),control->getPortNum());
  mux.enableTimeout(0); // disable timeout
  puts("starttime");
  t.reset();
  t.start();
  int i,j;
  for(i=0;i<header.h.numpackets;i++){
    // fprintf(stderr,"p");
    j=0;
    RawPort *p = mux.select();
    //  fprintf(stderr,"s");
    if(p==control){
      puts("breakout!");
      break; // break from loop
    }
    else if(p) // must be server
      j = server.recv(buffer,header.h.packetsize);
    // if(!p) puts("NULLPORT?");
    // if(!j) fprintf(stderr,"-");
    if(!(i%(header.h.numpackets>>4))) fprintf(stderr,".");
  }
  puts("done");
  t.stop();
  t.print();
  puts("got header again");
  sz = control->read(&(header.s),sizeof(header));
  header.h.numpackets=i;
  printf("resend with numpackets=%u\n",i);
  control->write(&(header.s),sizeof(Header));
  puts("done!");
  delete control;
}

#if 0
void main(int argc,char *argv[]){
  int bufsize=iMega(8);
  char *buffer = new char[bufsize];
  Timer t;
  UDPstreamServer server(7000); // implict connect
  server.setPacketSize(1200);
  server.setByteRate(fMega(1.0));
  printf("Required Packet Delay=%g\n",server.getPacketDelay());
  t.reset();
  t.start();
  for(int j=1,i=0,n=8;j>0 && i<10;i++,n+=8){
    j=server.recv(buffer,bufsize);
    if(j<=0) puts("Aborted recv!");
    else { printf("Got %u megs\n",n);}
  }
  t.stop();
  t.print();
}

#endif
