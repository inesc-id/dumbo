### external dependencies
LIB_THREADING := ../deps/threading
LIB_INPUT_HANDLER := ../deps/input_handler
LIB_HTM_ALG := ../deps/htm_alg
LIB_PSTM_ALG := ../deps/tinystm
LIB_ARCH := ../deps/arch_dep

INCLUDES := -I ./include -I $(LIB_THREADING)/include \
  -I $(LIB_INPUT_HANDLER)/include -I $(LIB_HTM_ALG)/include \
  -I $(LIB_ARCH)/include -I $(LIB_PSTM_ALG)/include \
  -I bench \
#
DEFINES  :=
#
LIBS     := -L $(LIB_INPUT_HANDLER) -linput_handler \
  -L $(LIB_HTM_ALG) -lhtm_sgl -L $(LIB_PSTM_ALG)/lib -lstm  \
  -L $(LIB_THREADING) -lthreading -lpthread
#
LIB    := libnvhtm.a
NVHTML := nvhtm
EXEC   := test_spins
CC     := gcc -g -std=c++11 -Wall -fno-strict-aliasing -fno-stack-protector
CXX    := g++ -g -std=c++11 -Wall -fno-strict-aliasing -fno-stack-protector
LD     := g++
AR     := ar rcs

###############################
# Extra parameters if available
# also defines cmd options
include Makefile.defines
###############################

### see Makefine.defines, added USE_VECT there
CFLAGS     := -c $(DEFINES) $(INCLUDES) -fPIC 
CXXFLAGS   := -c $(DEFINES) $(INCLUDES) -fPIC 
LDFLAGS    := $(LIBS)

TESTS_FOLDER   := test

TEST_SRC   := $(TESTS_FOLDER)/AutoGenTests.c
TEST_OBJ   := \
  $(TESTS_FOLDER)/AutoGenTests.o \
  $(TESTS_FOLDER)/CuTest.o \
  $(TESTS_FOLDER)/TestSpins.o \
# Place in TEST_OBJ also the actual tests
TEST_EXE   := TestRunner
GEN_SCRIPT := ./make-unit-tests.sh

SRC      := src
MAIN     := $(SRC)/main.o
OBJS     :=                 \
  $(SRC)/spins.o            \
  $(SRC)/global_structs.o   \
  $(SRC)/containers.o       \
  $(SRC)/log_checker.o      \
  $(SRC)/impl_log_replayer.o \
  $(SRC)/impl_pcwm.o        \
  $(SRC)/impl_pcwm2.o        \
  bench/bench.o             \
#

.PHONY:	all clean

all: $(EXEC) $(NVHTM) $(LIB)
  @echo "Done!"
  # Done

$(EXEC): $(OBJS) $(MAIN)
  @echo "Linking..."
  $(CXX) $(OBJS) $(MAIN) $(LDFLAGS) -o $@
  ### only if it is a lib compilation

$(LIB): $(OBJS)
  @echo "Building static lib..."
  $(AR) $@ $(OBJS)
  ### only if it is a lib compilation

$(TEST_SRC): $(GEN_SCRIPT)
  @echo "Generating auto tests"
  $(GEN_SCRIPT) $(TESTS_FOLDER)/*.c > $(TEST_SRC)

$(TEST_EXE): $(TEST_OBJ) $(NVHTM)
  @echo "linking auto tests"
  $(LD) $(TEST_OBJ) -L . -l $(NVHTML) $(LDFLAGS) -o $@

%.o:	%.c
  @echo "$<"
  $(CC) $(CFLAGS) -o $@ $<

%.o:	%.cpp
  @echo "$<"
  $(CXX) $(CXXFLAGS) -o $@ $<

test_clwb:
  gcc -o test_clwb_latency test_clwb_latency.c -I ./include

unit_test: $(TEST_EXE)

run:
  make all

clean:
  rm -f $(LIB) *.o $(SRC)/*.o $(TEST_SRC) $(TEST_OBJ) $(TEST_EXE) bench/bench.o
