#ifndef __DELAY_HH_
#define __DELAY_HH_
#include "Timeval.hh"

class Delay {
  timeval timeout;
  timeval lasttime;
protected:
  virtual void wait(timeval &tv);
  static inline void Time2Double(timeval &tv,double &t){
    t=(double)(tv.tv_sec);
    t+=((double)(tv.tv_usec))/1000000.0;
  }
  static inline void Double2Time(double t,timeval &tv){
    tv.tv_sec = (int64_t)floor(t);
    tv.tv_usec = (int64_t)(1000000.0*(t-floor(t)));
  }
  inline void wait(double t){
    timeval tv;
    Double2Time(t,tv);
    this->wait(tv);
  }
public:
  Delay();
  Delay(double t);
  void setDelay(timeval &tv);
  void setDelay(double seconds);
  void instantaneousWait();
  void continuousWaitFP();
  void continuousWait();
  // require 'reset()' be called just before use
  void errorDiffusionWait();
  void reset(){gettimeofday(&lasttime,0);}
};

// Use tight getttimeofday() loop to create delay
class SpinDelay : public Delay {
protected:
  virtual  void wait(timeval &tv);
public:
  SpinDelay(){}
  SpinDelay(double t):Delay(t){}
};

class SelectDelay : public Delay {
protected:
  virtual void wait(timeval &tv);
public:
  SelectDelay(){}
  SelectDelay(double t):Delay(t){}
};

// uses most accurate available technique based on interval of delay

class HybridDelay : public Delay {
protected:
  virtual void wait(timeval &tv);
public:
  HybridDelay(){}
  HybridDelay(double t):Delay(t){}
};

class NanoSleepDelay : public Delay{
protected:
  virtual void wait(timeval &tv);
public:
  NanoSleepDelay(){}
  NanoSleepDelay(double t):Delay(t){}
};

#endif
