#ifndef __PACKETPACER_HH_
#define __PACKETPACER_HH_
#include "Timeval.hh"
#include "Timer.hh"
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
  PacketPacer r;
  r.setRateBits(1.0e6); // set rate to 1 megabit per second
  r.newInterval(); // force an interval check
  // checkInterval actually calls system time which is 
  // high overhead, so you don't want to do it too much
  // however, you need to call checkInterval() any time there
  // has been an unknown length delay since the last send
  // in that situation, checkInterval() will automatically
  // reset all timing internally if the time deficit would
  // require blast sending of > 32k bytes of data... a
  // situation that would saturate the host output buffer.
  for(count=0;count<npackets;count++){
    int w=0;
    packets_sent=0;
    // rules inside of r.pace()
    // check interval automatically every 16 packets
    // or after we have sent enough packets to recover
    // the time deficit from the last pause operation (pause internally)
    //  -- calcInterval internally
    // checkInterval() will not attempt to do error recovery
    // if it is behind by more than 32k bytes because
    // it would otherwise flood the hosts send buffer
    r.pace(packet_size);
    udp.send(packet_data,packet_size);
    //---- send a packet here -------
    // r.newInterval(); /* compute a new deltaT */
  }
}
#endif

class PacketPacer {
  double desired_rate;
  double payload_bits;
  double delta_t;
  double over_t;
  timeval lasttime,thistime;
  Timer delay;
  int contig_byte_counter; // how many bytes to send contiguously to catch-up on time
  inline double getDeltaT();
  void pause(double nseconds); // using best avaialble method
public:
  PacketPacer();
  // void dumpState();
  // int getState(double *st);
  // inline void checkAccuracy(long long ncycles,int payload);
  // should have routine for checking timer granularity
  inline void reset(); // zero all of the error diffusion calculations
  inline void pace(unsigned int packetsize_bytes);
  inline void newInterval();
  
  void setBitRate(double bits_per_second);
  void setByteRate(double bytes_per_second);
  inline double getByteRate(){return desired_rate*8.0;}
  inline double getBitRate(){return desired_rate;}
};

double PacketPacer::getDeltaT(){
  timeval t;
  // 'i' means use the "inline" version
  iGetTime(thistime);
  // gettimeofday(&thistime,0);
  iSubtractTimeval(thistime,lasttime,t);
  iTimevalToDouble(t,delta_t);
  //printf("Pace: Compute New DeltaT=%f\n",(float)delta_t);
  return delta_t;
}

void PacketPacer::reset(){
  //printf("Pace: Reset\n");
  over_t=0;
  getDeltaT();
  delta_t=0;
  CopyTimeval(thistime,lasttime);
  // probably should subtract out the current time interval
  // because this forces an additional delay (not so good if
  // we are recovering.
  payload_bits=0;
  contig_byte_counter=0;
}

void PacketPacer::newInterval(){
  // printf("Pace: NewInterval\n");
  if(payload_bits<=0.0) payload_bits=1.0;
  getDeltaT();
  //  printf("\tovert=bits/rate - delta_t - over_t : %f= %f/%f(%f) - %f - %f\n",
  //	 payload_bits/desired_rate - delta_t - over_t,
  //	 payload_bits,desired_rate,
  //	 payload_bits/desired_rate,
  //	 delta_t,over_t);
  over_t = payload_bits/desired_rate - delta_t - over_t; // update over_t
  over_t=-over_t; // calc overtime from the last round
  // and then compute new time interval and
  // carry forward the error
  // getDeltaT();
  CopyTimeval(thistime,lasttime);
  payload_bits=1.0;
  contig_byte_counter=0;
}

// checkInterval is getDeltaT()
// and then payloadCompare();

void PacketPacer::pace(unsigned int payload_size){
  double pbits = ((double)payload_size)*8.0;
  // printf("Call GetDeltaT for payload =%u pbits=%f payload_bits=%f\n",payload_size,pbits,payload_bits+pbits);
  this->getDeltaT();
  // double v = pbits/desired_rate; // bits / (bits/sec) = nseconds to send
  // given an interval of delta_t+over_t to work with
  // so, we must send payload_size bytes within the interval
  payload_bits+=pbits;
  double timeleft = payload_bits/desired_rate - delta_t - over_t;
  // payload_bits gets zeroed when we start a new interval
  // it represents the number of bits sent during *this* interval


  // unconvinced that once timeleft goes positive that it is
  // the correct thing to pause.
  // perhaps calculate the new interval first?
  // and then pause?
  if(timeleft>0) {
    // printf("Timeleft>= so pause timeleft=%f\n",timeleft);
    // we can pause before sending
    pause(timeleft);
    // go ahead and compute a new interval?
    // and zero the counters
    // printf("\t and then compute new interval\n");
    newInterval();
  }
  else { // we are overtime... must send more than one packet to catch up
    // first lets compute how many bytes we would need to send to catch up
    // if its greater than 32k, then reset() everything
    // printf("we are overtime so immediately drop out of pace\n");
    if(-desired_rate/timeleft > 32.0e3) { 
      //printf("!!!!!Waaay overtime = %f, so reset\n",desired_rate/timeleft);
      reset(); return;}
    // otherwise, lets get out as quickly as we can unless 
    // we have exceeded our contig_bytes limit and must recompute
    contig_byte_counter+=payload_size;
    if(contig_byte_counter>32000) { newInterval(); return; }
  }
}
#endif
