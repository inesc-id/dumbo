PROG := array

SRCS += \
	array.c \
	$(LIB)/thread.c \
	$(LIB)/random.c \
	$(LIB)/mt19937ar.c \
#
OBJS := ${SRCS:.c=.o}
