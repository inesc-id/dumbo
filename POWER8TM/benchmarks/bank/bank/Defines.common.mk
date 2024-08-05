PROG := bank

SRCS += \
	bank.c \
	$(LIB)/thread.c \
	$(LIB)/random.c \
	$(LIB)/mt19937ar.c \
#
OBJS := ${SRCS:.c=.o}
