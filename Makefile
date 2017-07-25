CPP=icpc
CPPFLAGS=-g -wd1478 -std=c++11 -I. -I../../../include
LDFLAGS= -L../../../lib -L/usr/lib64
LDLIBS=-lboost_system -lboost_thread -lpthread -lrt -lnuma 

SRCS=main.cpp numa.cpp
OBJS=$(SRCS:.cpp=.o)

all: test.exe

test.exe: $(OBJS)
	$(CPP) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

$(OBJS): %.o : %.cpp
	$(CPP) $(CPPFLAGS) -c $< -o $@

clean:
	rm -fr *.exe
	rm -fr *.o
