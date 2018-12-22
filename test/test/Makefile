CC=g++
CFLAGS=-std=c++11 -Wall -O3 -g
INCLUDE=-I../..
LINK=-L../../build -llibgonet -llibgo -lboost_coroutine -lboost_context -lboost_thread -lboost_system -lboost_regex -pthread -ldl

#LINK+=-ltcmalloc_minimal
#CFLAGS+=-DPROFILE=1
#LINK+=-lprofiler -lunwind

LINK+=-lssl -lcrypto
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
