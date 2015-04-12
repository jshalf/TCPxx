#include "ReliableDatagram.hh"
#include "PacketPacer.hh"
#include <pthread.h>

/*
  Reliable Independent Paced Packets (RIPP)
  ToDo:
	* MTU Discovery:  On connection establishment, it should calculate
	   the MTU and then use it to initialize the packet buffers to the
	   correct size.
	* TCP control socket drop monitoring
	* ACK over TCP option (currently acks over UDP for min latency)
	* Calculate Loss rates
	* New base class for PacketPacer that accepts loss stats
	  and computes correct pacing strategy using internal
	  algorithm.  Can have "FixedRatePacketPacer" that ignores
	  loss stats and have a "TCPFriendlyPacketPacer" to implement
	  TCP Friendly behavior.  The Pacer can be plugged in at
	  runtime (all inherit from the same base class).
	* Reliable Message:  This is a branch of RIPP where messages
	  can be larger than the MTU.  This will require manual message
	  re-assembly as packets arrive. This is built on top of
	  the ReliableDatagram/RIPP transport layer.
*/

class TCPXX_API RIPP : public ReliableDatagram {
protected:
  pthread_mutex_t freeqlock,sendqlock,pendqlock,recvqlock;
  pthread_cond_t sendqcond,newdatacond,pingcond;
  pthread_cond_t sendqemptycond,pendqemptycond;
  pthread_t sendthread,recvthread;
  double desired_byte_rate,current_byte_rate;
  PacketPacer rate;

  int done,firsttime;
  static void *sendHandler(void *p);
  static void *recvHandler(void *p);
  void startThreads();
  void stopThreads();
public:
  inline int isDone(){return done;}
  //RIPP();
  RIPP(RawTCPport *control_socket);
  RIPP(char *host,int port);
  virtual ~RIPP();
  //
  // need to set the packet rates
  //
  inline void setByteRate(double byterate){
    printf("SetByteRate to %f\n",(float)byterate);
    rate.setByteRate(byterate);
  }
  inline void setBitRate(double bitrate){rate.setBitRate(bitrate);}
  void init(int maxpktsize=1480,int nfree=1024);
  virtual int send(char *buffer,int nbytes);
    // if we are multithreaded
    // must put mutexes around each queue access
    // call recvCheck() continuously and enqueue recvd packets
    // we can intersperse these checks with paced packet output
    // or put the paced packet output into a separate thread
    // however, if packet pacing goes into another thread, there
    // is a danger that lock contention may hurt overall efficiency!
  virtual int recv(char *buffer,int nbytes);
  virtual int recv(char *buffer,int nbytes,uint32_t &seq);
  virtual int ping(char *buffer,int nbytes,double &timeout);
  inline int check(){
    return pendq.size();
  }
  inline void waitForPending(){ // is this OK?  (was returning int)
    // wait for sendq to drain first
    pthread_mutex_lock(&sendqlock);
    if(!sendq.empty()){
       pthread_cond_wait(&sendqemptycond,&sendqlock);
    }
    pthread_mutex_unlock(&sendqlock);    
    // now block until pendq is empty
    pthread_mutex_lock(&pendqlock);
    if(!sendq.empty()){
       pthread_cond_wait(&pendqemptycond,&pendqlock);
    }
    pthread_mutex_unlock(&pendqlock);    
  }
  void sendCheck();
  void recvCheck(); // contains some queue mutex's
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


public: // stuff carried forward from RawUDP
  int connect(){return RawUDP::connect();}
  int connect(IPaddress &addr){return RawUDP::connect(addr);}
  int connect(char *host,int port){return RawUDP::connect(host,port);}
  virtual int bind(){ return RawUDP::bind();}
  virtual int bind(int pt){return RawUDP::bind(pt);}
};



class RIPPServer : public RawTCPserver {
public:
  RIPPServer(int port,int q=5);
  // ~RIPPServer(){}// nothing to do here as well
  RIPP *accept();
};
