#!/bin/sh

# corpus preparation

# read conf from $Base/run-prep.conf and $Wrk/run-prep.conf
#		log file => $Wrk/run-prep.log
# check parsed/ and tokenized/
# add path to $Base/bin
# create vocab.[ef], sym.tbl 
# make LM (trigram.binlm)
# make BXI {train,test}.bxi
# make phrase cutoff data (Tables/prob.tbl*)

Base=/usr/local/tools/Stx/Prep
Wrk=`pwd`
Conf=run-prep.conf
Log=run-prep.log
Tmp=prep$$.tmp

echo "Script started at `date`" > $Log
echo "Running on `hostname`" >> $Log
echo >> $Log
echo "Base=$Base" >> $Log
echo "Wrk=$Wrk" >> $Log
echo >> $Log

#
# Read Config Files
#

set > $Tmp

# read default config file
. $Base/$Conf

# read local config file
[ -f $Wrk/$Conf ] && . $Wrk/$Conf

# TempDir may have trailing "/"
TempDir=`echo $TempDir | sed 's:/$::g'`

# show what are set by config file
echo "===== Params set by config files =====" >> $Log
set | diff $Tmp - | grep "^> " | colrm 1 2 >> $Log
echo "======================================" >> $Log
rm -f $Tmp


#
# Check corpus directories
#

[ ! -d tokenized ] && echo "no tokenized directory" && exit 1
[ ! -d parsed ] && echo "no parsed directory" && exit 1


#
# Create local directory
#

[ ! -d Tables ] && mkdir Tables

#
# Add path to $Base/bin
#

PATH="$Base/bin:$PATH"; export PATH

#
# Add symlinks
#

rm -f punc.tbl rmpunc.pl
ln -s $Base/dat/punc.tbl
ln -s $Base/bin/$RmPunc rmpunc.pl


#
# Concat corpus
#


ALLE=$TempDir/all.e
ALLF=$TempDir/all.$SfxF
ALLP=$TempDir/all.par

echo "concat corpus files..."
gcat tokenized/*.e > $ALLE
gcat tokenized/*.$SfxF > $ALLF
gcat parsed/*.e | gegrep '^PROB|^.TOP' > $ALLP

#
# make vocab files
# 

echo "making vocab.[ef] and sym.tbl..."
gtr 'A-Z' 'a-z' < $ALLE | make-vocab.pl > vocab.e 2> vocab.e.log
make-vocab.pl -nodowncase < $ALLF > vocab.f 2> vocab.f.log

#
# make symtbl
#

filter-parse3.pl < $ALLP | modtree3 -conv punc.tbl -upqpunc -rmpunc -symtbl > sym.tbl

#
# make LM
#

echo "making LM..."

TXT=$TempDir/mklm.$$.txt
FRQ=$TempDir/mklm.$$.freq
VOC=$TempDir/mklm.$$.voc
IDN=$TempDir/mklm.$$.idn

BINLM=trigram.binlm
LogLM=make-lm.log
cp /dev/null $LogLM

gtr 'a-z' 'A-Z' < $ALLE | gsed 's/ \. *$//g' | gawk '{print "<s> " $0 " </s>"}' > $TXT

text2wfreq < $TXT > $FRQ 2>> $LogLM
wfreq2vocab -top 65530 < $FRQ > $VOC 2>> $LogLM
text2idngram -vocab $VOC < $TXT > $IDN 2>> $LogLM
idngram2lm -idngram $IDN -vocab $VOC -binary $BINLM 2>> $LogLM

rm -f $TXT $FRQ $VOC $IDN

# 
# make BXI
#

echo "making BXI files..."
echo "Start making BXI files at `date`" >> $Log
CutE=`awk '$3==1{print $1;exit}' vocab.e`
CutF=`awk '$3==1{print $1;exit}' vocab.f`

[ $CutE -ge 65500 ] && CutE=65500
[ $CutF -ge 65500 ] && CutF=65500
echo "CutE=$CutE CutF=$CutF" >> $Log

TMPDIR=$TempDir; export TMPDIR
(make-corpus3.pl -nodowncase -maxeid $CutE -maxfid $CutF \
	-e $ALLP -f $ALLF -ve vocab.e -vf vocab.f -sym sym.tbl | \
	stat-child -filter -maxc 4 -maxid 65500 -maxf $MaxLenF | \
	split-bxi.pl -r 0.01 -train train.bxi -test test.bxi) 2> make-bxi.log
ghead -300 train.bxi > align.bxi

#
# delete concat corpus
#

rm -f $ALLE $ALLF $ALLP

# 
# Make cutoff files
#

echo "making cutoff files..."
echo "Start making cutoff files at `date`" >> $Log
make-ttables3.pl -bxi train.bxi -tbl Tables/prob -prog1 "$CoCount" -prog2 "$GenTTable" \
	-cutoff1 $TTCutoff1 -cutoff2 $TTCutoff2 2> make-cutoff.log

#
# all done
#

echo "done"
echo "Script finished at `date`" >> $Log

exit 0
