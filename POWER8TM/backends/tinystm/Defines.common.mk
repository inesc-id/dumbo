CC       := gcc
CFLAGS   += -std=c++11 -g -w -pthread -mcpu=power9 -mtune=power9 -fpermissive
CFLAGS   += -O2
CFLAGS   += -I$(LIB) -I ../../../stms/tinystm/src/ -I ../../../stms/tinystm/include -I ../../../stms/tinystm/src/atomic_ops
CFLAGS   += -I /home/shady/lib/boost/include
CPP      := g++
CPPFLAGS += $(CFLAGS)
LD       := g++
LIBS     += -lpthread

# Remove these files when doing clean
OUTPUT +=

LIB := ../lib
