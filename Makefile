#makefile
UNAME            := $(shell uname | perl -pe 's/(sn\d\d\d\d|jsimpson)/UNICOS\/mk/')

CC = cc 
CXX = CC
RANLIB = touch

ifeq ($(UNAME), OSF1)
CXX = cxx
CCP = cxx
CFLAGS = -O -DOSF
LDFLAGS = -L. -lTCP++
AR = ar
RANLIB = ranlib
endif

ifeq ($(UNAME), Darwin)
CCP = c++
CFLAGS = -O
LDFLAGS = -L. -lTCP++
AR = ar
RANLIB = ranlib
endif

ifeq ($(UNAME), IRIX64)
CCP = CC
CFLAGS = -64 -O3 -mips4 -LNO -IPA -DSGI
LDFLAGS = -L. -lTCP++ -lC -lCsup
AR = /bin/ar
endif

ifeq ($(UNAME), IRIX)
CCP = CC
CFLAGS = -g
LDFLAGS = -L. -lTCP++
AR = /bin/ar
endif

ifeq ($(UNAME), UNICOS/mk)
CCP = CC
CFLAGS = -g -hinstantiate=used -DT3E
LDFLAGS = -L. -lTCP++
AR = ar
endif

ifeq ($(UNAME), Linux)
CC = gcc 
CCP = g++ 
CFLAGS = -g -DLINUX -DT3E -Wno-deprecated
LDFLAGS = -L. -lTCP++ 
AR = ar
endif

ifeq ($(UNAME), AIX)
CC = xlcc
CCP = xlC
CFLAGS = -g -DAIX
LDFLAGS = -L. -lTCP++
AR = ar
endif

OBJECTS  =  RawUDP.o ReliableDatagram.o RawMCAST.o RawTCP.o IPaddress.o PortMux.o  \
	RawPort.o NewString.o vatoi.o Timer.o  \
	ObjectMux.o AlignedAlloc.o ErrorReport.o mTCP.o \
	Delay.o UDPstream.o RateControl.o Timeval.o RIPP.o \
	PacketPacer.o

all:  libTCP++.a 

distclean: clean
	touch t.a
	rm -f *.a
	touch ii_files
	rm -rf ii_files
	touch core
	rm core
	touch t~
	rm -f *~
	(cd Examples; gmake distclean)

libTCP++.a:	$(OBJECTS)
	$(AR) ruc libTCP++.a $(OBJECTS)
	$(RANLIB) libTCP++.a

clean:
	touch t.o
	rm -f *.o
	(cd Examples; gmake clean)

tests: libTCP++.a
	(cd Examples ; gmake) 

examples: tests

echo: TCPechoClient TCPechoServer

TCPechoClient: TCPechoClient.cc libTCP++.a sTCP.o
	$(CCP) $(CFLAGS) TCPechoClient.cc -o TCPechoClient sTCP.o -L. -lTCP++


TCPechoServer: TCPechoServer.cc libTCP++.a sTCP.o
	$(CCP) $(CFLAGS) TCPechoServer.cc -o TCPechoServer sTCP.o -L. -lTCP++

vatoi.o: vatoi.hh vatoi.cc
	$(CCP) $(CFLAGS) -c vatoi.cc

Timer.o: Timer.hh Timer.cc Timeval.hh
	$(CCP) $(CFLAGS) -c Timer.cc

Delay.o: Delay.hh Delay.cc Timeval.hh
	$(CCP) $(CFLAGS) -c Delay.cc

RateControl.o: RateControl.hh RateControl.cc Timeval.hh
	$(CCP) $(CFLAGS) -c RateControl.cc

PacketPacer.o: PacketPacer.hh PacketPacer.cc Timeval.hh Timer.hh
	$(CCP) $(CFLAGS) -c PacketPacer.cc

Timeval.o: Timeval.hh Timeval.cc
	$(CCP) $(CFLAGS) -c Timeval.cc

Rate: Rate.cc
	$(CCP) $(CFLAGS) -o Rate Rate.cc -lm

mTCP.o: mTCP.hh mTCP.cc RawTCP.hh FlexArrayTmpl.H
	$(CCP) $(CFLAGS) -c mTCP.cc

sTCP.o: sTCP.hh sTCP.cc RawTCP.hh
	$(CCP) $(CFLAGS) -c sTCP.cc

NewString.o: NewString.hh NewString.cc
	$(CCP) $(CFLAGS) -c NewString.cc

IPaddress.o: IPaddress.hh IPaddress.cc
	$(CCP) $(CFLAGS) -c IPaddress.cc

RawPort.o: RawPort.cc RawPort.hh ErrorReport.hh
	$(CCP) $(CFLAGS) -c RawPort.cc

AlignedAlloc.o: AlignedAlloc.cc AlignedAlloc.h AlignedAlloc.hh
	$(CCP) $(CFLAGS) -c AlignedAlloc.cc

RawTCP.o: RawTCP.cc RawTCP.hh RawPort.hh IPaddress.hh
	$(CCP) $(CFLAGS) -c RawTCP.cc

ReliableDatagram.o: ReliableDatagram.cc ReliableDatagram.hh RawUDP.hh RawPort.hh IPaddress.hh
	$(CCP) $(CFLAGS) -c ReliableDatagram.cc

RIPP.o: RIPP.cc RIPP.hh ReliableDatagram.hh RawUDP.hh RawPort.hh RateControl.hh IPaddress.hh
	$(CCP) $(CFLAGS) -c RIPP.cc

RawUDP.o: RawUDP.cc RawUDP.hh RawPort.hh IPaddress.hh
	$(CCP) $(CFLAGS) -c RawUDP.cc

RawMCAST.o: RawMCAST.cc RawMCAST.hh RawPort.hh IPaddress.hh
	$(CCP) $(CFLAGS) -c RawMCAST.cc

UDPstream.o: UDPstream.cc UDPstream.hh RawUDP.hh IPaddress.hh Delay.hh Timer.hh
	$(CCP) $(CFLAGS) -c UDPstream.cc

ErrorReport.o: ErrorReport.cc ErrorReport.hh
	$(CCP) $(CFLAGS) -c ErrorReport.cc

PortMux.o: PortMux.cc PortMux.hh RawPort.hh OrderedList.H
	$(CCP) $(CFLAGS) -c PortMux.cc

ObjectMux.o: ObjectMux.cc ObjectMux.hh RawPort.hh OrderedList.H
	$(CCP) $(CFLAGS) -c ObjectMux.cc

CommandProcessor.o: CommandProcessor.cc CommandProcessor.h CommandProcessor.hh RawPort.hh PortMux.hh
	$(CCP) $(CFLAGS) -c CommandProcessor.cc

DataConnection.o: DataConnection.cc DataConnection.h DataConnection.hh RawPort.hh PortMux.hh
	$(CCP) $(CFLAGS) -c DataConnection.cc

