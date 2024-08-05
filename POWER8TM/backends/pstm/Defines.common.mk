LIB      := ../lib

CC       := gcc
CFLAGS   += -std=c++11 -w -fpermissive -mhtm -O2
CFLAGS   += -I $(LIB)
CPP      := g++
CPPFLAGS += $(CFLAGS)
LD       := g++
LIBS     := -lpthread
SRCS += $(LIB)/containers.cpp\
	$(LIB)/input_handler.cpp\
	$(LIB)/threading.cpp\
	$(LIB)/prod-cons.cpp

# Remove these files when doing clean
OUTPUT +=
