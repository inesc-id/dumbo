LIB      := ../lib

CC       := gcc
CFLAGS   += -std=c++11 -Wall -fpermissive  -O2 # -DNDEBUG
CFLAGS   += -I $(LIB) -I .
CPP      := g++
CPPFLAGS += $(CFLAGS)
LD       := g++
LIBS     := -lpthread
SRCS+=\
	$(LIB)/global_structs.c \
	$(LIB)/impl_pcwm.c \
	$(LIB)/impl_log_replayer.cpp\
	$(LIB)/containers.cpp\
	$(LIB)/input_handler.cpp\
	$(LIB)/threading.cpp\
	$(LIB)/prod-cons.cpp\
	$(LIB)/impl_crafty.c\
	$(LIB)/impl_ccHTM.c \
	$(LIB)/impl_epoch_impa.c\
	$(LIB)/impl_PHTM.c\
	$(LIB)/htm.cpp\
	$(LIB)/spins.cpp\
	$(LIB)/impl_htmOnly.c\
	$(LIB)/impl_pcwm.c\
	$(LIB)/impl_pcwm2.c\
# Remove these files when doing clean
OUTPUT +=
