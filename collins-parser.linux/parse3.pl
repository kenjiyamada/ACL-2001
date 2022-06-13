#!/usr/bin/perl

# Usage: % parse3.pl < text.pos > text.parse

$TmpFile="/tmp/parse.$$";
$MaxLine=2500;
$MaxChar=1000;
$MaxWord=20;
$ParserDir="/usr/local/tools/collins-parser.linux/parser";
$ParserBin="$ParserDir/src3/parser.linux";
$ParserEvn="$ParserDir/events/model2.02-21.zip";
$ParserGrm="$ParserDir/grammars/model2.02-21";

$|=1;
open(TMP,">$TmpFile") || die "can't open tmpfile [$TmpFile] for write: $!\n";

$PosNewFmt=0;
while (@ARGV) {
    $_ = shift;
    /^-maxw$/ && ($MaxWord=shift,next);
    /^-maxc$/ && ($MaxChar=shift,next);
    /^-oldpos$/ && ($PosNewFmt=0,next);
    /^-newpos$/ && ($PosNewFmt=1,next);
    /^-/ && (die "unrecognized option: ",$_,"\n");
    unshift(@ARGV,$_); last;
}

$lineno=1;
while(<>) {
    chop;
    $line = $_;
    $line = &conv_posfmt($line) if (! $PosNewFmt);

    # check char length or word length
    $line = "1 SentenceTooLong NN" if (! &check_length($line));

    # put to tmpfile
    print TMP "$line\n";

    # run parser if necessary
    if ($lineno>=$MaxLine) {
	&run_parse;
	$lineno=1;
    } else {
	$lineno++;
    }
}

# run parser if necessary
&run_parse if $lineno > 1;

close(TMP);
unlink $TmpFile;
exit 0;

sub run_parse {
    close(TMP);
    $Command = "gunzip < $ParserEvn | $ParserBin $TmpFile $ParserGrm 10000 1 0";
    &show_date;
    print STDERR "running [$Command]...\n";
    system($Command);
    &show_date;
    open(TMP,">$TmpFile") || die "can't reopen tempfile [$TmpFile] for write: $!\n";
}

sub show_date {
    local ($date);
    $date=`date`;
    print STDERR "$date";
}

sub conv_posfmt {
    local ($line) = @_;
    local ($sep,$ret,$w,$i,$w1,$w2,$numw);

    # find the last '_' for each word, and replace it with ' '

    $ret=""; $sep=""; $numw=0;
    for $w (split(" ",$line)) {
	$i = rindex($w,"_");
	$w1 = substr($w,0,$i);	# before _
	$w2 = substr($w,$i+1);	# after _
	$ret .= "$sep$w1 $w2";
	$sep = " "; $numw++;
    }
    $ret = "$numw $ret";

    # also convert '()' into -LRB- and -RRB-
    $ret =~ s/\(/-LRB-/g;
    $ret =~ s/\)/-RRB-/g;

    $ret;
}


sub check_length {
    local ($line) = @_;
    local ($ret,$numc,$numw,@words);

    $numc = length($line);
    @words = split(" ",$line);
    $numw = ($#words+1-1)/2;	# -1 for num-words at BOL

    $ret = (($numc <= $MaxChar) && ($numw <= $MaxWord));
    $ret;
}
