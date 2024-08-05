# ==============================================================================
#
# Defines.common.mk
#
# ==============================================================================


CC       := gcc
CFLAGS   += -g -pthread
CFLAGS   += -O2 -std=c++11 
CFLAGS   += -I$(LIB) $(BATCH_RATIO)
CPP      := g++
CPPFLAGS += $(CFLAGS)
LD       := g++
LIBS     += -lpthread

# Remove these files when doing clean
OUTPUT +=

LIB := ../lib

STM := ../../..//stms/norec



# ==============================================================================
#
# End of Defines.common.mk
#
# ==============================================================================
