#include <stdio.h>
#include <stack>
#include <queue>
#include <sys/types.h>
#include <unistd.h>
#include "RawUDP.hh"
#include "RawTCP.hh"
#include "PortMux.hh"
//#include <pthread.h>
/*
  1) Make sure the send/recv is returning the correct payload size (strip hdr)
  2) Return max packet size allowable (need to offer an integrated MTU test
  3) Need to implement an Accept() method for the server variety of socket
    this will init the return socket with a connect to the ephemeral
    port that was used by the client that connected on the other side.

    The connecting client needs to send several packets announcing
    its desire to connect.  The accept must eat redundant requests
    or we use TCP for the accept. We only instantiate the TCP for
    control/accept/die management.  Otherwise, it is unused unless we
    want to use it for the ACK streams (or NAK streams).
*/

#define PKT_ACK 0x01
#define PKT_NAK 0x02
#define PKT_DATA 0x04
#define PKT_DIE 0x08
#define PKT_PING 0x10
// PING packets contain a timestamp
// the recvr must immediately send the PING packet back

struct PacketHeader {
  char type; // can also contain byte order flags
  char flags; // or potentially circuit ID for demux
  uint32_t seq;
  uint32_t checksum;
  // uint32_t offset;
};

struct PacketInfo {
  char *packet;
  // PacketHeader *hdr; // points into packet struct
  char *data; // points into the packet struct for data
  PacketHeader *hdr; // points to header part of datastruct
  uint32_t *csum; // utility pointer for checksums
  int length; // returned by the recv op or size to output
  PacketInfo():packet(0),hdr(0),data(0){}
  ~PacketInfo();
  inline uint32_t getSeq(){ return ntohl(hdr->seq);}
  inline void setSeq(uint32_t seq){ hdr->seq = htonl(seq);}
  inline uint32_t getChecksum(){ return ntohl(hdr->checksum);}
  inline void setChecksum(uint32_t cs) { hdr->checksum=htonl(cs);}
  void setType(char t){hdr->type=t;}
    // compute checksum on data and then compare
    // to one computed
  void computeChecksum();
  int verifyChecksum();
  void init(int size);
  // fast copyin and fast copyout for data
  // downshift the counter and then copy remainder with if-ladder
  void copyDataIn(char *buffer,size_t sz);
  void copyDataInNoChecksum(char *buffer,size_t sz);
  int copyDataOut(char *buffer);
};

// This is faster than a modulo-based circular buffer because
// it ensures buffer wrap-around by masking the high bits
// rather than by computing the wrap-around using modulo 
// (mod is super slow!)
class FastCircBuffer {
  PacketInfo **cbuf;
  uint32_t mask,nitems,maxitems; // must compute the mask for this
  uint32_t front,back; // insert back.  pop off of the front

  void printBinary(uint32_t v);
  inline uint32_t realOffset(uint32_t idx){return mask&idx;}
public:
  FastCircBuffer(uint32_t size=4096);
  ~FastCircBuffer();
  inline uint32_t size(){return nitems;}
  inline int empty(){if(nitems<=0) return 1; else return 0;}
  inline int push(PacketInfo *p){
    // push returns #items (>0) if success
    // otherwise 0 means failure
    if(nitems>=(maxitems-1)) return 0; // danger
    else cbuf[back]=p;
    back=realOffset(back+1);
    nitems++;
    return nitems;
  }
  inline PacketInfo *pop(){
    PacketInfo *p=0;
    if(nitems>0){
      p=cbuf[front];
      cbuf[front]=0; // zero this entry out
      front=realOffset(front+1);
      nitems--;
    }
    return p;
  }
  inline PacketInfo *top(){ // returning a NULL is not fatal!
    // PacketInfo *p=0;
    if(nitems>0) return cbuf[front];
    else return 0;
  }
  // The std removeSeq allows gaps to remain in the circular
  // buffer.  This allows detection of out-of-order ACKs
  // without requiring timestamps.
  PacketInfo *removeSeq(uint32_t seq,int &seekback);

  // scan backwards to find a seq number
  // return the seekback length *and* the packetinfo
  // return null if that seq# is not enqueued
  // recopies entries
  // do a backwards scan for a particular sequence number
  // and repack the buffers
  PacketInfo *removeSeq2(uint32_t seq,int &seekback);
};


class TCPXX_API ReliableDatagram : public RawUDP {
protected:
  RawTCPport *control; // control interface
  // if we initialize ReliableDatagram with host and port, 
  // then it gets initialized as a client TCP socket.
  
  // if we initialize ReliableDatagram with port bind addr
  // then TCP get initialized as a server sock
  int have_return_address; // initialized to 0
  // but it gets filled out with a proper value
  // if we call "connect" or get a packet from 
  // the other side that contains a returnaddr.
  IPaddress returnaddress; // must figure out
  // the address of our partner
  // and stash it in here.
  // we only do this if we were initialized with only
  // the local binding.

  // otherwise, we can fill out the returnaddress
  // automagically upon construction.
  // once we know the returnaddress, we can go ahead
  // and do a "connect" to the returnaddress (so maybe
  // we don't really need to carry it around like this)
  int maxpacketsize,maxsegsize;
  uint32_t sndseq,localseqmax,localseqgap;
  uint32_t rcvseq,rmtseqmax,rmtseqgap; // gap is min seq that has gap
  // the difference between rseqmax and rseqgap should be less than ooo
  // except of the long gap is due to a resend
  int ooo; // out of order length
  int verifychecksum;
  PacketInfo *watchforping; // nonzero if a ping is in progress
                            // set back to zero when the ping completes
                            // might want to store loss stats in payload
  std::stack<PacketInfo *> freeq;  // unused packets
  std::queue<PacketInfo *> sendq; // waiting to get sent (packet pacing)
  // std::queue<PacketInfo *> pendq; // awaiting ack
  FastCircBuffer pendq;  // std queue was too #$%^ slow!
  std::queue<PacketInfo *> recvq; // data awaiting processing
  PortMux recvmux;

  // pthread_mutex_t freelock,sendqlock,pendqlock,recvqlock;
  
    // if we are multithreaded
    // must put mutexes around each queue access
    // call recvCheck() continuously and enqueue recvd packets
    // we can intersperse these checks with paced packet output
    // or put the paced packet output into a separate thread
    // however, if packet pacing goes into another thread, there
    // is a danger that lock contention may hurt overall efficiency!
  static void sendHandler(ReliableDatagram *p);
  static void recvHandler(ReliableDatagram *p){} // placeholder
public:
  // ReliableDatagram();
  // ReliableDatagram(int port);
  ReliableDatagram(char *host,int port);  // presume we are the client
protected:
  ReliableDatagram();
  ReliableDatagram(RawTCPport *s); // hand it the control socket to negotiate the connection.  This is for a server "accept"ed socket.
public:
  virtual ~ReliableDatagram();
  inline void setOOO(int o){ ooo=o; }
  inline int getOOO() { return ooo;}
  inline void verifyChecksums(int yesval=1){verifychecksum=yesval;}
  void init(int maxpktsize=1480,int nfree=1024);
  virtual int send(char *buffer,int nbytes);

  virtual int recv(char *buffer,int nbytes);
  virtual int recv(char *buffer,int nbytes,uint32_t &seq);
  // ping and expect a response withing a certain timelimit or die
  virtual int ping(char *buffer,int nbytes,double &timeout);
  inline int check(){
    recvCheck();
    return pendq.size();
  }
  inline void waitForPending(){
    if(pendq.size()>0){
      do {
	recvmux.wait();
	recvCheck();
      } while(pendq.size());
    }
  }
  // if(recvmux.poll()) { recvCheck(); return 1;} else return 0; }
    // first recvCheck,
    // then, check the recvq
    // do select
    // if return, then process ack
    // otherwise, keep trying
  // does not deal properly with aging
  // should be an agequeue?
  // or should we check resends in here?
  // or should OOO be handled in the pendq
  // and make pendq a list instead. (walk the list and
  // do manual aging (just compare the sequence number to
  // the max OOO limit!)
protected:
  // ::: Poll for packets and process them accordingly
  // if NAK
  //  requeue and resend
  // else if ACK
  //  remove from pendq
  
  // if we are in multithreaded mode, we should wait instead of poll
  // this can be done in the handler though
  void recvCheck();

public: // stuff carried forward from RawUDP
  int connect(){return RawUDP::connect();}
  int connect(IPaddress &addr){return RawUDP::connect(addr);}
  int connect(char *host,int port){return RawUDP::connect(host,port);}
  virtual int bind(){ return RawUDP::bind();}
  virtual int bind(int pt){return RawUDP::bind(pt);}
  virtual int write(void *buffer,int bufsize){
    return this->send((char*)buffer,bufsize);
  }
  virtual int read(void *buffer,int bufsize){
    return this->recv((char*)buffer,bufsize);
  }
/*
  virtual int write(void *buffer,int bufsize,char *hostname,int prt){
    return ::write(prt,buffer,bufsize);
  } */
  // The 'flgs' argument is just passing on standard
  // flags to the OS ::recv() call.  This needs to be
  // made more intuitive.... (right now its there to match
  // the functionality of the underlying system calls).
private: // disable
  virtual int recv(char *buffer,int bufsize,int flgs){
    return 0; // disable
  }
  // perhaps this should be for the UDP server?
  virtual int recv(char *buffer,int bufsize,
		   IPaddress &from,int flgs=-1){
    return 0; // disable
  }
  // disable
  virtual int send(char *buffer,int bufsize,
		   IPaddress &to,int flgs){
    return 0;
  }
  virtual int send(char *buffer,int bufsize,int flgs){
    return 0;
  }
  friend class ReliableDatagramServer;
};

class ReliableDatagramServer : public RawTCPserver {
public:
  ReliableDatagramServer(int port,int q=5);
  // ~ReliableDatagramServer(){}// nothing to do here as well
  ReliableDatagram *accept();
};
