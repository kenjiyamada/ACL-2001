#!/usr/bin/perl

# Usage: % make-vocab.pl [-nolatin1] [-stat file.stat] < plain.txt > vocab.vcb

while(@ARGV) {
    $_ = shift;
    /^-nolatin1$/ && ($NoLatin1=1,next);
    /^-nodowncase$/ && ($NoDowncase=1,next);
    /^-stat$/ && ($StatFile=shift,next);
    /^-/ && (die "unrecognized option: ",$_,"\n");
    unshift(@ARGV,$_); last;
}

# read input file

while(<>) {
    $NumLines++;
    chop;
    if (! $NoDowncase) {
	tr/A-Z/a-z/;
	if (! $NoLatin1) {
	    # taken from fix-latin1.pl
	    tr/ÀÁÂÃÄÅÆ/àáâãäåæ/;
	    tr/ÇÈÉÊË/çèéêë/;
	    tr/ÌÍÎÏ/ìíîï/;
	    # tr/Ğ/ğ/;
	    tr/Ñ/ñ/;
	    tr/ÒÓÔÕÖ/òóôõö/;
	    tr/×Ø/xø/;
	    tr/ÙÚÛÜ/ùúûü/;
	    tr/İŞ/ış/;
	}
    }
    @words = split;
    $NumWords += ($#words + 1);
    for $w (@words) {
	$Word{$w}++;
    }
}

# print result

$fmt = "%d\t%s\t%d\n";
$MaxReserved=10;
printf $fmt, 0,"NULL",0;
printf $fmt, 1,"UNK",0;
printf $fmt, 2,"<s>",0;
printf $fmt, 3,"</s>",0;
for ($i=4; $i<=$MaxReserved; $i++) {
    printf $fmt, $i, "--reserved.$i--",0;
}

@keys = sort by_freq keys %Word;
$n=$MaxReserved+1;
for $w (@keys) {
    printf $fmt, $n, "$w", $Word{$w};
    $n++;
}

print STDERR "Number of lines: $NumLines\n";
print STDERR "Number of words: $NumWords\n";

exit 0;

sub by_freq {
    $Word{$b} <=> $Word{$a};
}

