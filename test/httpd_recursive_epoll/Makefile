CC=g++
CFLAGS=-std=c++11 -Wall -g -O3
INCLUDE=-I../.. -I../../third_party/libgo -I../../third_party/libgo/libgo -I../../third_party/libgo/libgo/linux
LINK=-L../../build -L../../build/third_party/libgo -ltcmalloc_minimal -llibgo \
	 -lboost_context -lboost_thread -lboost_system -ldl -lpthread
#CFLAGS+=-DPROFILE=1
#LINK+=-lprofiler -lunwind
LINK+=-static

TARGET=$(patsubst %.cpp, %.t, $(wildcard *.cpp))

all : $(TARGET)

%.t: %.o
	@echo "LD $@"
	@$(CC) $(CFLAGS) $^ -o $@ $(INCLUDE) $(LINK)

.cpp.o:
	@echo "CC $@"
	@$(CC) $(CFLAGS) -c $^ -o $@ $(INCLUDE)

.PHONY: clean

clean:
	rm *.o $(TARGET) -f
