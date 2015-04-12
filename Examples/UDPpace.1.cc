#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DARWIN
#include <iostream>
using namespace std;
#else
#include <stream.h>
#endif

#include "RawUDP.hh"
#include "Timer.hh"
#include "Delay.hh"


  void TimevalToDouble(timeval &tm,double &out){
    out=(double)tm.tv_sec + (double)tm.tv_usec/(double)1000000.0;
  }
  void DoubleToTimeval(double v,timeval &r){
    r.tv_sec = (long)floor(v);
    r.tv_usec = (long)((v - floor(v))*(double)1000000.0);
  }
  void SubtractTimeval(timeval &in1,timeval &in2,timeval &out){
    out.tv_sec = in1.tv_sec - in2.tv_sec;
    if(in1.tv_usec < in2.tv_usec){
      /* borrow */
      //  puts("\t\t   BORROW\n");
      out.tv_sec--;
      out.tv_usec = 1000000l + in1.tv_usec - in2.tv_usec;
    }
    else out.tv_usec=in1.tv_usec-in2.tv_usec;
  }
  void AddTimeval(timeval &in1,timeval &in2,timeval &out){
    out.tv_sec = in1.tv_sec + in2.tv_sec;
    out.tv_usec = in1.tv_usec + in2.tv_usec;
    if(out.tv_usec>1000000l) {
      out.tv_sec+=1;
      out.tv_usec -= 1000000l;
    }
  }
  void CopyTimeval(timeval &in,timeval &out){
    out.tv_sec=in.tv_sec;
    out.tv_usec=in.tv_usec;
  }
  int SimpleCompareTimeval(timeval &in1,timeval &in2){
    /* if in1==in2, then return 0 */
    /* if in1>in2 then return 1 */
    /* if in1<in2 then return -1 */
    int r=0;
    if(in1.tv_sec < in2.tv_sec) {
      r=-1;
    }
    else { /* now we check for usec */
      if(in1.tv_sec == in2.tv_sec){
	if(in1.tv_sec < in2.tv_sec) {
	  r=-1;
	}
	else {
	  if(in1.tv_sec == in2.tv_sec){
	    return 0;
	  }
	  else
	    r=1;
	}
      }
      else {
	r=1;
      }
    }
  }
  int CompareTimeval(timeval &in1, timeval &in2){
    /* if in1==in2, then return 0 */
    /* if in1>in2 then return 1 */
    /* if in1<in2 then return -1 */
    /* if in1 is > 2x in2, then return +2 */
    /* if in1 is < 1/2 in2, then return -2 */
    /* now we check for factor of 2 diff */
    timeval difference;
    int r = SimpleCompareTimeval(in1,in2);
    if(r==0) return 0;
    if(r>0){
      /* normalize the interval */
      SubtractTimeval(in1,in2,difference);
      if(SimpleCompareTimeval(difference,in2)>0) return -2;
      else return -1;
    }
    else { /* r<0 */
      SubtractTimeval(in2,in1,difference);
      if(SimpleCompareTimeval(difference,in1)>0) return 2;
      else return 1;
    }
  }

void ZeroTimeval(timeval &t){
  t.tv_sec=0;
  t.tv_usec=0;
}

void PrintTimeval(char *lead_in,timeval &t){
  double d;
  TimevalToDouble(t,d);
  printf("%s: %u:%u --> %f\n",lead_in,t.tv_sec, t.tv_usec, d);
}

class RateControlI {
  double desired_rate;
  double payload_bits;
  timeval desired_interval; // computed from payload_bits/desired_rate
  double delta_t;
  double over_t;
  timeval lasttime,thistime,deltatime,overtime;
  void getDeltaT(){
    gettimeofday(&thistime,0);
    SubtractTimeval(thistime,lasttime,deltatime);
    TimevalToDouble(deltatime,delta_t);
  }
  static void ComputeInterval(double payload,double drate,timeval &i){
    double s=payload/drate;
    DoubleToTimeval(s,i);
    //PrintTimeval("Interval is",i);
  }
public:
  RateControlI(double desiredRate){
    timeval t;
    ZeroTimeval(overtime);
    ZeroTimeval(deltatime);
    desired_rate=desiredRate;
    gettimeofday(&thistime,0);
    CopyTimeval(thistime,lasttime);
    delta_t=over_t=payload_bits=0.0;
  }
  int checkInterval(double bits_in_payload){
    /* recomputes the rate and returns a 
     +1 to send this payload and a +2 if the overtime is
    way out of whack to indicate a multisend is required.
    -1 if the payload is way under (so can reduce interval)
    ( perhaps do this if 4x rather than 1/2? )
    0 if its not time to send yet.
    */
    double timeleft;
    getDeltaT(); /* converts delta_t into float */
    timeleft = bits_in_payload/desired_rate - delta_t - over_t;
    if(timeleft>0){
      if(delta_t < 0.5*payload_bits/desired_rate) return -2;
      return -1;
    }
    else { /* timeleft <= 0 */
      if(delta_t < 2.0*payload_bits/desired_rate) return 2;
      else return 1;
    }
  }
  int checkIntervalBytes(double bytes_in_payload){
    return checkInterval(bytes_in_payload*8.0);
  }
  int checkInterval(long bits_in_payload){
    return checkInterval((double)bits_in_payload);
  }
  int checkIntervalBytes(long bytes_in_payload){
    return checkIntervalBytes((double)bytes_in_payload);
  }
  void newInterval(){
    /* OK, here we must have integer over_t computed */
    // so current over_t is
    timeval t;
    if(payload_bits<=0.0) payload_bits=1.0;
    ComputeInterval(payload_bits,desired_rate,desired_interval);
    AddTimeval(deltatime,overtime,t);
    SubtractTimeval(desired_interval,t,overtime);
    // compute new overtime from this
    TimevalToDouble(overtime,over_t);
    // over_t = payload_bits/desired_rate - delta_t - over_t; // update over_t
    CopyTimeval(thistime,lasttime);
    payload_bits=0;
  }
  int sentData(double bits_in_payload){
    /* just update the overtime without a time check 
       so the checkRate is the only time that we call
       the gettimeofday.  So here, we recompute the interval
       without a new delta check.
     */
    payload_bits+=bits_in_payload;
    double timeleft = payload_bits/desired_rate - delta_t - over_t;
    /* return +1 if still over 
       return +2 if way over */
    /* return -1 if timeleft has been accounted_for */
    if(timeleft>0){
      if(delta_t < 0.5*payload_bits/desired_rate) return -2;
      return -1;
    }
    else { /* timeleft <= 0 */
      if(delta_t > 2.0*payload_bits/desired_rate) return 2;
      else return 1;
    }
  }
  int sentData(long bits_in_payload){ 
    return sentData((double)bits_in_payload);
  }
  int sentDataBytes(double bytes_in_payload){
    return sentData(bytes_in_payload*8.0);
  }
  int sentDataBytes(long bytes_in_payload){
    return sentData(((double)bytes_in_payload)*8.0);
  }
  /* doubleInterval  etc... */
  void setRate(double bits_per_second){
    desired_rate = bits_per_second;
  }
  void setRateBytes(double bytes_per_second){
    setRate(bytes_per_second * 8.0);
  }
  void checkAccuracy(long long ncycles,double payload){
    double ivl = payload*8.0/desired_rate;
    double total=0.0;
    for(long long i=0;i<ncycles;i++) total+=ivl;
    printf("payload=%g bytes rate=%g interval=%g total=%g\n",
	   payload,desired_rate,ivl,total);
  }
};

class RateControlFP {
  double desired_rate;
  double payload_bits;
  double delta_t;
  double over_t;
  timeval lasttime,thistime;
  void getDeltaT(){
    timeval t;
    gettimeofday(&thistime,0);
    SubtractTimeval(thistime,lasttime,t);
    TimevalToDouble(t,delta_t);
  }
  
public:
  void dumpState(){
    printf("\tdesired_rate=%f payload_bits=%f\n",
	   desired_rate,payload_bits);
    if(payload_bits>0)
      printf("\t\texpected_interval=%f\n",payload_bits/desired_rate);
    printf("\tdelta_t=%f over_t=%f\n",delta_t,over_t);
    PrintTimeval("\t lasttime",lasttime);
    PrintTimeval("\t thistime",thistime);
  }
  RateControlFP(double desiredRate){
    timeval t;
    desired_rate=desiredRate;
    gettimeofday(&thistime,0);
    CopyTimeval(thistime,lasttime);
    delta_t=over_t=payload_bits=0.0;
  }
  /* recomputes the rate and returns a 
     +1 to send this payload and a +2 if the overtime is
     way out of whack to indicate a multisend is required.
     -1 if the payload is way under (so can reduce interval)
     ( perhaps do this if 4x rather than 1/2? )
     0 if its not time to send yet.
  */
  inline int payloadCompare(double pbits){
    double timeleft = pbits/desired_rate - delta_t - over_t;
    /* return +1 if still over 
       return +2 if way over */
    /* return -1 if timeleft has been accounted_for */
    /*  if(timeleft>0){
      if(delta_t+over_t < 0.5*pbits/desired_rate) return -2;
      return -1;
    }
    else { 
      if(delta_t+over_t > 2.0*pbits/desired_rate) return 2;
      else return 1;
      } */
    
    if(timeleft>0){
      if(timeleft < 0.5*pbits/desired_rate) return -2;
      return -1;
    }
    else { /* timeleft <= 0 */
      if(-timeleft > pbits/desired_rate) return 2;
      else return 1;
    }
  }
  void reset(){
    over_t=0;
    getDeltaT();
    delta_t=0;
    CopyTimeval(thistime,lasttime);
    payload_bits=0;
  }
  void newInterval(){
    if(payload_bits<=0.0) payload_bits=1.0;
    over_t = payload_bits/desired_rate - delta_t - over_t; // update over_t
    over_t=-over_t;
    CopyTimeval(thistime,lasttime);
    payload_bits=0;
  }
  int checkInterval(double bits_in_payload){
    getDeltaT(); /* converts delta_t into float */
    return payloadCompare(bits_in_payload);
  }
  int sentData(double bits_in_payload){
    /* just update the overtime without a time check 
       so the checkRate is the only time that we call
       the gettimeofday.  So here, we recompute the interval
       without a new delta check.
     */
    payload_bits+=bits_in_payload;
    return payloadCompare(payload_bits);
  }

  int checkIntervalBytes(double bytes_in_payload){
    return checkInterval(bytes_in_payload*8.0);
  }
  int checkInterval(long bits_in_payload){
    return checkInterval((double)bits_in_payload);
  }
  int checkIntervalBytes(long bytes_in_payload){
    return checkIntervalBytes((double)bytes_in_payload);
  }
  int sentData(long bits_in_payload){ 
    return sentData((double)bits_in_payload);
  }
  int sentDataBytes(double bytes_in_payload){
    return sentData(bytes_in_payload*8.0);
  }
  int sentDataBytes(long bytes_in_payload){
    return sentData(((double)bytes_in_payload)*8.0);
  }
  /* doubleInterval  etc... */
  void setRate(double bits_per_second){
    desired_rate = bits_per_second;
  }
  void setRateBytes(double bytes_per_second){
    setRate(bytes_per_second * 8.0);
  }
  void checkAccuracy(long long ncycles,double payload){
    double ivl = payload*8.0/desired_rate;
    double total=0.0;
    for(long long i=0;i<ncycles;i++) total+=ivl;
    printf("payload=%g bytes rate=%g interval=%g total=%g\n",
	   payload,desired_rate,ivl,total);
  }
};
typedef RateControlFP RateControl;
/*
  Usage: UDPpace hostname npackets packets_per_second
*/
void main(int argc,char *argv[]){
  if(argc<4) { 
    printf("Usage:  %s host npackets packets_per_second\n",
	   argv[0]);
    exit(0);
  }
  long npackets=atoi(argv[2]),count=0;
  long packets_per_second=atoi(argv[3]);
  char buffer[1500];
  long *ibuffer = (long*)((void*)buffer);
  Timer t;
  int single_send=0,more_send=0,max_send_size=1,max_wait=0;
  double packetrate = (double)packets_per_second; /* double precision computed from packets_per_second */

  RateControl r(packetrate);
  t.reset(); t.start();
  r.setRateBytes(packetrate*1500.0);
  r.checkAccuracy(npackets,1500.0);
  r.reset();
  for(count=0;count<npackets;count++){
    int v,w;
    /* find out rate control issues */
    w=0;
    while((v=r.checkIntervalBytes(1500l))<=0){
      // puts("\t wait");
      w++;
    } /* my packet */
    if(w>max_wait) max_wait=w;
    /* check interval and sentdata : netierh returns proper values */
    if(v==1){ /* this payload is good */
      // puts("v==1");
      single_send++;
      r.sentDataBytes(1500l);
    }
    else if(v==2){
      int sendfactor=0;
      more_send++;
      //puts("v==2");
      /* OK, lets increase payload until its happy */
      while(v=r.sentDataBytes(1500l)>0){sendfactor++;}
      if(sendfactor>max_send_size) max_send_size=sendfactor;
      count=count+sendfactor-1;
    }
    else {
      printf("what?  v=%u\n",v);
    }
    r.newInterval();
    //printf("sent %u:%u\n",count,npackets);
  }
  t.stop();
  t.print("output is");
  printf("singles=%u mores=%u max_coalesce=%u max_wait=%u cycles\n",
	 single_send,more_send,max_send_size,max_wait);
  exit(0);
  // we will use delay between each packet for this right now 
  // (but really want to use direct timing of the pacing instead!)
  // You can modify Delay.hh Delay.cc to do timing for you.
  // Please refer to example Examples/TestDelay.cc for all the
  // parameters for Delay and how to use it.
  SpinDelay delay(1.0/packetrate);
  RawUDPclient udp(argv[1],7000); // implict connect
  ibuffer[1] = npackets; // write total number of packets into buffer

  for(count=0;count<npackets;count++){
    ibuffer[0]=count; // write current count in the sequence to buffer
    udp.write(buffer,1024); /* write in 1024 byte chunks for easy computation */
    /* now do a poll-delay until time to deliver next packet.
       first measurement is assumed time of last delivery.
    */
    delay.instantaneousWait(); 
  }

  ibuffer[0]=npackets; // send completion packet many times to ensure delivery
  for(int i=0;i<10;i++){
    udp.write(buffer,16);
    usleep(i);
  }
}


class IntervalTimer {
  timeval timelast,timenow;
  timeval interval,overtime;

public:
  IntervalTimer(){
    this->init();
    interval.tv_sec=interval.tv_usec=0;
  }
  void init(){
    gettimeofday(&timenow,0);
    timelast.tv_sec=timenow.tv_sec;
    timelast.tv_usec=timenow.tv_usec;
    overtime.tv_sec=overtime.tv_usec=0;
  }
  void setInterval(double ival){
    DoubleToTimeval(ival,interval);
  }
  void doubleInterval(){
    interval.tv_sec <<=1;
    interval.tv_usec <<=1;
    if(interval.tv_usec > 1000000){
      interval.tv_usec-=1000000;
      interval.tv_sec++;
    }
  }
  void halveInterval(){
    interval.tv_usec >>=1;
    if(interval.tv_sec==1)
      interval.tv_usec = (1000000>>1);
    interval.tv_sec >>= 1;
  }
  /* value of 2 is way over */
  int checkTime(){
    /* 0 is not ready */
    /* 1 is ready     */
    /* 2 is overdone  */
    /* if undertime by half on test, then return -1 */
    int r;
    timeval delta,ival;
    gettimeofday(&timenow,0);
    SubtractTimeval(timenow,timelast,delta);
    // AddTimeval();// hmm.. this compensates time differential
    // but doesn't take into account the actual bitrate
    // so the overtime must factor in the actual number of
    // bits in the payload.
    r=CompareTimeval(timenow,timelast);
  }
};

