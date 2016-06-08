CC=g++
#CFLAGS=-std=c++11 -g -Wall
CFLAGS=-std=c++11 -Wall -O3
INCLUDE=-I../.. -I../../third_party/libgo/third_party/gtest/include -I../../third_party/libgo -I../../third_party/libgo/libgo -I../../third_party/libgo/libgo/linux
LINK=-L../../build/third_party/libgo -L../../third_party/libgo/third_party/gtest/build -llibgonet -llibgo -ldl -llibgo_main -lpthread \
	 -lboost_thread -lboost_system -lboost_regex -lcares -lpthread
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
