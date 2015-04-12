#ifndef __MTCP_H_
#define __MTCP_H_
/*******************************************************
MTCP

Purpose: For high-performance networks, the TCP windowsize
 must be manipulated.  However, large TCP windows can 
 hurt performance because any lost packet necessitates
 a resend of the entire window (from the point that the
 packet was lost). You can get the advantages of high
 data bandwidth with the error-tolerance of small
 TCP windows by using multiple sockets to send your data.

Based on Work by Brian Toonen (ptTCP pthreaded TCP) from
Argonne National Lab.  This rev. eliminates threads from the
round-robin case

1999: Brian's work is related to 1992 paper by D.J. Iannucci
and J. Lekashman entitled "MFTP: Virtual TCP window scaling using
multiple connections".  

Could do current_target_offset (on each) and current_nbytes (on each)?

packetnum,packetoffset (offset is modulo MTU), finalbuf=number?
// just find the one with an incomplete offset

********************************************************/
#include "FlexArrayTmpl.H"
#include "RawTCP.hh"
#include "PortMux.hh"

class mTCPserver : public RawTCPserver{
  // RawTCPserver *master;
  FlexArray<RawTCPport *> ports;
  FlexArray<PortMux> ready_to_read,ready_to_write;
  FlexArray<int> read_bytes,write_bytes;
  long mtu,total_read_bytes,total_write_bytes;
public:
  mTCPserver(int port,int nports);
  virtual ~mTCPserver();
  virtual int read(void *buffer,size_t size);
  virtual int write(void *buffer,int size);
};

class mTCPclient : public RawTCPclient {
  FlexArray<RawTCPport *> ports;
  FlexArray<PortMux> ready_to_read,ready_to_write;
  FlexArray<int> read_bytes,write_bytes;
  long mtu,total_read_bytes,total_write_bytes;
public:
  mTCPclient(char *host,int port,int nports);
  virtual ~mTCPclient();
  virtual int read(void *buffer,size_t size);
  virtual int write(void *buffer,int size);
};

#endif
