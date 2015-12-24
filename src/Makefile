CC=g++
#CFLAGS=-std=c++11 -g -Wall -fPIC
CFLAGS=-std=c++11 -O3 -Wall -fPIC
STATIC_LIB=libnetwork.a
INCLUDE=-I.
OBJS=$(patsubst %.cpp, %.o, $(wildcard *.cpp))

all : $(STATIC_LIB)

$(STATIC_LIB) : $(OBJS)
	@ar cr $@ $^
	@echo "LD $(STATIC_LIB)"

.cpp.o:
	@echo "CC $@"
	@$(CC) $(CFLAGS) $(INCLUDE) -c $^ -o $@

.PHONY: clean install

clean:
	rm *.o $(STATIC_LIB) -f

install:
	install -m 0664 $(STATIC_LIB) /usr/local/lib
	mkdir -p /usr/local/include/network
	install -m 0664 *.h /usr/local/include/network
