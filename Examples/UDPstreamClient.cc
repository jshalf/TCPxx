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
  int numpackets;
  int numiter = 100;
  int bufsize=iMega(8);
  char *buffer = new char[bufsize];
  Timer t;
  RawTCPclient control("localhost",7000);
  UDPstreamClient client("localhost",7000); // implict connect
  client.setPacketSize(1200);
  client.setByteRate(fMega(50.0));
  
  SerializedHeader header;
  header.h.packetsize=client.getPacketSize();
  header.h.byterate=client.getByteRate();
  numpackets=header.h.numpackets=(numiter*iMega(8))/header.h.packetsize;
  control.write(&(header.s),sizeof(Header));
  printf("Required Packet Delay=%g\n",client.getPacketDelay());
  t.reset();
  t.start();
  for(int j=1,i=0;j>0 && i<numiter;i++){
    j=client.send(buffer,bufsize);
    if(j<=0) {puts("Aborted Send!"); break;}
  }
  t.stop();
  t.print();
  control.write(&(header.s),sizeof(Header)); // informs server of termination
  control.read(&(header.s),sizeof(Header)); // get loss results from server
  printf("PacketLoss  sent:%u arrived=%u lost=%u losspercent=%g\n",
	 numpackets,header.h.numpackets,numpackets-header.h.numpackets,
	 ((double)(numpackets-header.h.numpackets))*100.0/((double)numpackets));
}

#if 0
void main(int argc,char *argv[]){
  int bufsize=iMega(8);
  char *buffer = new char[bufsize];
  Timer t;
  UDPstreamClient client("localhost",7000); // implict connect
  client.setPacketSize(1200);
  client.setByteRate(fMega(1.0));
  printf("Required Packet Delay=%g\n",client.getPacketDelay());
  t.reset();
  t.start();
  for(int j=1,i=0;j>0 && i<10;i++){
    j=client.send(buffer,bufsize);
    if(j<=0) {puts("Aborted Send!"); break;}
  }
  t.stop();
  t.print();
}
#endif
