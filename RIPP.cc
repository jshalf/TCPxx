#include <stdio.h>
#include "Timer.hh"
#include "RIPP.hh"


RIPP::RIPP(RawTCPport *control_socket):ReliableDatagram(control_socket),firsttime(1),done(1){
  // set default rate to 10 megabits
  init();
  recvmux.enableTimeout();
  recvmux.setTimeout(2.0); // 1-5 second timeout
  startThreads();
}

RIPP::RIPP(char *host,int port):ReliableDatagram(host,port),firsttime(1),done(1){
  // set default rate to 10 megabits
  init();
  recvmux.enableTimeout();
  recvmux.setTimeout(2.0); // 1-5 second timeout
  startThreads();
}

RIPP::~RIPP(){
  stopThreads();
  if(maxsegsize>0) {
    // we need to dump everything
    // and re-init
    pthread_mutex_lock(&freeqlock);
    while(freeq.size()>0){
      PacketInfo *p = freeq.top();
      freeq.pop();
      delete p;
    }
    pthread_mutex_unlock(&freeqlock);
  }
  pthread_mutex_destroy(&freeqlock);
  pthread_mutex_destroy(&sendqlock);
  pthread_mutex_destroy(&pendqlock);
  pthread_mutex_destroy(&recvqlock);
  pthread_cond_destroy(&sendqcond);
  pthread_cond_destroy(&newdatacond);
  pthread_cond_destroy(&pingcond);
  pthread_cond_destroy(&sendqemptycond);
  pthread_cond_destroy(&pendqemptycond);
  // need a cond to kill thread
}

void RIPP::startThreads(){
  if(!done) return; // already stopped
  done=0;
  pthread_create(&recvthread,0,RIPP::recvHandler,this);
  pthread_create(&sendthread,0,RIPP::sendHandler,this);
}
void RIPP::stopThreads(){
  if(done) return; // already done
  done=1;
  pthread_cond_broadcast(&sendqcond);
  pthread_join(sendthread,0);
  pthread_join(recvthread,0);
}

void RIPP::init(int maxpktsize,int nfree){
  if(firsttime){
    pthread_mutex_init(&freeqlock,0);
    pthread_mutex_init(&sendqlock,0);
    pthread_mutex_init(&pendqlock,0);
    pthread_mutex_init(&recvqlock,0);
    pthread_cond_init(&sendqcond,0);
    pthread_cond_init(&newdatacond,0);
    pthread_cond_init(&pingcond,0);
    pthread_cond_init(&sendqemptycond,0);
    pthread_cond_init(&pendqemptycond,0);
    firsttime=0;
  }
  else { // if firsttime, the superclass already initialized this
    stopThreads();
    ReliableDatagram::init(maxpktsize,nfree);
    startThreads();
  }
}

int RIPP::send(char *buffer,int nbytes){
  if(freeq.empty()) return 0; // resource unavailable (perhaps return -1 so can send 0len packets)
  // get freelock
  pthread_mutex_lock(&freeqlock);
  PacketInfo *info = freeq.top();
  freeq.pop(); // remove from freelist
  pthread_mutex_unlock(&freeqlock);
  // release freelock
  info->copyDataIn(buffer,nbytes);
  info->setSeq(sndseq++);
  info->setType(PKT_DATA);
  // queue instead of sending
  pthread_mutex_lock(&sendqlock);
  sendq.push(info);
  pthread_mutex_unlock(&sendqlock);
  // moved the cond_signal outside of the mutex because
  // the released thread must acquire the lock as its first order
  // of business after being released
  pthread_cond_signal(&sendqcond); // inside or outside of mutex??
  // pthread_mutex_unlock(&sendqlock); 
  // can delay the sends in the foreground
  // rather than multithreading the sends
  // if the threading overhead gets too ridiculous
  return nbytes; // success
}

int RIPP::ping(char *buffer,int nbytes,double &timeout){
  // ping is a blocking operation
  // but you can supply a timeout since the ping might not respond
  if(freeq.empty()) return 0; // failure
  pthread_mutex_lock(&freeqlock);
  PacketInfo *p=freeq.top();
  freeq.pop();
  pthread_mutex_unlock(&freeqlock);
  p->hdr->type=PKT_PING;
  p->hdr->seq=sndseq;
  p->hdr->type=1; // means we are sending it (and should ping back)
  // a 0 means that it is a ping response
  Timer t;
  t.reset();
  t.start();
  this->watchforping=p;
  RawUDP::send(p->packet,sizeof(PacketHeader));
  // now we wait for the reply until timeout
  // do recvchecks while we are waiting
  // argh!  we need to either mutex lock this
  // or we need to have a separate mux
  pthread_mutex_lock(&freeqlock);
  pthread_cond_wait(&pingcond,&freeqlock);
  if(watchforping==0) {
    timeout=t.realTime(); // write back the elapsed time for ping response
    pthread_mutex_unlock(&freeqlock);
    return 0; // we got the response
  }
  else { // still didn't get a response
    freeq.push(watchforping);
    watchforping=0; 
    pthread_mutex_unlock(&freeqlock);
    return 1;
  }
}

void *RIPP::sendHandler(void *v){
  RIPP *p=(RIPP*)v;
  // if we are multithreaded
  // must put mutexes around each queue access
  // call recvCheck() continuously and enqueue recvd packets
  // we can intersperse these checks with paced packet output
  // or put the paced packet output into a separate thread
  // however, if packet pacing goes into another thread, there
  // is a danger that lock contention may hurt overall efficiency!
  p->rate.reset();
  while(!p->isDone()){
    p->sendCheck();
  }
  // use a volatile variable?
  // or use a pthread_cond
  return 0;
}

void RIPP::sendCheck(){
  //  int s=0;
  double plen;
  // put in inter-packet delay
  if(sendq.empty()) {
    pthread_mutex_lock(&sendqlock);
    pthread_cond_wait(&sendqcond,&sendqlock); // wait for new sendq entry
    pthread_mutex_unlock(&sendqlock); // mutex wasre-acquired, so let it go
  }
  // wait (use pthread cond)
  // decide if we should reset or was empty flag
  // rate.newInterval(); // or new interval?
  while(!sendq.empty()) {
    pthread_mutex_lock(&sendqlock);
    register PacketInfo *p=sendq.front();
    sendq.pop();
    pthread_mutex_unlock(&sendqlock);
    // if required, pause for interval in order to pace the packet rate
    //printf("Call rate.pace for packetlength=%u\n",p->length);
    rate.pace(p->length); 
    // here is where to put a select
    // however, that is inadvisable if the packetrate is high
    // (eg.  trying to saturate a GigE)
    // busy wait if nanosleep unavailable or time is too short
    // printf("Send [%s] len=%u byterate=%f\n",p->data,p->length,rate.getByteRate());
    RawUDP::send(p->packet,p->length); // should probably
    // double check returnval to make sure the send doesn't fail here!!!
    pthread_mutex_lock(&freeqlock);
    freeq.push(p);
    pthread_mutex_unlock(&freeqlock);
  }
  // sendq must be empty, so lets tell everyone about it
  pthread_cond_signal(&sendqemptycond);
}

int RIPP::recv(char *buffer,int nbytes){
  // first recvCheck,
  // then, check the recvq
  // do select
  // if return, then process ack
  // otherwise, keep trying
  if(recvq.empty()) return 0; // nonblocking semantics
  // while(recvq.empty()){} // blocking semantics return 0;
  // should wait for a data recv (use a cond?)
 
  //  if(recvq.empty()){
  //  pthread_mutex_lock(&recvqlock);
  //  if(recvq.empty()) pthread_cond_wait(&newdatacond,&recvqlock);
  //  pthread_mutex_unlock(&recvqlock);
  //  }

  {
    int r;
    pthread_mutex_lock(&recvqlock);
    PacketInfo *info = recvq.front();
    recvq.pop();
    pthread_mutex_unlock(&recvqlock);
    r=info->copyDataOut(buffer);
    pthread_mutex_lock(&freeqlock);
    freeq.push(info);
    pthread_mutex_unlock(&freeqlock);
    return r;
  }
}
int RIPP::recv(char *buffer,int nbytes,uint32_t &seq){
  // first recvCheck,
  // then, check the recvq
  // do select
  // if return, then process ack
  // otherwise, keep trying
  seq=0;
  if(recvq.empty()) return 0; // nonblocking semantics
  // while(recvq.empty()){} // blocking semantics return 0;
  // otherwise use cond for blocking semantics
  //  if(recvq.empty()){
  //  pthread_mutex_lock(&recvqlock);
  //  if(recvq.empty()) pthread_cond_wait(&newdatacond,&recvqlock);
  // cond only releases if new data is released...
  //  pthread_mutex_unlock(&recvqlock);
  //  }
  else{
    int r;
    pthread_mutex_lock(&recvqlock);
    PacketInfo *info = recvq.front();
    recvq.pop();
    pthread_mutex_unlock(&recvqlock);
    r=info->copyDataOut(buffer);
    seq=info->getSeq();
    pthread_mutex_lock(&freeqlock);
    freeq.push(info);
    pthread_mutex_unlock(&freeqlock);
    return r;
  }
}

void *RIPP::recvHandler(void *v){
  RIPP *p=(RIPP*)v;
  // if we are multithreaded
  // must put mutexes around each queue access
  // call recvCheck() continuously and enqueue recvd packets
  // we can intersperse these checks with paced packet output
  // or put the paced packet output into a separate thread
  // however, if packet pacing goes into another thread, there
  // is a danger that lock contention may hurt overall efficiency!
  while(!p->isDone()) {
    p->recvCheck(); // should check for exit condition
  }
  // use a volatile variable?
  // or use a pthread_cond
  return 0;
}

// does not deal properly with aging
// should be an agequeue?
// or should we check resends in here?
// or should OOO be handled in the pendq
// and make pendq a list instead. (walk the list and
// do manual aging (just compare the sequence number to
// the max OOO limit!)
void RIPP::recvCheck(){
  // ::: Poll for packets and process them accordingly
  // if NAK
  //  requeue and resend
  // else if ACK
  //  remove from pendq
  // recvmux.enableTimeout(0);
  // actually, perhaps we do need a timeout here
  // in order to catch the "stopthreads" request.
  while(recvmux.wait()){ // wait for a finite amount of time
    // just in case we get a request to stop the threads
    // on timeout, the recvmux returns a NULL which will break
    // out of this loop so that isDone() can be checked

    if(freeq.empty() || this->isDone()){ // its OK if this is unreliable
      // if this is a data packet, we are so screwed!
      // either expand the size of freeq or exit the recvcheck
      break; // hopefully the there is something in recvq to grab
    }
    // get the data
    pthread_mutex_lock(&freeqlock);
    // maybe have a freeq cond?
    // otherwise there is a potential race condition here
    PacketInfo *info = freeq.top();
    freeq.pop();
    pthread_mutex_unlock(&freeqlock);
    info->length=RawUDP::recv(info->packet,maxpacketsize);
    if((info->hdr->type) & (PKT_ACK)){
      int seekback;
      pthread_mutex_lock(&pendqlock);
      PacketInfo *p=pendq.removeSeq(info->getSeq(),seekback);
      pthread_mutex_unlock(&pendqlock);
      // and return it to the freeq
      pthread_mutex_lock(&freeqlock);
      freeq.push(info);
      if(p) freeq.push(p);
      pthread_mutex_unlock(&freeqlock);;
      if(p){
	// copy data 
	//	while(seekback>=this->ooo && pendq.size()>0){
	while(seekback>=1 && pendq.size()>0){
	  // start doing resends of packets that 
	  // are beyond the out-of-order limit
	  // arbitrarily set to 5 here.
	  pthread_mutex_lock(&pendqlock);
	  p = pendq.pop();
	  if(p) {
	    pendq.push(p);
	    pthread_mutex_unlock(&pendqlock);
	  // or should we queue to send
	    RawUDP::send(p->packet,p->length);
	  }
	  else pthread_mutex_unlock(&pendqlock);
	}
      }
    }
    else if((info->hdr->type) & PKT_NAK){
      int seekback;
      // find in pending send in pendq and remove
      // then resend and requeue (change resend counter)
      // and then free up info packet
      pthread_mutex_lock(&pendqlock);
      PacketInfo *p=pendq.removeSeq(info->getSeq(),seekback);
      if(p){
	pendq.push(p);
	pthread_mutex_unlock(&pendqlock);
	RawUDP::send(p->packet,p->length);
      }
      else pthread_mutex_unlock(&pendqlock);
      pthread_mutex_lock(&freeqlock);
      freeq.push(info);
      pthread_mutex_unlock(&freeqlock);
      if(p){
	while(seekback>=1 && pendq.size()>0){
	//while(seekback>=this->ooo && pendq.size()>0){
	  pthread_mutex_lock(&pendqlock);
	  p = pendq.pop();
	  if(p) {
	    pendq.push(p);
	    pthread_mutex_unlock(&pendqlock);
	  // or should we queue to send
	    RawUDP::send(p->packet,p->length);
	  }
	  else pthread_mutex_unlock(&pendqlock);
	}
      }
    }
    else if((info->hdr->type) & PKT_DATA){
      // check first to make sure checksum is OK
      // if checksum is OK, then push into recvq
      // else, must send NAK.
      // got data, put it in the recvq
      if(this->verifychecksum && !info->verifyChecksum()){
	info->setType(PKT_NAK);
	// we flunked the checksum, so we send a NAK
	RawUDP::send(info->packet,sizeof(PacketHeader));
	pthread_mutex_lock(&freeqlock);
	freeq.push(info); // discard this packet
	pthread_mutex_unlock(&freeqlock);
	break; // lets get outta of here...
      }
      else {
	info->setType(PKT_ACK); // send the ack for the data
	RawUDP::send(info->packet,sizeof(PacketHeader));
	info->setType(PKT_DATA); // set it back to data
	pthread_mutex_lock(&recvqlock);
	recvq.push(info); // and enqueue it for recv() call
	pthread_mutex_unlock(&recvqlock);
	// we can put a cond here to unblock the recv
	pthread_cond_signal(&newdatacond);
      }
    }
    else if((info->hdr->type) & PKT_PING){
      // send packet back to help recvr to calc RTT
      if(info->hdr->flags){
	info->hdr->flags=0; // set response sequence number to zero
	// probably should use flags instead
	RawUDP::send(info->packet,info->length);
      }
      else {
	if(watchforping){
	  pthread_mutex_lock(&freeqlock);
	  freeq.push(watchforping);
	  watchforping=0;
	  pthread_mutex_unlock(&freeqlock);
	  pthread_cond_signal(&pingcond); // release blocked ping wait
	}
      }
      // no ack expected for a PING (its just best-effort)
      pthread_mutex_lock(&freeqlock);
      freeq.push(info);
      pthread_mutex_unlock(&freeqlock);
    }
    else {
      printf("Unknown packet type\n");
      pthread_mutex_lock(&freeqlock);
      freeq.push(info);
      pthread_mutex_unlock(&freeqlock);
    }
    // loss stats?  can stash in the PING packet
    if(pendq.empty()){
      pthread_cond_broadcast(&pendqemptycond);
    }
  }
  if(pendq.empty()){
    pthread_cond_broadcast(&pendqemptycond);
  }
}

RIPPServer::RIPPServer(int port,int q):RawTCPserver(port,q){
 // not much to do here
}

RIPP *RIPPServer::accept() {
    // must set up a new RIPP connection
    // control port is established.  Must bind the 
    // ephemeral port address for the new service
    // for both endpoints
    RawTCPport *sock = RawTCPserver::accept();
    return new RIPP(sock);
    // on client side, we connect first with TCP control socket
    // after successful connect, we must establish 
    // source and destination ephemeral ports for UDP.
}
