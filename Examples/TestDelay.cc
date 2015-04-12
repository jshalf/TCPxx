#include <stdio.h>
#include <stdlib.h>

#include "Delay.hh"
#include "Timer.hh"
/*
  So a multi-timer is required
  As well as a timer which works by error diffusion
 */

int main(int argc,char *argv[]){
  double r,u,s;
  fd_set bogus;
  int n=10,err;
  double nd=(double)n; 
  double timedelay=1.0;
  puts("=========================================");
  for(int j=0;j<7;j++,timedelay/=10.0){
    {
      SpinDelay delay;
      Timer time;
      delay.setDelay(timedelay);
      time.start(); delay.reset();
      for(int i=0;i<n;i++){
	//delay.continuousWait();
	delay.instantaneousWait();
      }
      time.stop();
      time.elapsedTimeSeconds(s,u,r);
      printf("SpinDelay: time=%g\testtime=%g\trtime=%g\tdiff=%g\n",
	     timedelay,timedelay*nd,r,timedelay*nd - r);
    }
    {
      SpinDelay delay;
      Timer time;
      delay.setDelay(timedelay);
      time.start(); delay.reset();
      for(int i=0;i<n;i++){
	delay.continuousWait();
	//delay.instantaneousWait();
      }
      time.stop();
      time.elapsedTimeSeconds(s,u,r);
      printf("SpinCont : time=%g\testtime=%g\trtime=%g\tdiff=%g\n",
	     timedelay,timedelay*nd,r,timedelay*nd - r);
    } 
    {
      SpinDelay delay;
      Timer time;
      delay.setDelay(timedelay);
      time.start(); delay.reset();
      for(int i=0;i<n;i++){
	delay.errorDiffusionWait();
	//delay.instantaneousWait();
      }
      time.stop();
      time.elapsedTimeSeconds(s,u,r);
      printf("SpinICont: time=%g\testtime=%g\trtime=%g\tdiff=%g\n",
	     timedelay,timedelay*nd,r,timedelay*nd - r);
    }
    puts("---------");
    {
      SelectDelay delay;
      Timer time;
      delay.setDelay(timedelay);
      time.start(); delay.reset();
      for(int i=0;i<n;i++){
	//delay.continuousWait();
	delay.instantaneousWait();
      }
      time.stop();
      time.elapsedTimeSeconds(s,u,r);
      printf("SelDelay: time=%g\testtime=%g\trtime=%g\tdiff=%g\n",
	     timedelay,timedelay*nd,r,timedelay*nd - r);
    }
    {
      SelectDelay delay;
      Timer time;
      delay.setDelay(timedelay);
      time.start(); delay.reset();
      for(int i=0;i<n;i++){
	delay.continuousWait();
	//delay.instantaneousWait();
      }
      time.stop();
      time.elapsedTimeSeconds(s,u,r);
      printf("SelCont : time=%g\testtime=%g\trtime=%g\tdiff=%g\n",
	     timedelay,timedelay*nd,r,timedelay*nd - r);
    }
    {
      SelectDelay delay;
      Timer time;
      delay.setDelay(timedelay);
      time.start(); delay.reset();
      for(int i=0;i<n;i++){
	delay.errorDiffusionWait();
	//delay.instantaneousWait();
      }
      time.stop();
      time.elapsedTimeSeconds(s,u,r);
      printf("SelICont: time=%g\testtime=%g\trtime=%g\tdiff=%g\n",
	     timedelay,timedelay*nd,r,timedelay*nd - r);
    }
    puts("---------");
    {
      HybridDelay delay;
      Timer time;
      delay.setDelay(timedelay);
      time.start(); delay.reset();
      for(int i=0;i<n;i++){
	//delay.continuousWait();
	delay.instantaneousWait();
      }
      time.stop();
      time.elapsedTimeSeconds(s,u,r);
      printf("SelDelay: time=%g\testtime=%g\trtime=%g\tdiff=%g\n",
	     timedelay,timedelay*nd,r,timedelay*nd - r);
    }
    {
      HybridDelay delay;
      Timer time;
      delay.setDelay(timedelay);
      time.start(); delay.reset();
      for(int i=0;i<n;i++){
	delay.continuousWait();
	//delay.instantaneousWait();
      }
      time.stop();
      time.elapsedTimeSeconds(s,u,r);
      printf("SelCont : time=%g\testtime=%g\trtime=%g\tdiff=%g\n",
	     timedelay,timedelay*nd,r,timedelay*nd - r);
    }
    {
      HybridDelay delay;
      Timer time;
      delay.setDelay(timedelay);
      time.start(); delay.reset();
      for(int i=0;i<n;i++){
	delay.errorDiffusionWait();
	//delay.instantaneousWait();
      }
      time.stop();
      time.elapsedTimeSeconds(s,u,r);
      printf("SelICont: time=%g\testtime=%g\trtime=%g\tdiff=%g\n",
	     timedelay,timedelay*nd,r,timedelay*nd - r);
    }
    puts("=========================================");
  }
}
