#!/usr/bin/perl

# it must sync with cnv-sym3.pl
# used for convert f0data
# Usage: % head -10 f0r.out | sed 's/^.*: //g' | cnv-sym3-f0.pl > nolaw.f0

# convert sym.tbl (VP-A => VP.A, NP_AND => NP^AND, X => .X.)

# format "LHS idx... -> RHS RHS..."

while(<>) {
    chop;
    @item = split;
    for ($i=0; $i<=$#item; $i++) {
	$sym = $item[$i];
	$item[$i] = &cnv_sym3($sym) if $sym ne "->";
    }
    printf "%s\n", join(" ",@item);
}
exit 0;

sub cnv_sym3 {
    local ($sym) = @_;

    $sym =~ s/-/./g;
    $sym =~ s/_/^/g;
    
    $sym = ".X." if $sym eq "X";
    $sym = ".TOP." if $sym eq "TOP";

    $sym;
}


