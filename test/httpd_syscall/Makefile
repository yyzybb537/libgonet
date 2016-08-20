CC=g++
CFLAGS=-std=c++11 -Wall -O3 -g
LINK=-ltcmalloc_minimal
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
