CC=g++
CFLAGS=-std=c++11 -g -Wall
#CFLAGS=-std=c++11 -g -O3 -Wall -pg -ftest-coverage -fprofile-arcs
INCLUDE=-I../.. -I../../third_party/libgo/third_party/gtest/include -I../../third_party/libgo -I../../third_party/libgo/libgo -I../../third_party/libgo/libgo/linux
LINK=-L../../build/third_party/libgo -L../../third_party/libgo/third_party/gtest/build -llibgonet -llibgo -ldl -lgtest_main -lgtest -lpthread \
	 -lboost_coroutine -lboost_context -lboost_thread -lboost_system -lboost_regex -lpthread -static -static-libgcc -static-libstdc++
#LINK+=-static -static-libgcc -static-libstdc++
TARGET=$(patsubst %.cpp, %.t, $(wildcard *.cpp))

all : ../../third_party/libgo/third_party/gtest/build/libgtest.a $(TARGET)

../../third_party/libgo/third_party/gtest/build/libgtest.a:
	cd ../../third_party/libgo/third_party/gtest/ && mkdir -p build && cd build && cmake .. && make -j4

%.t: %.o
	@echo "LD $@"
	@$(CC) $(CFLAGS) $^ -o $@ $(INCLUDE) $(LINK)

.cpp.o:
	@echo "CC $@"
	@$(CC) $(CFLAGS) -c $^ -o $@ $(INCLUDE)

.PHONY: clean

clean:
	rm *.o $(TARGET) -f
