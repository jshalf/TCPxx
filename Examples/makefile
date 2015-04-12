# Figure out what architecture we are dealing with here
# Strip out T3E unicos wierdness
UNAME := $(shell uname | perl -pe 's/(sn\d\d\d\d|jsimpson)/UNICOS\/mk/')

CC=cc
CCP=CC
MPCXX = CC

#default flags
CC = cc
CCP = CC
CFLAGS = -I.. -O
LDFLAGS = -L.. -lTCP++ -lC
LIBLIST = ../libTCP++.a

ifeq ($(UNAME), machten)
CC = gcc
CCP = g++
CFLAGS += -DMACHTEN
endif

ifeq ($(UNAME), Darwin)
CC = cc
CCP = c++
CFLAGS += -DDARWIN
endif

ifeq ($(UNAME), IRIX64)
CCP = CC
# CFLAGS = -I.. -g -n32 -mips4 -O0
CFLAGS = -I.. -O3 -64 -mips4 -LNO -IPA -LANG:ansi-for-init-scope=ON
LDFLAGS = -L.. -lTCP++ -lm 
AR = /bin/ar
endif

ifeq ($(UNAME), IRIX)
CCP = CC
CFLAGS = -I.. -O
LDFLAGS = -L.. -lTCP++ -lm
AR = /bin/ar
endif

ifeq ($(UNAME), UNICOS/mk)
CCP = CC
CFLAGS = -g -I.. -hinstantiate=used -DT3E
LDFLAGS = -L.. -lTCP++ -lm
AR = ar
endif

ifeq ($(UNAME), Linux)
CC = gcc 
CCP = g++ 
MPCXX = g++
CFLAGS = -g -I.. -DLINUX -DT3E -Wno-deprecated
LDFLAGS = -L.. -lTCP++ -lpthread -lm
MPIFLAGS = -I/usr/local/include -DMPI
MPILIBS = -L/usr/local/lib -lmpich
AR = ar
endif

ifeq ($(UNAME), AIX)
CC = xlcc
CCP = xlC
MPCXX = mpCC
CFLAGS = -g -I.. -DAIX
LDFLAGS = -L.. -lTCP++ -lm
MPIFLAGS = -DMPI
MPILIBS = -lmpi
AR = ar
endif

all: tests

distclean: clean
	touch  tmp
	rm -f  tmp SpeedTestClient TestDataConnection \
		NonBlockClient SpeedTestServer UDPechoClient \
		UDPpaceServer UDPpace \
		NonBlockServer TCPechoClient UDPechoServer \
		TCPclient TCPechoServer TCPserver \
		TestCommandProcessor Rlogin getURL simpleGetURL \
		ServerPush ReliableDatagramExample RIPPExample
	touch ii_files
	rm -rf ii_files
	touch core
	rm core
	rm -f *~

clean:
	touch t.o
	rm -f *.o

tests: SpeedTestClient  \
       NonBlockClient SpeedTestServer UDPechoClient \
       NonBlockServer TCPechoClient UDPechoServer \
       TCPclient TCPechoServer TCPserver \
       Rlogin getURL simpleGetURL TestDelay \
	MCASTechoClient MCASTechoServer ReliableDatagramExample \
	RIPPExample

examples: tests

TestDelay: TestDelay.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o TestDelay TestDelay.cc $(LDFLAGS)

getURL: getURL.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o getURL getURL.cc $(LDFLAGS)

simpleGetURL: simpleGetURL.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o simpleGetURL simpleGetURL.cc $(LDFLAGS)

WebServer: WebServer.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o WebServer WebServer.cc $(LDFLAGS)

EmbededServerPush: EmbededServerPush.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o EmbededServerPush EmbededServerPush.cc $(LDFLAGS) -lcl -lfpe -lm

ServerPush: ServerPush.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o ServerPush ServerPush.cc $(LDFLAGS)

TestRlogin: TestRlogin.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o TestRlogin TestRlogin.cc $(LDFLAGS)

mcx: mcx.cc $(LIBLIST)
	$(CCP) $(CFLAGS) mcx.cc -o mcx $(LDFLAGS)

mpx: mpx.cc $(LIBLIST)
	$(CCP) $(CFLAGS) mpx.cc -o mpx $(LDFLAGS)

mpx_t: mpx_t.cc $(LIBLIST)
	$(CCP) $(CFLAGS) mpx_t.cc -o mpx_t $(LDFLAGS) -lpthread

NonBlockClient: NonBlockClient.cc $(LIBLIST) 
	$(CCP) $(CFLAGS) NonBlockClient.cc -o NonBlockClient $(LDFLAGS) 

NonBlockServer: NonBlockServer.cc $(LIBLIST)
	$(CCP) $(CFLAGS) NonBlockServer.cc -o NonBlockServer $(LDFLAGS)

Rlogin: Rlogin.o RloginClient.o $(LIBLIST)
	$(CCP) $(CFLAGS) -o Rlogin Rlogin.o RloginClient.o $(LDFLAGS)

VPreader: VPreader.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o VPreader VPreader.cc ${LDFLAGS}

VPtester: VPtester.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o VPtester VPtester.cc ${LDFLAGS}

VPdatasink: VPdatasink.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o VPdatasink VPdatasink.cc ${LDFLAGS} -lpthread

CactusStubP3: CactusStubP3.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -DHAS_MPI -I/usr/local/include -o CactusStubP3 CactusStubP3.cc ${LDFLAGS} -L/usr/local/lib -lmpich

Rlogin.o: Rlogin.cc RloginClient.hh $(LIBLIST)
	$(CCP) $(CFLAGS) -c Rlogin.cc $(LDFLAGS)

RloginClient.o: RloginClient.cc RloginClient.hh
	$(CCP) $(CFLAGS) -c  RloginClient.cc

TCPclient: TCPclient.cc
	$(CCP) $(CFLAGS) TCPclient.cc -o TCPclient $(LDFLAGS)

TCPserver: TCPserver.cc $(LIBLIST)
	$(CCP) $(CFLAGS) TCPserver.cc -o TCPserver $(LDFLAGS)

TCPechoClient: TCPechoClient.cc $(LIBLIST)
	$(CCP) $(CFLAGS) TCPechoClient.cc -o TCPechoClient $(LDFLAGS)

TCPechoServer: TCPechoServer.cc $(LIBLIST)
	$(CCP) $(CFLAGS) TCPechoServer.cc -o TCPechoServer $(LDFLAGS)

SpeedTestClient: SpeedTestClient.cc $(LIBLIST)
	$(CCP) $(CFLAGS) SpeedTestClient.cc -o SpeedTestClient $(LDFLAGS)

SpeedTestServer: SpeedTestServer.cc $(LIBLIST)
	$(CCP) $(CFLAGS) SpeedTestServer.cc -o SpeedTestServer $(LDFLAGS)

UDPblaster: UDPblaster.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPblaster.cc -o UDPblaster $(LDFLAGS)

ParallelUDPblaster: ParallelUDPblaster.cc $(LIBLIST)
	$(MPCXX) $(CFLAGS) ${MPIFLAGS} \
	 ParallelUDPblaster.cc MPIutils.cc -o ParallelUDPblaster \
	 $(LDFLAGS) ${MPILIBS}

ReliableDatagramExample: ReliableDatagramExample.cc $(LIBLIST)
	$(CCP) $(CFLAGS) ReliableDatagramExample.cc -o ReliableDatagramExample $(LDFLAGS)

RIPPExample: RIPPExample.cc $(LIBLIST)
	$(CCP) $(CFLAGS) RIPPExample.cc -o RIPPExample $(LDFLAGS)

UDPecho: UDPecho.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPecho.cc -o UDPecho $(LDFLAGS)

UDPechoServer: UDPechoServer.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPechoServer.cc -o UDPechoServer $(LDFLAGS)

UDPechoClient: UDPechoClient.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPechoClient.cc -o UDPechoClient $(LDFLAGS)

pace: UDPpaceServer UDPpace

UDPpaceServer: UDPpaceServer.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPpaceServer.cc -o UDPpaceServer $(LDFLAGS)

UDPpace: UDPpace.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPpace.cc -o UDPpace $(LDFLAGS) -lm

MCASTechoServer: MCASTechoServer.cc $(LIBLIST)
	$(CCP) $(CFLAGS) MCASTechoServer.cc -o MCASTechoServer $(LDFLAGS)

MCASTechoClient: MCASTechoClient.cc $(LIBLIST)
	$(CCP) $(CFLAGS) MCASTechoClient.cc -o MCASTechoClient $(LDFLAGS)

UDPstreamClient: UDPstreamClient.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPstreamClient.cc -o UDPstreamClient $(LDFLAGS)

UDPstreamServer: UDPstreamServer.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPstreamServer.cc -o UDPstreamServer $(LDFLAGS)

UDPperf: UDPperf.cc $(LIBLIST)
	$(CCP) $(CFLAGS) UDPperf.cc -o UDPperf $(LDFLAGS)

TestCommandProcessor: TestCommandProcessor.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o TestCommandProcessor \
		TestCommandProcessor.cc $(LDFLAGS)

TestDataConnection: TestDataConnection.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o TestDataConnection TestDataConnection.cc $(LDFLAGS)

