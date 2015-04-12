
#include <stdio.h>
#include <vector>
#include <stack>
#include <queue>
#include <sys/types.h>
#include <unistd.h>
#include "RawUDP.hh"
#include <pthread.h>

struct PacketHeader {
  char type; // can also contain byte order flags
  // pad?
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
  inline uint32_t getSeq(){ return ntohl(hdr->seq);}
  inline void setSeq(uint32_t seq){ hdr->seq = htonl(seq);}
  inline uint32_t getChecksum(){ return ntohl(hdr->checksum);}
  inline void setChecksum(uint32_t cs) { hdr->checksum=htonl(cs);}

  // computeChecksum()
  void init(int size){
    this->packet = new char[size];
    this->data = this->packet+sizeof(PacketHeader);
    this->hdr = (PacketHeader*)this->packet;
    this->csum = (uint32_t*)this->data;
  }
  // fast copyin and fast copyout for data
  // downshift the counter and then copy remainder with if-ladder
  void copyDataIn(char *buffer,size_t sz){    
    length=sz;
    //for(length=0;length<sz;length++) data[length]=buffer[length];
    // should compute checksum here as well
    // do it the stupid way?
    register int i=0,ilast=sz>>2;
    register uint32_t cs=0,*b=(uint32_t*)buffer;
    for(;i<ilast;i++) {
      csum[i]=b[i]; // copy 32-bits at a time
      cs^=csum[i]; // and checksum while we are at it
    }
    i=ilast<<2; // now compute the remainder
    for(;i<4;i++) data[i]=buffer[i]; // should fully unroll
    if(i<sz) data[i]=buffer[i]; else data[i]=0; i++;
    if(i<sz) data[i]=buffer[i]; else data[i]=0; i++;
    if(i<sz) data[i]=buffer[i]; else data[i]=0; i++;
    if(i<sz) data[i]=buffer[i]; else data[i]=0; i++;
    cs^=csum[(i>>2)]; // xor that last word
    this->setChecksum(cs);
  }
  int copyDataOut(char *buffer){
    register int i;
    for(i=0;i<length;i++) buffer[i]=data[i];
  }
};

struct PacketQueues {
  const int maxpacketsize,maxsegsize;
  uint32_t sndseq,localseqmax,localseqgap;
  uint32_t rcvseq,rmtseqmax,rmtseqgap; // gap is min seq that has gap
  // the difference between rseqmax and rseqgap should be less than ooo
  // except of the long gap is due to a resend
  int ooo; // out of order length
  std::stack<PacketInfo *> freeq;  // unused packets
  std::queue<PacketInfo *> sendq; // waiting to get sent (packet pacing)
  std::queue<PacketInfo *> pendq; // awaiting ack
  std::queue<PacketInfo *> recvq; // data awaiting processing
  pthread_mutex_t freelock,sendqlock,pendqlock,recvqlock;
  const RawUDP &udp;
  
  PacketQueues(RawUDP &sock,int maxpktsize=1480,int nfree=1024):udp(sock),maxpacketsize(maxpktsize),maxsegsize(maxpktsize-sizeof(PacketHeader)),sndseq(0),rcvseq(0),ooo(5){
    int i;
    for(i=0;i<nfree;i++){ 
      PacketInfo *info = new PacketInfo;
      info->init(maxpacketsize);
      freeq.push(info);
    }
  }

  void send(char *buffer,int nbytes){
    int offset;
    for(offset=0;offset<nbytes;offset+=maxsegsize){
      if(freeq.empty()){
	// use cond to wait for freelist to increase in size
	// or spinwait
	while(freeq.empty()){} // spinwait
      }
      // get freelock
      PacketInfo *info = freeq.top();
      freeq.pop(); // remove from freelist
      // release freelock
      register int sz=maxsegsize;
      if(sz+offset>nbytes) sz=nbytes-offset;
      info->copyDataIn(buffer+offset,sz);
      // now queue for sending
      sendq.push(info);
    }
  }

  void sendHandler(){
    // got a recvq
    
  }
};

int main(int argc,char *argv[]){
  RawUDP udp("localhost",5000);
  PacketQueues p(udp);
  
}
