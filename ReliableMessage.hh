#include <stdio.h>
#include <stack>
#include <queue>
#include <map>
#include <sys/types.h>
#include <unistd.h>
#include "ReliableDatagram.hh"
#include <pthread.h> // we must be threadsafe from the getgo...

struct MessageInfo {
  vector<PacketInfo*> packets;
  int minseq; // minimum sequence #
  int slots; // count of empty slots in the msg before it is completed
  int nbytes; // the number of bytes that will be in the completed message
  MessageInfo(PacketInfo *firstpacket){
    // use first packet to determine nslots for MessageInfo
    // and insert it as the first member
  }
  static int getMsgRange(PacketInfo *pkt,int &minseq,int &maxseq){
    if(pkt->length<8) return 0; // too short
    uint32_t *d=(uint32_t*)pkt->data;
    uint32_t min,max;
    min=ntohl(d[0]);
    max=ntohl(d[1]);
    // just being anal about conversion safety issues
    minseq=(int)min;
    maxseq=(int)max;
    return 1;
  }
  static void setMsgRange(PacketInfo *pkt,int minseq,int maxseq){
    if(pkt->length<8) return 0; // too short
    uint32_t *d=(uint32_t*)pkt->data;
    uint32_t min,max;
    min=(int)minseq;
    max=(int)maxseq;
    d[0]=(uint32_t)htonl(d[0]);
    d[1]=(uint32_t)htonl(d[1]);
    // just being anal about conversion safety issues;
    return 1;
  }
  int copyOut(char *buffer){ // buffer is presumed to be at least as long as the message
    if(slots>0) return -1; // eeerrrorr!!!
    register int p,idx;
    for(idx=0,p=0;p<packets.size();p++){
      PacketInfo *pkt=packets[p];
      int last = pkt->length;
      memcpy(buffer+idx,pkt->data+8,pkt->length-8);
      idx += (pkt->length-8);
    }
    return idx;
  }
  // we never really need to do a copy-in because
  // we split the message into packets from the getgo
  int getSize() {return nbytes;}
  int isReady(){slots?0:1;}
  int addPacket(PacketInfo *p){
    // find the correct offset and copy it in
    int seq=p->seq;
    seq-=minseq;
    if(seq<0 || seq>=packets.size()) return -1; // total freakin error
    vector[seq]=p; // install the packet
    slots--; // and decrement the slot counter
    return isReady(); // and tell the caller if we are done yet
  }
};

class MessageQueue {
  map<int,MessageInfo*> mtable;
  queue<MessageInfo*> readyqueue; // completed messages
public:
  MessageQueue(){}
  ~MessageQueue(){
    // really, we need to use an iterator to remove all queued messages
    mtable.clear();
  }
  inline int empty(){(readyqueue.size()>0)?0:1;}
  inline int push(PacketInfo *p){
    int minseq,maxseq;
    MessageInfo *m;
    if(!p) return -1;
    MessageInfo::getMsgRange(p,minseq,maxseq); // extract message range from packet
    if(mtable.count(minseq)){
      m = mtable[minseq];
      if(m->addPacket(p)>0){
	readyqueue.push(m);
	mtable->erase(minseq);
      }
    }
    else {
      // probably should have a pool of MessageInfo objects?
      // well, that would screw up the new/delete on new message thing
      m = new MessageInfo(p);
      if(m.isReady()){ // early completion
	readyqueue.push(m);
      }
      else { // we have more packets to wait for
	mtable[minseq] = m;
      } 
    }
  }
  //inline int pending() {} // count number of mtable entries for pending messages
  inline int size(){return readyqueue.size();}
  inline MessageInfo *pop(){ // get the next ready message or NULL if nothing is ready
    if(readyqueue.empty()) return 0; // nothing there
    MessageInfo *m = readyqueue.top();
    readyqueue.pop();
    return m;
  }
  inline MessageInfo *top(){
    if(readyqueue.size()>0) return readyqueue.top();
    else return 0;
  }
};

class TCPXX_API ReliableMessage : public ReliableDatagram {
  // the key is the minsequence number for the message
  // If it is already in the map, then OK.
  MessageQueue mqueue;
  // use MessageAssemblyTable.count(uint32_t key); to find out if key is in container
  // MessageAssemblyTable.erase(key); to remove a key from the table
  // arriving packets get put into the MessageAssemblyTable
  // perhaps deserves its own class
protected:
  ReliableMessage();
  ReliableMessage(RawTCPport *s); // hand it the control socket to negotiate the connection.  This is for a server "accept"ed socket.
public:
  virtual ~ReliableMessage();
  void init(int maxpktsize=1480,int nfree=1024);
  virtual int send(char *buffer,int nbytes);
  virtual int recv(char *buffer,int nbytes);
  virtual int recv(char *buffer,int nbytes,uint32_t &seq);
// stuff carried forward from RawUDP
  int connect(){return RawUDP::connect();}
  int connect(IPaddress &addr){return RawUDP::connect(addr);}
  int connect(char *host,int port){return RawUDP::connect(host,port);}
  virtual int bind(){ return RawUDP::bind();}
  virtual int bind(int pt){return RawUDP::bind(pt);}
  virtual int write(void *buffer,int bufsize){
    return this->send((char*)buffer,bufsize);
  }
  virtual int readMsgSize(); // find out how large the message will be before getting the payload.  This is optional.  If you know a-priori that the message will always be smaller than a particular fixed size, then you need not check this for each message.
  virtual int read(void *buffer,int bufsize){
    return this->recv((char*)buffer,bufsize);
  }
  friend class ReliableMessageServer;
};


class TCPXX_API ReliableMessageServer : public ReliableDatagramServer {
public:
  ReliableMessageServer(int port,int q=5);
  // ~ReliableDatagramServer(){}// nothing to do here as well
  ReliableMessage *accept();
};

