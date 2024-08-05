CC       := gcc
CFLAGS   += -std=c++11 -w -pthread -fpermissive -mcpu=power9 -mtune=power9 -I$(LIB) -I . -O2
CPP      := g++
CPPFLAGS += -std=c++11 -w -pthread -fpermissive -mcpu=power9 -mtune=power9 -I$(LIB) -I . -O2
LD       := g++
LIBS     += -lpthread

# Remove these files when doing clean
OUTPUT +=

LIB := ../lib
