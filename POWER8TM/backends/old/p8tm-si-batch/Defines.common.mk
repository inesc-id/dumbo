CC       := gcc
CFLAGS   += -std=c++11 -gdwarf-2 -g3 -w -pthread -fpermissive -mcpu=power9 -mtune=power9 -L/home/shady/lib 
CFLAGS   += -O0
CFLAGS   += -I$(LIB)
CPP      := g++
CPPFLAGS += $(CFLAGS)
LD       := g++
LIBS     += -lpthread

# Remove these files when doing clean
OUTPUT +=

LIB := ../lib
