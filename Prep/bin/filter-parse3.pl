#!/usr/bin/perl

# filter out SentenceTooLong and PROB 0 parse
# also perform 'fix-paren'

# modified from filter-parse.pl, for new parser format (PROB 0 0 0)
# also removed fix-paren part (ok?)

while(<>) {
    if (/^PROB 0 0 0$/) {	# new parser format
	$off=1;
    } elsif (/^PROB/) {
	$off=0;
    } elsif (/SentenceTooLong/) {
	# skip
    } elsif (/SentenceNotParsed/) {
	# skip
    } else {
	# perform 'fix-paren'
#	s,\\,\\\\,g;		# \ -> \\
#	s,\(\(/,\\\(\(/,g;	# ((/ => \((/  # work with next pattern
#	s,\(/,\\\(/,g;		# (/ => \(/
#	s,\)\)/,\\\)\)/,g;	# ))/ => \))/  # work with next pattern
#	s,\)/,\\\)/,g;		# )/ => \)/
#	s,/\(,/\\\(,g;		# /( => /\(
#	s,/\),/\\\),g;		# /) => /\)
#	s,~\(\(,~\(\\\(,g;	# ~(( => ~(\( # work with next pattern
#	s,~\(,~\\\(,g;		# ~( => ~\(
#	s,~\)\),~\)\\\),g;	# ~)) => ~)\) # work with next pattern
#	s,~\),~\\\),g;		# ~) => ~\)

	print unless $off;
    }
}
exit 0;

	
