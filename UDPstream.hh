#ifndef __UDPSTREAM_H_
#define __UDPSTREAM_H_

/*
  This will implement the basis for the TCP-unfriendly protocol
  Brian Toonen, John Shalf
  ANL/NCSA, 1997.
 */
#include "RawUDP.hh"
#include "Delay.hh"
#include "Timer.hh"
#ifndef fMega
#define iMega(x) (x*1024*1024)
#define fMega(x) (x*1024.0*1024.0)
#endif
#ifndef fKilo
#define iKilo(x) (x*1024)
#define fKilo(x) (x*1024.0)
#endif

class UDPstream : public RawUDP {
private:
  long packetsize; // each packet is fixed size (copy into page-aligned buf)
  int mtu; // discovered
  double packetrate; // tied to packetdelay
  HybridDelay packetdelay; // delay for constant rate sending
  HybridDelay shortdelay; // just to handle ENOBUF errors
  double actualbyterate; // measured byte rate
  Timer packettimer;  // used to get measurement of byterate and packetrate
public:
  UDPstream(int pt);
  UDPstream(char *host,int pt);
  double getPacketRate(){return packetrate;}
  double getPacketDelay(){return (packetrate>0.0)?(1.0/packetrate):0.0;}
  double getByteRate() { return ((double)packetsize)*packetrate;}
  // get bit rate
  long getPacketSize() { return packetsize; }

  // breaks the buffer up into separate packets
  virtual int write(char *buffer,int size) { return this->send(buffer,size); }
  virtual int send(char *buffer,int size,
		   int flgs=-1);
  virtual int send(char *buffer,int size,
		   IPaddress &to,int flgs=-1);
  // packetdelay.reset() at start of ech send
  // virtual int recv(char *buffer,int size); // recv rate doesn't need delay

  // must get these one at a time
  virtual int recv(char *buffer,int bufsize,IPaddress &from,int flgs=-1);
  virtual int recv(char *buffer,int bufsize,int flgs=-1);
  virtual inline int read(void *buffer,int bufsize){
    return this->recv((char *)buffer,bufsize);
  }
  int getMTU();
  void setPacketRate(double packetspersecond);
  void setByteRate(double bytespersecond);
  // set bit rate
  void setPacketDelay(double seconds);
  void setPacketSize(long numbytes);// must recalculate packet delay
  // now we allow setting of timing requirments
};

class UDPstreamServer : public UDPstream {
private:
  inline virtual int bind() {return RawUDP::bind();}
public:
  UDPstreamServer(int pt):UDPstream(pt){ this->bind(); }
};

class UDPstreamClient : public UDPstream {
private:
  inline virtual int connect() {return RawUDP::connect();}
public:
  UDPstreamClient(char *h,int pt):UDPstream(h,pt){ this->connect(); }
};

#endif
