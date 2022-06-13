#!/bin/sh

# mv j4.tbl-x.dmp -> j4.dmp-x

F=${1:-j4}
N=${2:-20}

i=0

while [ $i -le $N ]
do
    dmp=$F.tbl-$i.dmp
    if [ -f $dmp ] 
    then
        mv $dmp $F.dmp-$i
    else
	rm -f $F.dmp-$i
	ln -s /dev/null $F.dmp-$i
    fi
    i=`expr $i + 1`
done

exit 0
