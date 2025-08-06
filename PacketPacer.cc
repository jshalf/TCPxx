#include "PacketPacer.hh"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

PacketPacer::PacketPacer():desired_rate(1500.0*8.0) { // default approx 1 1500 byte packet per second
  this->reset();
  this->reset(); // to get the deltaT flushed completely
}

void PacketPacer::setBitRate(double bits_per_second){
  desired_rate = bits_per_second;
  // printf("Set Desired Rate to %f Megabits per sec\n",desired_rate/1000000.0);
}
void PacketPacer::setByteRate(double bytes_per_second){
  setBitRate(bytes_per_second * 8.0);
}

// this is presumably internal
void PacketPacer::pause(double timeleft){
  int r;
  // getDeltaT(); // refresh the current interval
#ifdef _TIMESPEC_DECLARED
  // double timeleft = bytes_in_payload*8.0/desired_rate - delta_t - over_t;
    struct timespec wt;
    if(timeleft>0.0){ // just double-check
      wt.tv_sec=(time_t)(timeleft);
      wt.tv_nsec=(unsigned long)(1.0e9*(timeleft-(double)wt.tv_sec)); // 1e-9
      //printf("timeleft > 0 = %f, so nanosleep\n",(float)timeleft);
      r=nanosleep(&wt,0); // doesn't check for signal errors
      if(r<0){ // nanosleep failed: assume we must busywait
	delay.reset(); delay.start(); // start a timer
	//printf("Nanosleep failed! so spinwait\n");
	while(delay.realTime()<timeleft){} // busy wait
      }
    }
#else // we don't have nanosleep, so kludge it with sleep and usleep()
    if(timeleft>=1.0){
      delay.reset(); delay.start(); // start a timer
      sleep((unsigned int)timeleft);
      timeleft-=delay.realTime();
    }
    if(timeleft>1.0e4 && timeleft<1.0){ // we can try to use select to pause
      usleep((unsigned int)(timeleft*1.0e6));
    }
    else { // must spinwait with inaccurate clock 
      delay.reset(); delay.start(); // start a timer
      while(delay.realTime()<timeleft){} // spinwait
    }
#endif
    // return delay.realTime(); // return actual time spent
}
