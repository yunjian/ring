
INC      = -I.
LIB      =
OPTIMIZE = -O2
DEBUG    = -g
CFLAGS-G = $(DEBUG)
CFLAGS-O = $(OPTIMIZE)
CFLAGS   = $(INC) $(CFLAGS-G)
LFLAGS   =
CPLUS    = g++
CC       = gcc
YACC     = byacc -vd
LEX      = flex
RANLIB   = ranlib
AR       = ar

.SUFFIXES: .o .cpp .c
HEADERS  = ring.h gif/gifsave.h img/imgRotate.h
SRCS_LIB = ring.cpp neu/ringNet.cpp neu/ringNeuron.cpp \
	neu/ringExport.cpp neu/ringUtil.cpp \
	img/imgPads.cpp
SRCS_LIC = gif/gifsave.c

OBJS_LIB = $(SRCS_LIB:.cpp=.o)
OBJS_LIC = $(SRCS_LIC:.c=.o)

ring: $(OBJS_LIB) $(OBJS_LIC)
	$(CPLUS) $(CFLAGS) $(LFLAGS) $(OBJS_LIB) $(OBJS_LIC) -o ring

.cpp.o:
	$(CPLUS) $(CFLAGS) -c -o $(<:.cpp=.o) $<

.c.o:
	$(CC) $(CFLAGS) -c -o $(<:.c=.o) $<

clean:
	rm -f ring.exe
	rm -f $(OBJS_LIB) $(OBJS_LIC)
