CXX=`root-config --cxx`
RM=rm -f
CXXFLAGS=-O3 -Wall $(shell root-config --cflags)
LDFLAGS=-O3 $(shell root-config --ldflags)
LDLIBS=$(shell root-config --libs) -lMinuit

SRCS=charon_offaxis.cpp process.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: charon_onaxis

charon_onaxis: $(OBJS)
	$(CXX) $(LDFLAGS) -o charon_offaxis $(OBJS) $(LDLIBS) 

depend: .depend

.depend: $(SRCS)
	$(RM) ./.depend
	$(CXX) $(CXXFLAGS) -MM $^>>./.depend;

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) *~ .depend

include .depend
