#!/usr/bin/perl

# make corpus
# usage: % make-corpus.pl -e a_000_e.parse -f a_000_f -ve xe.vcb -vf xf.vcb -sym x.sym> x.bxi

# filter out unparsed sent (SentTooLong or Prob 0).
# call integerize.pl for F, call modtree3 -convsym for E, and combine them to BXI

$TmpDir=$ENV{"TMPDIR"};
$TmpDir="/tmp" if $TmpDir eq "";

$TempE="$TmpDir/mkcorpus.$$.e";
$TempF="$TmpDir/mkcorpus.$$.f";
$TempE2="$TmpDir/mkcorpus.$$.e2";
$TempF2="$TmpDir/mkcorpus.$$.f2";

$MaxID=0;

while(@ARGV) {
    $_ = shift;
    /^-e$/ && ($FileE=shift,next);
    /^-f$/ && ($FileF=shift,next);
    /^-ve$/ && ($VocabFileE=shift,next);
    /^-vf$/ && ($VocabFileF=shift,next);
    /^-sym$/ && ($SymbolFile=shift,next);
    /^-maxid$/ && ($MaxEID=$MaxFID=shift,next);
    /^-maxeid$/ && ($MaxEID=shift,next);
    /^-maxfid$/ && ($MaxFID=shift,next);
    /^-nodowncase$/ && ($NoDowncase=1,next); # apply only French side
    /^-/ && (die "unrecognized option: ",$_,"\n");
    unshift(@ARGV,$_); last;
}

$ArgMaxEID = ($MaxEID==0) ? "" : "-maxvid $MaxEID";
$ArgMaxFID = ($MaxFID==0) ? "" : "-maxid $MaxFID";
$ArgDowncase = $NoDowncase ? "-nodowncase" : "";

# read FileE and FileF, and create temp files

open(FE,$FileE) || die "can't open FileE [$FileE]: $!\n";
open(FF,$FileF) || die "can't open FileF [$FileF]: $!\n";
open(TMPE,">$TempE") || die "can't open TempE [$TempE]: $!\n";
open(TMPF,">$TempF") || die "can't open TempF [$TempF]: $!\n";

while(<FE>) {
    $lineE = $_;
    if (/^PROB 0 0 0$/) {
	$parse_ok=0;
    } elsif (/^PROB/) {
	$parse_ok=1;
    } elsif (/SentenceTooLong/) {
	$lineF = <FF>;		# read FileF regardless of $parse_ok
    } else {
	# now about to use this parse
	$lineF = <FF>;
	if ($parse_ok) {
	    print TMPE $lineE;
	    print TMPF $lineF;
	}
    }
}
close(TMPF); close(TMPE); close(FF); close(FE);

# apply integerizer for both E and F

# filter-parse.pl must work with no PROB lines
system("filter-parse3.pl $TempE | modtree3 -conv punc.tbl -upqpunc -rmpunc -lower -vocab $VocabFileE -convsym $SymbolFile -noorg $ArgMaxEID > $TempE2");
system("rmpunc.pl < $TempF | integerize.pl -vocab $VocabFileF $ArgMaxFID $ArgDowncase > $TempF2");

# combine them
open(FE2,$TempE2) || die "can't open TempE2 [$TempE2]: $!\n";
open(FF2,$TempF2) || die "can't open TempF2 [$TempF2]: $!\n";
while(<FE2>) {
    $lineE=$_;
    $lineF=<FF2>;
    print "$lineE$lineF\n";
}
close(FF2); close(FE2);

# cleanup
unlink $TempE,$TempF,$TempE2,$TempF2;

exit 0;

