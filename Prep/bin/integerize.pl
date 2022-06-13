#!/usr/bin/perl

# Usage: 
# % make-vocab.pl a_000_f.txt > xf.vcb
# % integerize.pl -vocab xf.vcb [-nolatin1] [-stat file.stat] < a_000_f.txt > a_000_f.int

$MaxID=65500;

while(@ARGV) {
    $_ = shift;
    /^-nolatin1$/ && ($NoLatin1=1,next);
    /^-nodowncase$/ && ($NoDowncase=1,next);
    /^-vocab$/ && ($VocabFile=shift,next);
    /^-stat$/ && ($StatFile=shift,next);
    /^-maxid$/ && ($MaxID=shift,next);
    /^-/ && (die "unrecognized option: ",$_,"\n");
    unshift(@ARGV,$_); last;
}

# read vocab file
open(V,$VocabFile) || die "can't open vocab file [$VocabFile]: $! \n";
while(<V>) {
    chop;
    ($id, $word, $freq) = split;
    $Vocab{$word}=$id if $id <= $MaxID;
}
close(V);

# read input file

while(<>) {
    $NumLines++;
    chop;

    # downcase words (This must by sync with make-vocab.pl)
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

    # convert each word
    @words = split;
    $NumWords += ($#words + 1);
    @ids=();
    for $w (@words) {
	$id = $Vocab{$w};
	$id = $Vocab{"UNK"} if $id==0;
	push(@ids,$id);
    }

    # print converted sentence
    print join(" ",@ids),"\n";
}
exit 0;
