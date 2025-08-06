#include "Delay.hh"
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

void Delay::wait(timeval &tv){
  // fprintf(stderr,"This is a virtual base class\n");
  //fprintf(stderr,"You need to use one of the inherited\n");
  //fprintf(stderr,"classes of SpinDelay or SelectDelay.\n");
  if(tv.tv_sec) sleep(tv.tv_sec);
  if(tv.tv_usec) usleep(tv.tv_usec);
}
Delay::Delay() {
  gettimeofday(&lasttime,0);
  timeout.tv_sec=0;
  timeout.tv_usec=0;
}
Delay::Delay(double t) {
  gettimeofday(&lasttime,0);
  this->setDelay(t);
}
void Delay::setDelay(timeval &tv){
  timeout.tv_sec=tv.tv_sec;
  timeout.tv_usec=tv.tv_usec;
}
void Delay::setDelay(double seconds){
  Double2Time(seconds,timeout);
}
void Delay::instantaneousWait(){
  // always wait a fixed amount
  this->wait(timeout);
}
inline void SubtractTime(timeval &a,timeval &b,timeval &diff){
  if(a.tv_usec<b.tv_usec){
    // do a borrow operation (hopefully won't overflow)
    diff.tv_usec=a.tv_usec+1000000-b.tv_usec; 
    diff.tv_sec=a.tv_sec-b.tv_sec-1;
  }
  else {
    diff.tv_sec=a.tv_sec-b.tv_sec;
    diff.tv_usec=a.tv_usec-b.tv_usec;
  }
}
inline void AddTime(timeval &a,timeval &b,timeval &prod){
  prod.tv_sec=a.tv_sec+b.tv_sec;
  prod.tv_usec=a.tv_usec+b.tv_usec;
  if(prod.tv_usec>1000000){
    //puts("Overflow^^^^^^^");
    prod.tv_usec-=1000000;
    prod.tv_sec+=1;
  }
}
// integer-based continuous wait
void Delay::continuousWait(){
  timeval timediff,thistime;
  gettimeofday(&thistime,0);
  SubtractTime(thistime,this->lasttime,timediff);
  if(timediff.tv_sec<this->timeout.tv_sec || 
     (timediff.tv_sec==this->timeout.tv_sec && 
      timediff.tv_usec<this->timeout.tv_usec)){
    timeval tv;
    SubtractTime(this->timeout,timediff,tv);
    this->wait(tv);
    AddTime(this->lasttime,this->timeout,this->lasttime);
  }
  else {
    // puts("overtime");
    this->lasttime.tv_sec=thistime.tv_sec;
    this->lasttime.tv_usec=thistime.tv_usec;
  }
}
// error-diffusion-based
void Delay::errorDiffusionWait(){
  timeval timediff,thistime;
  gettimeofday(&thistime,0);
  SubtractTime(thistime,this->lasttime,timediff);
  if(timediff.tv_sec<this->timeout.tv_sec || 
     (timediff.tv_sec==this->timeout.tv_sec && 
      timediff.tv_usec<this->timeout.tv_usec)){
    timeval tv;
    SubtractTime(this->timeout,timediff,tv);
    this->wait(tv);
    AddTime(this->lasttime,this->timeout,this->lasttime);
  }
  else {
    double td,to;
    Time2Double(timediff,td);
    Time2Double(timeout,to);
    if(td>to*10.0){
      this->lasttime.tv_sec=thistime.tv_sec;
      this->lasttime.tv_usec=thistime.tv_usec;
    }
    else
      AddTime(this->lasttime,this->timeout,this->lasttime);
  }
}
void Delay::continuousWaitFP(){ // based on timing of last wait
  double dthistime,dtimediff,dlasttime,dtimedelay;
  timeval tv;
  gettimeofday(&tv,0);
  Time2Double(tv,dthistime);
  Time2Double(this->lasttime,dlasttime);
  Time2Double(this->timeout,dtimedelay);
  dtimediff = dthistime-dlasttime;
  if(dtimediff<dtimedelay){// must delay
    dtimediff = dtimedelay-dtimediff;
    this->wait(dtimediff);
    dlasttime+=dtimedelay;
    Double2Time(dlasttime,this->lasttime);
  }
  else { // overtime (reset the timing to start of this routine)
    Double2Time(dthistime,this->lasttime);
  }
}
void SpinDelay::wait(timeval &tv){
  timeval start,now,d;
  gettimeofday(&start,0);
  do {
    gettimeofday(&now,0);
    d.tv_sec = now.tv_sec - start.tv_sec;
    d.tv_usec = now.tv_usec - start.tv_usec;
  } while(d.tv_sec<tv.tv_sec || 
	  (d.tv_sec==tv.tv_sec && d.tv_usec<tv.tv_usec));
}

void SelectDelay::wait(timeval &tv){
  timeval mytv; // urk... select destroys timeval contents!
  mytv.tv_sec=tv.tv_sec;
  mytv.tv_usec=tv.tv_usec;
  // fprintf(stderr,"w[%u:%u]",mytv.tv_sec,mytv.tv_usec);
  select(0,0,0,0,&mytv);
}

void HybridDelay::wait(timeval &tv){
  if(tv.tv_sec==0 && tv.tv_usec<10000){
    timeval start,now,d;
    gettimeofday(&start,0);
    do {
      gettimeofday(&now,0);
      d.tv_sec = now.tv_sec - start.tv_sec;
      d.tv_usec = now.tv_usec - start.tv_usec;
    } while(d.tv_sec<tv.tv_sec || 
	    (d.tv_sec==tv.tv_sec && d.tv_usec<tv.tv_usec));
  }
  else {
    timeval mytv; // urk... select destroys timeval contents!
    mytv.tv_sec=tv.tv_sec;
    mytv.tv_usec=tv.tv_usec;
    // fprintf(stderr,"w[%u:%u]",mytv.tv_sec,mytv.tv_usec);
    select(0,0,0,0,&mytv);
  }
}

void NanoSleepDelay::wait(timeval &tv){
#ifdef _TIMESPEC_DECLARED
  struct timespec wt;
  wt.tv_sec=tv.tv_sec;
  wt.tv_nsec=tv.tv_usec;
  wt.tv_nsec*=1000; // 1e-9 insted of 1e-6
  nanosleep(&wt,0);
#else
  timeval mytv; // urk... select destroys timeval contents!
  mytv.tv_sec=tv.tv_sec;
  mytv.tv_usec=tv.tv_usec;
  // fprintf(stderr,"w[%u:%u]",mytv.tv_sec,mytv.tv_usec);
  select(0,0,0,0,&mytv);
#endif
}
