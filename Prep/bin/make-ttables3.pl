#!/usr/bin/perl

# make all t-tables
# Usage: % make-ttables.pl a_000_b a000  => produces a000.tbl-N

$MOD=20;
$Prog1="co-count3 -C";
$Prog2="gen-ttable3.sun64";
$Cutoff1=1;
$Cutoff2=4;

while(@ARGV) {
    $_ = shift;
    /^-bxi$/ && ($BXI=shift,next);
    /^-tbl$/ && ($TBL=shift,next);
    /^-prog1$/ && ($Prog1=shift,next);
    /^-prog2$/ && ($Prog2=shift,next);
    /^-mod$/ && ($MOD=shift,next);
    /^-cutoff1$/ && ($Cutoff1=shift,next);
    /^-cutoff2$/ && ($Cutoff2=shift,next);
    /^-/ && (die "unrecognized option: ",$_,"\n");
    unshift(@ARGV,$_); last;
}

die "need -bxi and -tbl args\n" if ($BXI eq "" || $TBL eq "");

$|=1;
for ($i=1; $i<=$MOD; $i++) {
    $tblFile = "${TBL}.tbl-$i";
    $idx = $i-1;
    $cutoff="-cutoff $Cutoff1 -cutoff2 $Cutoff2";
    $cmd="$Prog1 -mod $MOD -idx $idx < $BXI | $Prog2 $cutoff > $tblFile";
    print STDERR "$cmd\n";
    system($cmd);
}
exit 0;

