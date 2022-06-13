
PROGS = co-count3 gen-ttable3 gen-ttable3.sun64 modtree3 stat-child

all : ${PROGS}

co-count3 : 
	g++ -O3 -o co-count3 co-count3.cc

gen-ttable3 : 
	g++ -O3 -o gen-ttable3 gen-ttable3.cc

gen-ttable3.sun64 : 
	g++ -DSPARCV9 -O3 -o gen-ttable3.sun64 gen-ttable3.cc

modtree3 : 
	g++ -O3 -o modtree3 modtree3.cc

stat-child : 
	g++ -O3 -o stat-child  stat-child.cc


clean :
	rm -f ${PROGS} *.o *~ core

