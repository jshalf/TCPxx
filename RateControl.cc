#include "RateControl.hh"
RateControl::RateControl(double desiredRate){
  timeval t;
  desired_rate=desiredRate;
  gettimeofday(&thistime,0);
  CopyTimeval(thistime,lasttime);
  delta_t=over_t=payload_bits=0.0;
}

void RateControl::dumpState(){
  printf("----------------------------\n");
  printf("\tdesired_rate=%f payload_bits=%f\n",
	 desired_rate,payload_bits);
  if(payload_bits>0)
    printf("\t\texpected_interval=%f packets_per_interval=%f\n",
	   payload_bits/desired_rate,
	   (delta_t+over_t)/(payload_bits/desired_rate));
  // payload_bits/desired_rate);
  printf("\tdelta_t=%f over_t=%f\n",delta_t,over_t);
  PrintTimeval("\t lasttime",lasttime);
  PrintTimeval("\t thistime",thistime);
}

int RateControl::getState(double *st){
  int i=0;
  st[i]=delta_t; i++;
  st[i]=over_t; i++;
  st[i]=delta_t+over_t; i++;
  st[i]=1500.0*8.0/desired_rate; i++;
  return i;
}
 
