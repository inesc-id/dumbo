CC       := gcc
CFLAGS   += -std=c++11 -gdwarf-2 -g3 -w -pthread -fpermissive -mcpu=power9 -mtune=power9 -I$(LIB) -O2
CPP      := g++
CPPFLAGS += -std=c++11 -gdwarf-2 -g3 -w -pthread -fpermissive -mcpu=power9 -mtune=power9 -I$(LIB) -O2
LD       := g++
LIBS     += -lpthread

# Remove these files when doing clean
OUTPUT +=

LIB := ../lib
