#!/usr/bin/perl

# split BXI into test/train
# Usage: % split-bxi.pl -r 0.01 -train $BXI -test $BXIT < $TMPBXI

$Ratio=0.01;
while (@ARGV) {
    $_ = shift;
    /^-r$/ && ($Ratio=shift,next);
    /^-train$/ && ($TrainBXI=shift,next);
    /^-test$/ && ($TestBXI=shift,next);
    /^-/ && (die "unrecognized argument: ",$_,"\n");
    unshift(@ARGV,$_); last;
}

open(TR,">$TrainBXI") || die "can't open TrainBXI for write [$TrainBXI]: $!\n";
open(TS,">$TestBXI") || die "can't open TestBXI for write [$TestBXI]: $!\n";

$Every=1.0/$Ratio;
$NumPairs=0; $lineno=0;
while(<>) {
    $mod = $lineno++ % 3;
    if ($mod==0) {
	$pair = $_;
    } elsif ($mod==1) {
	$pair .= $_;
    } elsif ($mod==2) {
	die "non-null line found: $lineno:[$_]\n" if $_ ne "\n";
	if ($NumPairs++ % $Every == 0) {
	    print TS "$pair\n";
	} else {
	    print TR "$pair\n";
	}
	$pair="";
    }
}
close(TR); close(TS);
exit 0;
