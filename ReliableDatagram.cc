#include <stdio.h>
#include "Timer.hh"
#include "ReliableDatagram.hh"

void PacketInfo::init(int size){
  this->packet = new char[size];
  this->data = this->packet+sizeof(PacketHeader);
  this->hdr = (PacketHeader*)this->packet;
  this->csum = (uint32_t*)this->data;
}

PacketInfo::~PacketInfo(){ delete packet; }

  // fast copyin and fast copyout for data
void PacketInfo::computeChecksum(){
  // compute checksum on data and then compare
  // to one computed
  // uint32_t stored_cs = this->getChecksum();
  int sz = this->length - sizeof(PacketHeader);
  int i=0,ilast=sz>>2;
  uint32_t cs=0,*b=(uint32_t*)data;
  for(i=0;i<ilast;i++) {
    cs^=csum[i]; // and checksum while we are at it
  }
  if(sz&0x00000003){
    i=ilast<<2; // now compute the remainder
    printf("sz=%u ilast=%u ilast*4=%u i=%u\n",
	   sz,ilast,ilast*4,i);
    // must ensure trailing edge word of data array is zeroed
    if(i>=sz) { data[i]=0; printf("data[%u]\n",i); } i++;
    if(i>=sz) {data[i]=0;printf("data[%u]\n",i); } i++;
    if(i>=sz) {data[i]=0;printf("data[%u]\n",i); } i++;
    if(i>=sz) {data[i]=0;printf("data[%u]\n",i); } i++;
    cs^=csum[(i>>2)]; // xor that last word
  }
  printf("\ti is now %u csum[i>>2] is %0x\n",i>>2,csum[(i>>2)]);
  this->setChecksum(cs);
  printf("Checksum Computed! %0x\n",cs);
}
int PacketInfo::verifyChecksum(){
  // compute checksum on data and then compare
  // to one computed
  uint32_t stored_cs = this->getChecksum();
  int sz = this->length - sizeof(PacketHeader);
  int i=0,ilast=sz>>2;
  uint32_t cs=0,*b=(uint32_t*)data;
  for(i=0;i<ilast;i++) {
    cs^=csum[i]; // and checksum while we are at it
  }
  if(sz&0x00000003){
    i=ilast<<2; // now compute the remainder
    printf("sz=%u ilast=%u i=%u\n",
	   sz,ilast,i);
    // must ensure trailing edge word of data array is zeroed
    if(i>=sz) data[i]=0; i++;
    if(i>=sz) data[i]=0; i++;
    if(i>=sz) data[i]=0; i++;
    if(i>=sz) data[i]=0; i++;
    cs^=csum[(i>>2)]; // xor that last word
  }
  // if(cs==stored_cs) printf("Checksum Passed! %0x\n",cs);
  // else printf("*****Checksum Failure cs=%0x stored=%0x\n",cs,stored_cs);
  if(cs==stored_cs) return 1;
  else return 0;
}

// downshift the counter and then copy remainder with if-ladder
void PacketInfo::copyDataIn(char *buffer,size_t sz){    
  //for(length=0;length<sz;length++) data[length]=buffer[length];
  // should compute checksum here as well
  // do it the stupid way?
#if 0
  memcpy(data,buffer,sz);
  length = sz+sizeof(PacketHeader);
  this->computeChecksum();
#endif
  int i=0,ilast=sz>>2;
  uint32_t cs=0,*b=(uint32_t*)buffer;
  for(;i<ilast;i++) {
    csum[i]=b[i]; // copy 32-bits at a time
    cs^=csum[i]; // and checksum while we are at it
  }
  if(sz&0x00000003){
    i=ilast<<2; // now compute the remainder
    // for(;i<4;i++) data[i]=buffer[i]; // should fully unroll
    if(i<sz) data[i]=buffer[i]; else data[i]=0; i++;
    if(i<sz) data[i]=buffer[i]; else data[i]=0; i++;
    if(i<sz) data[i]=buffer[i]; else data[i]=0; i++;
    if(i<sz) data[i]=buffer[i]; else data[i]=0; i++;
    cs^=csum[(i>>2)]; // xor that last word
  }
  length = sz+sizeof(PacketHeader);
  this->setChecksum(cs);
}

// downshift the counter and then copy remainder with if-ladder
void PacketInfo::copyDataInNoChecksum(char *buffer,size_t sz){    
  //for(length=0;length<sz;length++) data[length]=buffer[length];
  // should compute checksum here as well
  // do it the stupid way?
  memcpy(data,buffer,sz);
  length = sz+sizeof(PacketHeader);
  this->computeChecksum();

}

int PacketInfo::copyDataOut(char *buffer){
  int i,ilast=length-sizeof(PacketHeader);
  for(i=0;i<ilast;i++) buffer[i]=data[i];
  return ilast;
}

// This is faster than a modulo-based circular buffer because
// it ensures buffer wrap-around by masking the high bits
// rather than by 
void FastCircBuffer::printBinary(uint32_t v){
  int i;
  printf("v=%u\tb=",v);
  for(i=0;i<32;i++){
    if(!(i%4)) printf(" ");
    if((v<<i)&0x80000000) printf("1");
    else printf("0");
  }
}

FastCircBuffer::FastCircBuffer(uint32_t size){
  unsigned int i,maxbit=0;
  // find high bit and then mask off the nitems
  for(i=0;i<32;i++){
    // original version :::  if((size>>i) && 0x00000001) maxbit=i; 
    if(((size>>i) & 0x00000001)) maxbit=i; 
  }
  maxitems=0x00000001;
  maxitems<<=maxbit;
  cbuf=new PacketInfo*[maxitems];
  mask=0;
  for(i=0;i<maxbit;i++) mask|=(0x00000001<<i);
  // OK, now we can use the mask to enforce cirbuf boundaries
  // rather than having to rely on a conditional (much faster!!!)
  nitems=0;
  front=0; back=0;
  //printf("maxitems: ");
  //printBinary(maxitems);
  //printf("\nmask: ");
  //printBinary(mask);
  //printf("\n");
}

FastCircBuffer::~FastCircBuffer(){delete cbuf;}
PacketInfo *FastCircBuffer::removeSeq(uint32_t seq,int &seekback){ 
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
PacketInfo *FastCircBuffer::removeSeq2(uint32_t seq,int &seekback){
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



ReliableDatagram::ReliableDatagram():RawUDP(),control(0),sndseq(0),rcvseq(0),ooo(5),have_return_address(0){
  init();
  // this allows for a delayed "connect"
  // however, we would need to establish our TCP control socket
  // connection before we can establish the returnaddress for
  // communication.
}

//ReliableDatagram::ReliableDatagram(int port):RawUDP(port),sndseq(0),rcvseq(0),ooo(5){
//  init();
//  recvmux.addInport(this);
//}

ReliableDatagram::ReliableDatagram(char *host,int port):RawUDP(),control(0),sndseq(0),rcvseq(0),ooo(5),have_return_address(0),verifychecksum(0){
  control = new RawTCPclient(host,port);
  control->setTCPnoDelay();

  // now negotiate connection specifics via the control socket
  // how do we find IPaddress of the endpoints?
  // getHostname(name,sz) gets us local hostname
  // IPaddress is a private member

  // when I get the IPaddress from RawTCP, I am getting
  // the address of the address endpoint
  control->getIPaddress(this->returnaddress);
  this->have_return_address=1;
  {
    char h[32];
    int pt,i;
    for(i=0;i<32;){
      int r;
      r= control->read(h,32);
      if(r<0){ perror("Failure in socket"); break;}
      i+=r;
    }
    RawUDP::bind(atoi(h)); // bind and reply
    control->write(h,32); // TCP nodelay should ensure this gets out timely
    RawUDP::recv(h,32,this->returnaddress);
    RawUDP::connect(this->returnaddress);
    snprintf(h,sizeof(h),"%u",this->returnaddress.getPort());
    //printf("I got returnaddress is [%s], so I inform the other side\n",h);
    control->write(h,32);
    returnaddress.getHostname(h);
    pt=returnaddress.getPort();
    //printf("ReturnAddress=[%s]:%u\n",h,pt);
  }
  if(0) { // mtu discovery currently disabled
    char buffer[10000];
    int sz;
    // OK, now we do MTU discovery
    sz=RawUDP::recv(buffer,9900);
    printf("Size of packet is %u msg=[%s]\n",sz,buffer);
    sz=RawUDP::recv(buffer,9900);
    printf("Size of packet is %u msg=[%s]\n",sz,buffer);
    sz=RawUDP::recv(buffer,9900);
    printf("Size of packet is %u msg=[%s]\n",sz,buffer);
  }
  init();
}

ReliableDatagram::ReliableDatagram(RawTCPport *s):RawUDP(),control(s),sndseq(0),rcvseq(0),ooo(5),have_return_address(0),verifychecksum(0){
  control->setTCPnoDelay();
  // now negotiate connection specifics via the control socket
  // establish the return address
  control->getIPaddress(this->returnaddress);
  // returnaddress contains the port and host of endpoint
  // but the endpoint UDP is not yet bound to this address
  // se we must tell it to bind via the control channel
  this->have_return_address=1;
  {
    char h[32];
    int pt,i;
    snprintf(h,sizeof(h),"%u",this->returnaddress.getPort());
    //printf("TCP:Write The ReturnAddress to the other end (%s)!\n",h);
    control->write(h,32); // tell the other side to bind
    // wait on control socket to make sure the initial request made it
    for(i=0;i<32;){
      int r;
      r= control->read(h+i,32-i); // make sure we get all 32 bytes
      if(r<0){ perror("Failure in socket"); break;}
      i+=r;
    }
    // printf("TCP:Read: Other side bound itself.  Now I can connect to it\n");
    RawUDP::connect(this->returnaddress); // now we can send
    // printf("UDP:send: Send bogus info to the other side so it can get my retaddr\n");
    RawUDP::send(h,32); // right back atcha via UDP now
    
    // do a select and timeout
    // if nothing after 10 seconds, send again (probably should set type bits
    // so that the packet will be ignored by the packet engine if an extra
    // packet was sent)
    {
      PortMux mux;
      mux.addInport(control);
      mux.setTimeout(10.0); // 10 second timeout
      mux.enableTimeout();
      while(!mux.select()){
	RawUDP::send(h,32);
      }
    }
    // need to do this until control socket says its we got it
    for(i=0;i<32;){
      int r;
      r=control->read(h+i,32-i); // make sure we get all 32 bytes
      if(r<0){ perror("Failure in socket"); break;}
      i+=r;
    }
    pt=atoi(h);
    // printf("TCP:Read: Other side tells me what my source port was so I can bind to [%u]\n",pt);
    // RawUDP::bind(pt); // ok, I won't bother to bind the source port (just replyto)
    returnaddress.getHostname(h);
    pt=returnaddress.getPort();
    //printf("ReturnAddress=[%s]:%u\n",h,pt);
  }
  if(0) { // MTU discovery currently disabled
    char buffer[10000]; // 10k buffer
    // OK, now we do MTU discovery
    snprintf(buffer,sizeof(buffer),"1500");
    RawUDP::send(buffer,1500-20);
    snprintf(buffer,sizeof(buffer),"4000");
    RawUDP::send(buffer,4000-20);
    snprintf(buffer,sizeof(buffer),"9000");
    RawUDP::send(buffer,9000-20);
  }
  init();
}

ReliableDatagram::~ReliableDatagram(){
  if(control && control->isValid()) delete control;
  control=0;
  if(maxsegsize>0) {
    // we need to dump everything
    // and re-init
    while(freeq.size()>0){
      PacketInfo *p = freeq.top();
      freeq.pop();
      delete p;
    }
  }
}

void ReliableDatagram::init(int maxpktsize,int nfree){
  watchforping=0;
  if(maxsegsize>0) {
    // we need to dump everything
    // and re-init
    while(freeq.size()>0){
      PacketInfo *p = freeq.top();
      freeq.pop();
      delete p;
    }
  }
  maxpacketsize=maxpktsize;
  maxsegsize=maxpktsize-sizeof(PacketHeader);
  // why are we blowing all of these structs away
  // and then reallocating?  Because if you don't the
  // heap gets fragmented and the memory layout is 
  // considerably less efficient!
  int i;
  for(i=0;i<nfree;i++){ 
    PacketInfo *info = new PacketInfo;
    info->init(maxpacketsize);
    freeq.push(info);
  }
  recvmux.addInport(this);
  recvmux.enableTimeout(0); // infinite timeout except poll (never timeout)
  // recvmux.setTimeout(-1); // infinite timeout
}

int ReliableDatagram::send(char *buffer,int nbytes){
  int r=0;
  if(freeq.empty()) return -1; // resource unavailable
     // Note that this implies cannot send a 0 length message
     // might want to return -1 for a failure due to resource unavailable
  // get freelock
  PacketInfo *info = freeq.top();
  freeq.pop(); // remove from freelist
  // release freelock
  info->setType(PKT_DATA);
  // we always compute checksum on copyin
  // because we don't know if the other side is going
  // to be verifying it (sad but true)
  // the send is cache-miss limited anyways
  info->copyDataIn(buffer,nbytes);
  info->setSeq(sndseq++);
  // printf("Send packet: len=%u\n",info->length);
  r=RawUDP::send(info->packet,info->length);
  // now queue for acks
  pendq.push(info);
  //printf("And push into pendQ len=%u\n",pendq.size());
  // now check for acks or incoming data
  recvCheck();
  return r;
}

void ReliableDatagram::sendHandler(ReliableDatagram *p){
  // if we are multithreaded
  // must put mutexes around each queue access
  // call recvCheck() continuously and enqueue recvd packets
  // we can intersperse these checks with paced packet output
  // or put the paced packet output into a separate thread
  // however, if packet pacing goes into another thread, there
  // is a danger that lock contention may hurt overall efficiency!
}

int ReliableDatagram::recv(char *buffer,int nbytes){
  // first recvCheck,
  // then, check the recvq
  // do select
  // if return, then process ack
  // otherwise, keep trying
  if(recvq.empty()){
    while(recvq.empty()){ // check until we have something to recv (blocking recv)
      recvmux.wait();
      recvCheck();
      // if the control socket terminated, exit also (return -1)
    }
  }
  else
    recvCheck();
  //else{
  int r;
  // printf("copy out packet data\n");
  PacketInfo *info = recvq.front();
  recvq.pop();
  r=info->copyDataOut(buffer);
  freeq.push(info);
  return r;
  //}
}

int ReliableDatagram::recv(char *buffer,int nbytes,uint32_t &seq){
  // first recvCheck,
  // then, check the recvq
  // do select
  // if return, then process ack
  // otherwise, keep trying
  if(recvq.empty()){
    while(recvq.empty()){
      recvmux.wait();
      recvCheck();
      // if the control socket terminated, exit also (return -1)
    }
  }
  else
    recvCheck();
  
  //else{
  int r;
  // printf("copy out packet data\n");
  PacketInfo *info = recvq.front();
  recvq.pop();
  r=info->copyDataOut(buffer);
  seq=info->getSeq();
  freeq.push(info);
  return r;
  // }
}

int ReliableDatagram::ping(char *buffer,int nbytes,double &timeout){
  // ping is a blocking operation
  // but you can supply a timeout since the ping might not respond
  if(freeq.empty()) return 0; // failure
  PacketInfo *p=freeq.top();
  freeq.pop();
  p->setType(PKT_PING);
  p->hdr->seq=sndseq;
  p->hdr->flags=1; // means we are sending it (and should ping back)
  // a 0 means that it is a ping response
  Timer t;
  t.reset();
  t.start();
  this->watchforping=p;
  // printf("Send ping\n");
  RawUDP::send(p->packet,sizeof(PacketHeader));
  // now we wait for the reply until timeout
  // do recvchecks while we are waiting
  recvmux.enableTimeout();
  do {
    recvmux.setTimeout(timeout-t.realTime());
    recvmux.wait();
    //  printf("RevMux Wait completed or timed out\n");
    recvCheck(); // process other packets
    if(watchforping==0) {
      timeout=t.realTime(); // write back the elapsed time for ping response
      break; // we got the response
    }
  } while(t.realTime()<timeout);
  // removal of the ping packet from watchforping
  // indicates that the ping packet returned successfully
  if(this->watchforping){
    freeq.push(watchforping);
    watchforping=0;
    return 0;
  }
  else return 1;
}

// does not deal properly with aging
// should be an agequeue?
// or should we check resends in here?
// or should OOO be handled in the pendq
// and make pendq a list instead. (walk the list and
// do manual aging (just compare the sequence number to
// the max OOO limit!)
void ReliableDatagram::recvCheck(){
  // ::: Poll for packets and process them accordingly
  // if NAK
  //  requeue and resend
  // else if ACK
  //  remove from pendq
  // printf("recvcheck\n");
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
    info->length=RawUDP::recv(info->packet,maxpacketsize);
    //printf("Got packet len=%u\n",info->length);
    if((info->hdr->type) & (PKT_ACK)){
      int seekback;
      //printf("ACK packet\n");
      PacketInfo *p=pendq.removeSeq(info->getSeq(),seekback);
      // and return it to the freeq
      freeq.push(info);
      if(p){
	freeq.push(p);
	// copy data 
	// JMS: In order to make the OOO work correctly
	// we need to flag each packet by the number of
	// times it is traversed in a seekback operation
	// when the traversal exceeds ooo, then we 
	// should resubmit.  The position in the seekback
	// is not sufficient because we have to accumulate
	// a 5-entry backlock to trigger the resend.
	// If we flag the packet by the number of times
	// it was crossed over, it will do a better job
	// of knowing that we have hit the seekback limit
	// (requires also that we init the seekback info
	// upon insertion into the FastCirBuffer.)
	// for now, we will only allow a seekback of 1 (no reordering)
	// This may  be overly aggressive for networks using
	// Juniper m-series routers given their queue policies...
	// 
	//while(seekback>=this->ooo && pendq.size()>0){
	while(seekback>=1 && pendq.size()>0){
	  // start doing resends of packets that 
	  // are beyond the out-of-order limit
	  // arbitrarily set to 5 here.
	  p = pendq.pop();
	  if(!p) continue;
	  RawUDP::send(p->packet,p->length);
	  pendq.push(p);
	}
      }
    }
    else if((info->hdr->type) & PKT_NAK){
      int seekback;
      // find in pending send in pendq and remove
      // then resend and requeue (change resend counter)
      // and then free up info packet
      //printf("NAK packet\n");
      PacketInfo *p=pendq.removeSeq(info->getSeq(),seekback);
      freeq.push(info);
      if(p){
	RawUDP::send(p->packet,p->length);
	pendq.push(p);
	while(seekback>=1 && pendq.size()>0){
	//while(seekback>=this->ooo && pendq.size()>0){
	  p = pendq.pop();
	  if(!p) continue;
	  RawUDP::send(p->packet,p->length);
	  pendq.push(p);
	}
      }
    }
    else if((info->hdr->type) & PKT_DATA){ // force a resend
      // #define TEST_RELIABLE
      // check first to make sure checksum is OK
      // if checksum is OK, then push into recvq
      // else, must send NAK.
      // got data, put it in the recvq
      // printf("DATA packet\n");
#ifdef TEST_RELIABLE  // #define this to force packet loss for testing purposes
      static int rel=0;
      if(info->hdr->seq==2 && rel==0){ // fake/force unreliability for testing
	info->setType(PKT_NAK);
	RawUDP::send(info->packet,sizeof(PacketHeader));
	freeq.push(info);
	rel=1;
      }
      else {
#endif
	if(this->verifychecksum && !info->verifyChecksum()){
	  info->setType(PKT_NAK);
	  // we flunked the checksum, so we send a NAK
	  RawUDP::send(info->packet,sizeof(PacketHeader));
	  freeq.push(info); // discard this packet
	  break; // lets get outta of here...
	}
	else {
	  recvq.push(info);
	  info->setType(PKT_ACK);
	  // reuse the same packet to send the ack
	  RawUDP::send(info->packet,sizeof(PacketHeader));
	}
	// should defer until we verify the checksum
	info->setType(PKT_DATA); // and set back to data packet
#ifdef TEST_RELIABLE
      }
#endif
    }
    else if((info->hdr->type) & PKT_PING){
      // send packet back to help recvr to calc RTT
      // printf("PKT_PING: \n");
      if(info->hdr->flags) {
	// printf("\tReply to the PING\n");
	info->hdr->flags=0; // set response sequence number to zero
	// probably should use flags instead
	RawUDP::send(info->packet,info->length);
      }
      else {
	// printf("\twe are waiting for a ping to return\n");
	if(watchforping){
	  // printf("\tGot a reply, so inform the ping() that is waiting\n");
	  freeq.push(watchforping);
	  watchforping=0; // announce that the ping returned
	}
      }
      // no ack expected for a PING (its just best-effort)
      freeq.push(info);
    }
    else {
      freeq.push(info); // terurn info to the queue (don't know what to do with it)
      printf("Unknown packet type\n");
    }
    // loss stats?  can stash in the PING packet
  }
  //printf("leaving recvcheck\n");
}

ReliableDatagramServer::ReliableDatagramServer(int port,int q):RawTCPserver(port,q){
 // not much to do here
}

ReliableDatagram *ReliableDatagramServer::accept() {
    // must set up a new ReliableDatagram connection
    // control port is established.  Must bind the 
    // ephemeral port address for the new service
    // for both endpoints
    RawTCPport *sock = RawTCPserver::accept();
    return new ReliableDatagram(sock);
    // on client side, we connect first with TCP control socket
    // after successful connect, we must establish 
    // source and destination ephemeral ports for UDP.
}
