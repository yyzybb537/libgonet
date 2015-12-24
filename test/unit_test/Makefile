CC=g++
CFLAGS=-std=c++11 -O3 -Wall
#CFLAGS=-std=c++11 -g -Wall -pg -ftest-coverage -fprofile-arcs
INCLUDE=-I.. -I../../coroutine/unit_test/gtest_unit/gtest/include
LINK=-L../../coroutine/unit_test/gtest_unit/gtest/lib/.libs -lcoroutine -ldl -lnetwork -lgtest_main -lgtest -lpthread -L..\
	 -lboost_thread -lboost_system -lboost_regex -lpthread
#LINK+=-static -static-libgcc -static-libstdc++
TARGET=$(patsubst %.cpp, %.t, $(wildcard *.cpp))

all : /usr/lib/libgtest.a $(TARGET)

/usr/lib/libgtest.a:
	cd ../../coroutine/unit_test/gtest_unit/gtest && ./configure && make
	sudo cp gtest/lib/.libs/* /usr/lib

%.t: %.o
	@echo "LD $@"
	@$(CC) $(CFLAGS) $^ -o $@ $(INCLUDE) $(LINK)

.cpp.o:
	@echo "CC $@"
	@$(CC) $(CFLAGS) -c $^ -o $@ $(INCLUDE)

.PHONY: clean

clean:
	rm *.o $(TARGET) -f
