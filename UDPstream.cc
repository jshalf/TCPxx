#include <stdio.h>
#include <stdlib.h>
#include "UDPstream.hh"

#ifdef WIN32
#include <windows.h>
#define SOCKERR(x) (x==SOCKET_ERROR)
#define ENOBUFS WSAENOBUFS
#else
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include <stack>
#include <queue>
#include <unistd.h>
#define SOCKERR(x) (x<0)
#endif
//============= Packet Info ==============
struct PacketHeader {
  char type; // can also contain byte order flags
  char seq; // just for local ordering constraints
  uint32_t offset;
  uint32_t checksum;
};

struct PacketInfo {
  char *packet;
  // PacketHeader *hdr; // points into packet struct
  char *data; // points into the packet struct for data
  PacketHeader *hdr; // points to header part of datastruct
  int length; // returned by the recv op or size to output
  void decodeHdr(){
    hdr->offset=ntohl(hdr->offset);
    // compute a checksum and compare to normalized hdr->checksum (zero extra bytes)
    hdr->checksum=ntohl(hdr->checksum);
  }
  void encodeHdr(int ndatabytes){
    hdr->offset=htonl(hdr->offset);
    // compute a checksum and place into hdr->checksum (zero extra bytes)
    // so that we can do extra unrolling on the checksum loop
    hdr->checksum=htonl(hdr->checksum);
  }
  int encodeChecksum(){
    // zero extra bytes
    return 0;
  }
  int decodeChecksum(){
    return 0;
  }
};

//============= Packet Queues ============
/*
  Thinking in terms of circ buffers
  
  send: send immediate if possible, otherwise add the send queue

  sendthread: do paced sends.
    Put sent packets into sendbuf circular buffer.

  recvthread: 
    remove pktinfo from free list
    if ACK/NAK: (processed for sent packets)
       in sendbuf, walk from current sequence number to the ACK/NAK number.
       for each null entry continue.
       for each non-null entry ignore until out-of-order limit.
       after OOO limit, start re-queue packet for send.
       once we get to correct seq number, we resend if NAK or mark as acked if ACK.
        (postive if ACK. On resend, use negative numbers to block resends)
       If the tail and head meet, then we have to block.
*/
struct PacketQueue {
  uint32_t soffset,roffset;  // monotonically increasing offset for send and recv buffers
  // will wrap around at 4tb (that's OK, though.. its just for local alignment)
  std::queue<PacketInfo*> sent; // sent packets awaiting acks. (push/pop)
  std::queue<PacketInfo*> asyncsendq; // waiting to be sent asynchronously by thread
  std::stack<PacketInfo*> free; // can expand the size of the free list on demand
  std::queue<PacketInfo*> recvd; // waiting to be picked up by recv()
  // can make recvd also be a circ buffer.  Allows blocking and re-request of gaps
  std::vector<PacketInfo*> deferbuf; // circ-buffer watches for gaps (add with skip must walk sequentially through to skip point to check for non-empty buckets.  A non-empty bucket indicates a gap in the sequence.  Acks come to sender out-of-order.  So sender should have the defer buffer.  It copies-in skipped acks and will do resend if re-encountered. )
  /* potential race condition: What if resend arrives, but the recvr has insufficient 
     buffer space to accept it even though the resend is needed to fill a gap
     that is blocking the dequeue of successful recvs? We can insert unsuccessful
     recvs into the queue or use circ buf.

     Can define limit on distance from pending resends and successful pkt recvs.
     since acks can be reordered, should perhaps rely on sender to detect gaps.
     Or reply with current seq number and max recvd seq in addition to ack request.
     resends should take higher priority.
     
     Or stash a window of recvs into the ack.
     
     We get into a possible wedge condition. Sender must limit
     number of outstanding packets (including acks).  Recvr should perhaps
     send its queue backlog in next ACK or just delay sending of acks if
     the backlog is too large.  This means recvr should do backlog check.
     or send an "I'm getting full" message to sender to force flush of 
     gap queue.
     Each ack should contain recvr backlog along with gapsize.

     Perhaps can use circ buffer as the recv queue.
  */
  //  std::vector<PacketInfo*> all; // listed here so they can be resized dynamically
  // must instantiate with fixed buffer size because it is too hard
  // to make sure that no packetinfo objects are currently in use for a
  // resize request.  There must be an init method to ensure that the
  // packet queue gets initialized properly. (must know MTU at init time)

  // alternatively you can dynamically assign packet data to subsections
  // of a very long character array


  //******** send procedure
  // get pktinfo from free list
  //   if no pktinfo, then create one
  // copy data into pktinfo->data
  // encode header (including checksum)
  // send pkt (with pacing or enqueue for thread to send)
  // add to sent queue
  //   done:  can do a recv or ack check if desired
 
  //******* recv procedure
  // get pktinfo from free list
  // recv packet
  // decode header
  // if nak or ack
  //   see if sequence number is next expected by dequeing an entry from
  //   the sent queue.  See if offset matches:
  //   *true*:  Dequeue sent data and return pktinfo to free queue
  //            possibly must remove entries from hash table as well.
  //   *false*: pkt requeueing/fast-resend procedure():
  //           May want to do a search in hash for offset and compare seq
  //              if seq is different, we may have been requeued.
  //           assuming no reording of packets on transit, resend immediate
  //           If we must worry about reordering, then add to defer queue. 
  //           if defer queue length or age is > reorder size, then request the
  //           resend. for the top defer queue entry.
  /*
         Alt is that recvr has gaps, it records them in the defer queue
	 If front of defer queue is > nreorder away from the currently
	 max recvd packet, then send a NAK.  Problem is that you must
	 sort entries in the defer queue to accomodate newly arrived
	 out-of-order pieces.

	 Could have circular buffer list based on modulo of seq.  
	 When tail meets a gap, then send a NAK.
   */
  //       and then pop the next entry off of sent queue.
  // else its a recvd data packet
  //   Make sure this packet is not redundant send by comparing
  //      its sequence number to the "retired" counter in PacketQueue
  //      discard if it is retired.  (must watch for wraps though!)
  //   validate the checksum
  //     *false*: send NAK
  //     *true*: check if in sequence... if not, send NAK packet for missing seq#'s
  //             send ack packet for this recv (rewrite header for the ack and send)
  //             put into recvd queue
  //             check for outstanding recv requests and copy
  //             recvd packets out of the buffer.  Update retired counter accordingly.
  //   can do send check now or force sends to occur in foreground.
  // Must have concurrent recv thread.  Send can be in foreground.
};

// breaks the buffer up into separate packets

UDPstream::UDPstream(int pt):
  RawUDP(pt),mtu(0),packetsize(576),actualbyterate(0.0),shortdelay(0.002){
  setByteRate(fMega(50.0));
}

UDPstream::UDPstream(char *host,int pt):
  RawUDP(host,pt),mtu(0),packetsize(576),actualbyterate(0.0),shortdelay(0.002){
  setByteRate(fMega(50.0));
}

/*
  Eventually need to tag the packets with a sequence number
  Can tag in-situ or always copy to an auxiliary buffer
 */
int UDPstream::send(char *buffer,int bufsize,int flgs){
  int r,n=0,sendsize;
  // how to sendto???
  if(flgs>=0) flags=flgs;
  // will fail if not bound or connected
  // OK, so now we break it into blocks
  packetdelay.reset();
  sendsize=(this->packetsize>bufsize)?bufsize:this->packetsize;
  r = ::send(port,buffer,sendsize,flags);
  if(SOCKERR(r)){
    if(errno==ENOBUFS){
      // try again after delay
      do{
	shortdelay.instantaneousWait();
	r = ::send(port,buffer,sendsize,flags);
      }while(SOCKERR(r) && errno==ENOBUFS);
      n+=r;
    }
    else {
      puts("Aborted with error which is not ENOBUFS");
      perror("error1 was ");
    }
  }
  else n+=r;
  // now do the rest ======================
  for(int i=sendsize;r>0 && i<bufsize;i+=this->packetsize,n+=r){
    int remaining = bufsize-i;
    sendsize=(this->packetsize>remaining)?remaining:this->packetsize;
    packetdelay.errorDiffusionWait();
    r = ::send(port,buffer,sendsize,flags);
    if(SOCKERR(r)){
      if(errno==ENOBUFS){ // try again after small delay
	do{
	  shortdelay.instantaneousWait();
	  r = ::send(port,buffer,sendsize,flags);
	}while(SOCKERR(r) && errno==ENOBUFS);
	n+=r;
      }
      else {
	puts("Aborted with error which is not ENOBUFS");
	perror("error2 was ");
      }
    }
    else n+=r;
    // manage ENOBUFS errors and fail on other errors
    // just do a short delay
  }
  return n;
}

int UDPstream::send(char *buffer,int bufsize,
		    IPaddress &to,int flgs){ // how to sendto???
  int r,n=0,sendsize;
  // how to sendto???
  if(flgs>=0) flags=flgs;
  // will fail if not bound or connected
  // OK, so now we break it into blocks
  packetdelay.reset();
  sendsize=(this->packetsize>bufsize)?bufsize:this->packetsize;
  r = ::sendto(port,buffer,sendsize,flags,
	       to.getSocketAddress(),sizeof(struct sockaddr_in));
  if(SOCKERR(r)){
    if(errno==ENOBUFS){
      // try again after delay
      do{
	shortdelay.instantaneousWait();
	r = ::sendto(port,buffer,sendsize,flags,
		     to.getSocketAddress(),sizeof(struct sockaddr_in));
      }while(SOCKERR(r) && errno==ENOBUFS);
    }
  }
  // now do the rest ======================
  for(int i=sendsize;r>0 && i<bufsize;i+=this->packetsize,n+=r){
    int remaining = bufsize-i;
    sendsize=(this->packetsize>remaining)?remaining:this->packetsize;
    packetdelay.errorDiffusionWait();
    r = ::sendto(port,buffer,sendsize,flags,
		 to.getSocketAddress(),sizeof(struct sockaddr_in));
    if(SOCKERR(r)){
      // manage ENOBUFS errors and fail on other errors
      // just do a short delay
      if(errno==ENOBUFS){ // try again after small delay
	do {
	  shortdelay.instantaneousWait();
	  r = ::sendto(port,buffer,sendsize,flags,
		       to.getSocketAddress(),sizeof(struct sockaddr_in));
	} while(SOCKERR(r) && errno==ENOBUFS);
      }
    }
  }
  return n;
}
int UDPstream::recv(char *buffer,int bufsize,IPaddress &from,int flgs){
  int n=0;
  for(int i=0;i<bufsize;i+=packetsize){
    int r,recvsize=bufsize-i;
    if(recvsize>packetsize) 
      recvsize=packetsize;
    r=RawUDP::recv(buffer,recvsize,from,flgs);
    if(!SOCKERR(r)) n+=r;
  }
  return n;
}
int UDPstream::recv(char *buffer,int bufsize,int flgs){
  int n=0;
  for(int i=0;i<bufsize;i+=packetsize){
    int r,recvsize=bufsize-i;
    if(recvsize>packetsize) 
      recvsize=packetsize;
    r=RawUDP::recv(buffer,recvsize,flgs);
    if(!SOCKERR(r)) n+=r;
  }
  return n;
}
int UDPstream::getMTU(){
  // how does MTU discovery work in this context???
  return 1440;
}

void UDPstream::setPacketRate(double packetspersecond){
  // implies measurement from packet-to-packet
  packetrate=packetspersecond;
  setPacketDelay(1.0/packetrate);
}

void UDPstream::setByteRate(double bytespersecond){
  this->setPacketRate(bytespersecond/((double)packetsize));
}

void UDPstream::setPacketDelay(double seconds){ // converts to integer internally
  packetdelay.setDelay(seconds);
  // make packetrate correct?
  if(seconds>0.0)
    packetrate=1.0/seconds;
  else // ALL BETS ARE OFF IF YOU MAKE THE DELAY 0!!!
    seconds=1e-6; // approx minimum measurable delay
}
// now we allow setting of timing requirments
void UDPstream::setPacketSize(long nbytes){
  double currentbyterate = this->getByteRate();
  this->packetsize = nbytes;
  this->setByteRate(currentbyterate);
}
