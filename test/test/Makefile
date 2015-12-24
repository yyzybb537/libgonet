CC=g++
CFLAGS=-std=c++11 -g -Wall
INCLUDE=-I../../src -I../../third_party/libgo/src
LINK=-L../../build -L../../build/third_party/libgo -llibgonet -llibgo -ldl -lpthread \
	 -lboost_thread -lboost_system -lboost_regex -lpthread
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
