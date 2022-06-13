
SRCS = misc.cc memory.cc port.cc context.cc train.cc
HEADERS = misc.h port.h context.h train.h
OBJS = misc.o memory.o port.o context.o train.o

SRCS2 = tserver.cc ttable.cc rtable.cc
HEADERS2 = tserver.h ttable.h rtable.h
OBJS2 = tserver.o misc.o memory.o port.o context.o ttable.o rtable.o

SRCS3 = gserver.cc tpair.cc tree.cc graph.cc showf0.cc
HEADERS3 = gserver.h tpair.h tree.h graph.h
OBJS3 = gserver.o misc.o memory.o port.o context.o tpair.o tree.o graph.o showf0.o

TARFILES = ${SRCS} ${HEADERS} ${SRCS2} ${HEADERS2} ${SRCS3} ${HEADERS3} Makefile Memo
PRTFILES = Makefile misc.h misc.cc memory.cc tree.h tree.cc \
		train.h train.cc \
		port.h port.cc context.h context.cc \
		gserver.h gserver.cc tpair.h tpair.cc graph.h graph.cc showf0.cc \
		tserver.h tserver.cc ttable.h ttable.cc rtable.h rtable.cc
ALLPS = all.ps


ifeq (${HOSTTYPE},sun4)
LDFLAGS = -lsocket -lnsl
endif

all :	train tserver gserver


train : ${OBJS}
	g++ ${CFLAGS} -o train ${OBJS} ${LDFLAGS}

tserver : ${OBJS2}
	g++ ${CFLAGS} -o tserver ${OBJS2} ${LDFLAGS}

gserver : ${OBJS3}
	g++ ${CFLAGS} -o gserver ${OBJS3} ${LDFLAGS}


%.o : %.cc
	g++ ${CFLAGS} -c -o $@ $<


misc.o : misc.cc misc.h

memory.o : memory.cc

train.o : train.cc train.h misc.h

port.o : port.cc port.h context.h misc.h

context.o : context.cc context.h misc.h

tserver.o : tserver.cc tserver.h ttable.h rtable.h misc.h port.h

ttable.o : ttable.cc ttable.h misc.h

rtable.o : rtable.cc rtable.h misc.h

gserver.o : gserver.cc gserver.h misc.h port.h tpair.h graph.h

tpair.o : tpair.cc tpair.h misc.h

tree.o : tree.cc tree.h misc.h

graph.o : graph.cc graph.h misc.h

debug :
	make CFLAGS="-g -DDEBUG"

opt :
	make CFLAGS=-O3

clean :
	rm -f train tserver gserver *.o *~ core

tar :
	tar cvfz all.tgz ${TARFILES}

prt :
	rm -f ${ALLPS}; \
	for i in ${PRTFILES}; do \
		enscript -h -2r -p- $$i >> ${ALLPS}; \
	done; echo ${ALLPS} done

