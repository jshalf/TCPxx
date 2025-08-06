
#include <stdio.h>
#include <vector>
#include <stack>
#include <queue>
#include <list>
#include <sys/types.h>
#include <unistd.h>
#include "RawUDP.hh"
#include "PortMux.hh"
#include <pthread.h>

#define PKT_ACK 0x01
#define PKT_NAK 0x02
#define PKT_DATA 0x04
#define PKT_DIE 0x08
#define PKT_PING 0x10
// PING packets contain a timestamp
// the recvr must immediately send the PING packet back

struct PacketHeader {
  char type; // can also contain byte order flags
  char flags;
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
    //for(length=0;length<sz;length++) data[length]=buffer[length];
    // should compute checksum here as well
    // do it the stupid way?
    int i=0,ilast=sz>>2;
    uint32_t cs=0,*b=(uint32_t*)buffer;
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
    length = sz+sizeof(PacketHeader);
    this->setChecksum(cs);
  }
  int copyDataOut(char *buffer){
    int i,ilast=length-sizeof(PacketHeader);
    for(i=0;i<ilast;i++) buffer[i]=data[i];
    return ilast;
  }
};

// This is faster than a modulo-based circular buffer because
// it ensures buffer wrap-around by masking the high bits
// rather than by 
class FastCircBuffer {
  PacketInfo **cbuf;
  uint32_t mask,nitems,maxitems; // must compute the mask for this
  uint32_t front,back; // insert back.  pop off of the front

  void printBinary(uint32_t v){
    int i;
    printf("v=%u\tb=",v);
    for(i=0;i<32;i++){
      if(!(i%4)) printf(" ");
      if((v<<i)&0x80000000) printf("1");
      else printf("0");
    }
  }
  inline uint32_t realOffset(uint32_t idx){return mask&idx;}
public:
  FastCircBuffer(uint32_t size=4096){
    unsigned int i,maxbit=0;
    // find high bit and then mask off the nitems
    for(i=0;i<32;i++){
      if((size>>i)&&0x00000001) maxbit=i; 
    }
    maxitems=0x00000001;
    maxitems<<=maxbit;
    cbuf=new PacketInfo*[maxitems];
    mask=0;
    for(i=0;i<maxbit;i++) maxbit|=0x00000001;
    // OK, now we can use the mask to enforce cirbuf boundaries
    // rather than having to rely on a conditional (much faster!!!)
    nitems=0;
    front=0; back=0;
    printf("maxitems=");
    printBinary(maxitems);
    printf("\nmask=");
    printBinary(mask);
    printf("\n");
  }
  ~FastCircBuffer(){delete cbuf;}
  inline uint32_t size(){return nitems;}
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
    PacketInfo *p=0;
    if(nitems>0) return cbuf[front];
    else return 0;
  }
  PacketInfo *removeSeq(uint32_t seq,int &seekback){ 
    uint32_t i;
    for(i=0;i<nitems;i++){
      uint32_t idx = realOffset(i+front);
      PacketInfo *p=cbuf[idx];
      if(!p) continue; // skip null items
      if(seq==p->getSeq()){ // is this the seq we are looking for??
	// now we do removal
	uint32_t j;
	idx=front;
	p=cbuf[idx];
	cbuf[idx]=0; // store a null
	// OK, now P contains our target
	pop(); // remove from front
	seekback=i;
	return p;
      }
    }
    // we didn't find anything
    seekback=0;
    return 0;
  }
  // do a backwards scan for a particular sequence number
  // and repack the buffers
  PacketInfo *removeSeq2(uint32_t seq,int &seekback){
    // scan backwards to find a seq number
    // return the seekback length *and* the packetinfo
    // return null if that seq# is not enqueued
    // will need to recopy entries
    uint32_t i;
    for(i=0;i<nitems;i++){
      uint32_t idx = realOffset(i+front);
      PacketInfo *p=cbuf[idx];
      if(seq==p->getSeq()){ // is this the seq we are looking for??
	// now we do removal
	uint32_t j;
	idx=front;
	p=cbuf[idx];
	for(j=0;j<i;j++){
      uint32_t idxp = realOffset(j+front+1);
	  p=cbuf[idxp];
	  cbuf[idxp]=cbuf[idx];
	  idx=idxp;
	}
	// OK, now P contains our target
	pop(); // remove from front
	seekback=i;
	return p;
      }
    }
    // we didn't find anything
    seekback=0;
    return 0;
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
  // std::queue<PacketInfo *> pendq; // awaiting ack
  FastCircBuffer pendq;  // std queue was too #$%^ slow!
  std::queue<PacketInfo *> recvq; // data awaiting processing
  pthread_mutex_t freelock,sendqlock,pendqlock,recvqlock;
  PortMux recvmux;
  RawUDP &udp;
  
  PacketQueues(RawUDP &sock,int maxpktsize=1480,int nfree=1024):udp(sock),maxpacketsize(maxpktsize),maxsegsize(maxpktsize-sizeof(PacketHeader)),sndseq(0),rcvseq(0),ooo(5){
    int i;
    for(i=0;i<nfree;i++){ 
      PacketInfo *info = new PacketInfo;
      info->init(maxpacketsize);
      freeq.push(info);
    }
    recvmux.addInport((RawPort*)(&udp));
  }

  int send(char *buffer,int nbytes){
    if(freeq.empty()) return 0; // resource unavailable
    // get freelock
    PacketInfo *info = freeq.top();
    freeq.pop(); // remove from freelist
    // release freelock
    info->copyDataIn(buffer,nbytes);
    udp.send(info->packet,info->length);
    // now queue for acks
    pendq.push(info);
    // now check for acks or incoming data
    recvCheck();
  }

  static void sendHandler(PacketQueues *p){
    // if we are multithreaded
    // must put mutexes around each queue access
    // call recvCheck() continuously and enqueue recvd packets
    // we can intersperse these checks with paced packet output
    // or put the paced packet output into a separate thread
    // however, if packet pacing goes into another thread, there
    // is a danger that lock contention may hurt overall efficiency!
  }

  int recv(char *buffer,int nbytes){
    // first recvCheck,
    // then, check the recvq
    // do select
    // if return, then process ack
    // otherwise, keep trying
    recvCheck();
    if(recvq.empty()) return 0;
    else{
      int r;
      PacketInfo *info = recvq.front();
      recvq.pop();
      r=info->copyDataOut(buffer);
      freeq.push(info);
      return r;
    }
  }
  
  // does not deal properly with aging
  // should be an agequeue?
  // or should we check resends in here?
  // or should OOO be handled in the pendq
  // and make pendq a list instead. (walk the list and
  // do manual aging (just compare the sequence number to
  // the max OOO limit!)
  
  void recvCheck(){
    // ::: Poll for packets and process them accordingly
    // if NAK
    //  requeue and resend
    // else if ACK
    //  remove from pendq

    // if we are in multithreaded mode, we should wait instead of poll
    // this can be done in the handler though
    while(recvmux.poll()){ // do instantaneous check for data
      // get the data
      if(freeq.empty()){
	// if this is a data packet, we are so screwed!
	// either expand the size of freeq or exit the recvcheck
	return; // hopefully the there is something in recvq to grab
      }
      PacketInfo *info = freeq.top();
      freeq.pop();
      info->length=udp.recv(info->packet,maxpacketsize);
      if((info->hdr->type) & (PKT_ACK)){
	int seekback;
	PacketInfo *p=pendq.removeSeq(info->getSeq(),seekback);
	// and return it to the freeq
	freeq.push(info);
	if(p){
	  freeq.push(p);
	  // copy data 
	  while(seekback>=this->ooo && pendq.size()>0){
	    // start doing resends of packets that 
	    // are beyond the out-of-order limit
	    // arbitrarily set to 5 here.
	    p = pendq.pop();
	    if(!p) continue;
	    udp.send(p->packet,p->length);
	    pendq.push(p);
	  }
	}
      }
      else if((info->hdr->type) & PKT_NAK){
	int seekback;
	// find in pending send in pendq and remove
	// then resend and requeue (change resend counter)
	// and then free up info packet
	PacketInfo *p=pendq.removeSeq(info->getSeq(),seekback);
	freeq.push(info);
	if(p){
	  udp.send(p->packet,p->length);
	  pendq.push(p);
	  while(seekback>=this->ooo && pendq.size()>0){
	    p = pendq.pop();
	    if(!p) continue;
	    udp.send(p->packet,p->length);
	    pendq.push(p);
	  }
	}
      }
      else if((info->hdr->type) & PKT_DATA){
	// check first to make sure checksum is OK
	// if checksum is OK, then push into recvq
	// else, must send NAK.
	// got data, put it in the recvq
	recvq.push(info);
      }
      else if((info->hdr->type) & PKT_PING){
	// send packet back to help recvr to calc RTT
	udp.send(info->packet,info->length);
	// no ack expected for a PING (its just best-effort)
	freeq.push(info);
      }
      // loss stats?  can stash in the PING packet
    }
  }
};

int main(int argc,char *argv[]){
  RawUDP udp("localhost",5000);
  PacketQueues p(udp);
  
}
