
SRC := ./src
INC := -I ./include
LIB := rep
LDFLAGS += -L . -l $(LIB)
CXXFLAGS += -std=c++2b -O2 -Wall $(BENCH_TYPE)
LD := $(CXX)
AR := ar cru

REP_SRCS := \
	$(SRC)/replayer.cpp \
	$(SRC)/naive_repl_impl.cpp \
	$(SRC)/forward_link_repl_impl.cpp \
	$(SRC)/seq_log_repl_impl.cpp \
#
REP_OBJS := ${REP_SRCS:.cpp=.o}
EXEC := test

all: test

lib$(LIB).a: $(REP_OBJS)
	$(AR) $@ $^ 

$(EXEC).cpp: lib$(LIB).a
	# forces librep.a to be built

$(EXEC): $(EXEC).cpp
	@$(LD) $(CXXFLAGS) $(INC) -o $@ $^ $(LDFLAGS) 

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

clean:
	rm -f $(REP_OBJS) $(EXEC) lib$(LIB).a

