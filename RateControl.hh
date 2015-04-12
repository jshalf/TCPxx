#ifndef __RATECONTROL_HH_
#define __RATECONTROL_HH_
#include "Timeval.hh"
#include <stdio.h>

/*
  Attempts to maintain constant rate
  Using cyclic error diffusion.
  Accurate up to 600Megabits.  Considerably less accurate at higher rates.
*/

/*=================  Typical Usage Case: =========================*/
#if 0
{
  int packets_sent;
  int packet_size=1480;
  RateControl r(packetrate*(double)packet_size);
  r.setRateBytes(packetrate*(double)packet_size);
  r.reset();
  for(count=0;count<npackets;count+=packets_sent){
    int w=0;
    packets_sent=0;
    // find out rate control issues 
    while(r.checkIntervalBytes((double)packet_size)<=0) w++; // wait
    // or can use
    // r.pauseIntervalBytes((double)packet_size); to try nanosleep() wait
    do{ // now send packets (coalesce packets if deltaT is too large)
      //---- send a packet here -------
      packets_sent++;
      if(packets_sent>=16) break; // max coalesce
    } while(r.sentDataBytes((double)packet_size)>1);
    r.newInterval(); /* compute a new deltaT */
  }
}


// A new usage case.  This attempts to take advantage of
// a new blocking wait feature to reduce CPU utilization for
// lower speed data streams.  This may not be very effective
// if the network interface is driven at full performance.
{
  int packets_sent;
  int max_packet_size=1480;
  RateControl r(packetrate*(double)max_packet_size);
  r.setRateBytes(packetrate*(double)max_packet_size);
  r.reset();
  for(count=0;count<npackets;count+=packets_sent){
    int w=0;
    double plen = (double)packet_size;
    packets_sent=0;
    // find out rate control issues 
    r.pauseIntervalBytes(plen); // blocking pause
    do { // now send packets (coalesce packets if deltaT was too large)
      // this makes up for overshoot of the timer.
      //---- send a packet here -------
      packets_sent++;
      if(packets_sent>=16) break; // max coalesce
    } while(r.sentDataBytes(plen)>1);
    r.newInterval(); /* compute a new deltaT */
  }
}
#endif
/* ================= End Use Case ======================== */

class RateControl {
  double desired_rate;
  double payload_bits;
  double delta_t;
  double over_t;
  timeval lasttime,thistime;
  inline void getDeltaT();
public:
  RateControl(double desiredRate);
  void dumpState();
  int getState(double *st);
  /* recomputes the rate and returns a 
     +1 to send this payload and a +2 if the overtime is
     way out of whack to indicate a multisend is required.
     -1 if the payload is way under (so can reduce interval)
     ( perhaps do this if 4x rather than 1/2? )
     0 if its not time to send yet.
  */
  inline int payloadCompare(double pbits);
  inline void reset();
  // remove carry-over of errors from previous measurement period
  inline void resetErrorDiffusion();
  inline void newInterval();
  inline int checkInterval(double bits_in_payload);
  inline int sentData(double bits_in_payload);
  inline int pauseIntervalBytes(double bytes_in_payload);
  inline int checkIntervalBytes(double bytes_in_payload);
  inline int checkInterval(long bits_in_payload);
  inline int checkIntervalBytes(long bytes_in_payload);
  inline int sentData(long bits_in_payload);
  inline int sentDataBytes(double bytes_in_payload);
  inline int sentDataBytes(long bytes_in_payload);
  /* doubleInterval  etc... */
  inline void setRate(double bits_per_second);
  inline void setRateBytes(double bytes_per_second);
  inline double getRate(){return desired_rate;}
  inline double getRateBytes(){return desired_rate/8.0;}
  inline void checkAccuracy(long long ncycles,double payload);
};


/*********** Tons of Inlined Stuff **************/

void RateControl::getDeltaT(){
  timeval t;
  // 'i' means use the "inline" version
  iGetTime(thistime);
  // gettimeofday(&thistime,0);
  iSubtractTimeval(thistime,lasttime,t);
  iTimevalToDouble(t,delta_t);
}

int RateControl::payloadCompare(double pbits){
  double v = pbits/desired_rate;
  double timeleft = v - delta_t - over_t;
  /* return +1 if still over 
     return +2 if way over */
  /* return -1 if timeleft has been accounted_for */
  /* if(timeleft>0){
     if(delta_t+over_t < 0.25*v) return -2;
     return -1;
     }
     else { 
     if(delta_t+over_t > 2.0*v) return 2;
     else return 1;
     } */
  //printf("Payload Compare v=%f from pbits=%f/desired rate=%f and timeleft=%f\n",v,pbits,desired_rate,timeleft);
  if(v>0.0)
    v = timeleft/v;
  else v=0.0;
  //printf("\t normalized to v=%f\n",v);
  if(timeleft>0){
    if(v>0.5) return -2; // we are greater than 50% overtime
    return -1;
  }
  else {
    if(-v>0.5) return 2;
    else return 1;
  }
}
void RateControl::reset(){
  over_t=0;
  getDeltaT();
  delta_t=0;
  CopyTimeval(thistime,lasttime);
  // probably should subtract out the current time interval
  // because this forces an additional delay (not so good if
  // we are recovering.
  payload_bits=0;
}
void RateControl::resetErrorDiffusion(){
  this->newInterval();
  over_t=0;
}
void RateControl::newInterval(){
  if(payload_bits<=0.0) payload_bits=1.0;
  over_t = payload_bits/desired_rate - delta_t - over_t; // update over_t
  over_t=-over_t;
  CopyTimeval(thistime,lasttime);
  payload_bits=0;
}
int RateControl::checkInterval(double bits_in_payload){
  getDeltaT(); /* converts delta_t into float */
  return payloadCompare(bits_in_payload);
}
int RateControl::sentData(double bits_in_payload){
  /* just update the overtime without a time check 
     so the checkRate is the only time that we call
     the gettimeofday.  So here, we recompute the interval
     without a new delta check.
  */
  payload_bits += bits_in_payload;
  return payloadCompare(payload_bits);
}

int RateControl::pauseIntervalBytes(double bytes_in_payload){
    int r=checkIntervalBytes(bytes_in_payload);
    // printf("CheckInterval = %d\n",r);
    if(r>0) return r;
#ifdef _TIMESPEC_DECLARED
    register double timeleft = bytes_in_payload*8.0/desired_rate - delta_t - over_t;
    register struct timespec wt;
    // printf("Timespec Declared!\n");
    if(timeleft>0){ // just double-check
      wt.tv_sec=(time_t)(timeleft);
      wt.tv_nsec=(long)(1.0e9*(timeleft-(double)wt.tv_sec)); // 1e-9
      //printf("timeleft > 0 = %f, so nanosleep\n",(float)timeleft);
      r=nanosleep(&wt,0); // doesn't check for signal errors
      if(r<0) // nanosleep failed: assume we must busywait
	//	printf("Nanosleep failed! so spinwait\n");
	while((r=checkIntervalBytes(bytes_in_payload))<=0){} // busy wait
    }
    r=1; // this is somewhat arbitrary! (hope this is true!!!!)
    // it (*should*) be true and will save some processing overhead!
#else
    while((r=checkIntervalBytes(bytes_in_payload))<=0){} // busy wait
#endif
    // printf("pauseIntervalBytes=%u for bytes_in_payload=%f\n",
    //	   r,(float)bytes_in_payload);
    return r;
}

int RateControl::checkIntervalBytes(double bytes_in_payload){
  return checkInterval(bytes_in_payload*8.0);
}
int RateControl::checkInterval(long bits_in_payload){
  return checkInterval((double)bits_in_payload);
}
int RateControl::checkIntervalBytes(long bytes_in_payload){
  return checkIntervalBytes((double)bytes_in_payload);
}
int RateControl::sentData(long bits_in_payload){ 
  return sentData((double)bits_in_payload);
}
int RateControl::sentDataBytes(double bytes_in_payload){
    return sentData(bytes_in_payload*8.0);
}
int RateControl::sentDataBytes(long bytes_in_payload){
  return sentData(((double)bytes_in_payload)*8.0);
}
/* doubleInterval  etc... */
void RateControl::setRate(double bits_per_second){
  desired_rate = bits_per_second;
  //printf("Set Desired Rate to %f Megabits per sec\n",desired_rate/1000000.0);
}
void RateControl::setRateBytes(double bytes_per_second){
  setRate(bytes_per_second * 8.0);
}
void RateControl::checkAccuracy(long long ncycles,double payload){
  double ivl = payload*8.0/desired_rate;
    double total=0.0;
    for(long long i=0;i<ncycles;i++) total+=ivl;
    // printf("payload=%g bytes rate=%g interval=%g total=%g\n",
    //	   payload,desired_rate,ivl,total);
}



#endif


#if 0 /* implementation with primarily integer computations */
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
#endif
