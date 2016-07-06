# Figure out what architecture we are dealing with here
# Strip out T3E unicos wierdness
UNAME := $(shell uname | perl -pe 's/(sn\d\d\d\d|jsimpson)/UNICOS\/mk/')

#default flags
CC = cc
CCP = CC
CFLAGS = -I.. -O
LDFLAGS = -L.. -lTCP++ -lC
LIBLIST = ../libTCP++.a 

# ../../libsockio.a

XLDFLAGS = -L../.. -lsockio -lieeeio 

ifeq ($(UNAME), machten)
CC = gcc
CCP = g++
CFLAGS += -DMACHTEN
endif

ifeq ($(UNAME), IRIX64)
CCP = CC
CFLAGS =  -I.. -O3 -64 -mips4 -LNO -IPA
LDFLAGS = $(XLDFLAGS) -L.. -lTCP++ -lC -lCsup
AR = /bin/ar
endif

ifeq ($(UNAME), IRIX)
CCP = CC
CFLAGS = -I.. -O
LDFLAGS = $(XLDFLAGS) -L.. -lTCP++
AR = /bin/ar
endif

ifeq ($(UNAME), UNICOS/mk)
CCP = CC
CFLAGS = -g -hinstantiate=used -DT3E
LDFLAGS = $(XLDFLAGS) -I.. -L.. -lTCP++
AR = ar
endif

CFLAGS  += -I../..

all: FileServer FileReceive

FileServer: FileServer.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o $@ $< $(LDFLAGS)

FileReceive: FileReceive.cc $(LIBLIST)
	$(CCP) $(CFLAGS) -o $@ $< $(LDFLAGS)

../libTCP++.a:
	$(MAKE) -C .. libTCP++.a

../../libsockio.a:
	$(MAKE) -C ../.. libsockio.a
