CC       := gcc
CFLAGS   += -std=c++11 -w -pthread -fpermissive -mcpu=power9 -mtune=power9
CFLAGS   += -O2 # -g3 -gdwarf-2 
CFLAGS   += -I$(LIB)
CPP      := g++
CPPFLAGS += $(CFLAGS)
LD       := g++
LIBS     += -lpthread

# Remove these files when doing clean
OUTPUT +=

LIB := ../lib
