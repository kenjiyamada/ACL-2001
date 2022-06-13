#!/usr/bin/perl

# remove PUNC (chinese GB version)

# NOTE: additional punc were added on Nov 6, 2001

while(<>) {
    chop;
    $_ = " $_ ";

    # should sync with punc.tbl for modtree3
    s/ £¬ / /g;
    s/ ¡£ / /g;
    s/ £º / /g;

    # additional punc added on Nov 6, 2001
    s/ ¡¢ / /g;
    s/ , / /g;
    s/ : / /g;

    #s/ \" / /g;
    #s/ '' / /g;

    s/^ //;
    s/ $//;
    print "$_\n";
}
exit 0;


