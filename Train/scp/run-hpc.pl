#!/usr/bin/perl

# run training job on hpc

# MFC from -r1.29 

# Job Controller vars
$MaxCpuPerNode=2;
$Cmd1File="run1.sh";
$Cmd2File="run2.sh";
$Log1File="run1.log";
$Log2File="run2.log";

#$JobDir=$ENV{"PBS_O_WORKDIR"};	# where this job was submited.
#die "PBS_O_WORKDIR is not available" if $JobDir eq "";
#chdir $JobDir;
$CurDir=`pwd`; chop($CurDir);
$CurNode=`hostname`; chop($CurNode);

# Job specific vars
$Rsh = "/usr/bin/rsh";
$UseDump=0;			# use dmp files
$DumpMode=0;			# copy&use dmp files (obslete)
$Iter=10;
$Label="train";
$TblName="Tables/prob";
$TestLabel="test";
$AlignLabel="align";
$LArg=0;
$MArg=0;
$L1Const=0;
$L1Vary=0;
$NullShr=0;
$EqlProb=0;
$Cutoff=0;
$ReTrainR=0;
$CMode=0;
$FMode=0;
$OldMode=$OldMode2=0;
$NumTServer=21;
$NumGServer=30;
$GServerPerNode=1;
$DoCmd1=1;
$DoCmd2=1;
$PSCmd="/bin/ps x";
$NoRun=0;

$|=1;
$AllArgs = join(" ",@ARGV);
while(@ARGV) {
    $_ = shift;
    /^-testfloor$/ && ($TestFloor=shift,next);
    /^-alignfloor$/ && ($AlignFloor=shift,next);
    /^-host$/ && ($Host=shift,next);
    /^-pscmd$/ && ($PSCmd=shift,next);
    /^-norun$/ && ($NoRun=1,next);
    /^-name$/ && ($Label=shift,$TblName="Tables/$Label",next);
    /^-nt$/ && ($NumTServer=shift,next);
    /^-ng$/ && ($NumGServer=shift,next);
    /^-G1$/ && ($GServerPerNode=1,next);
    /^-G2$/ && ($GServerPerNode=2,next);
    /^-G3$/ && ($GServerPerNode=3,next);
    /^-G4$/ && ($GServerPerNode=4,next);
    /^-G8$/ && ($GServerPerNode=8,next);
    /^-treeonly$/ && ($DoCmd1=0,$DoCmd2=1,next);
    /^-notree$/ && ($DoCmd1=1,$DoCmd2=0,next);
    /^-phprob$/ && ($DoCmd1=1,$DoCmd2=0,$Iter=1,$ExtPhrase=1,next);
    /^-showf0$/ && ($DoCmd1=1,$DoCmd2=0,$Iter=1,$ShowF0=1,next);
    /^-alltree$/ && ($DoCmd1=1,$DoCmd2=0,$Iter=1,$AllTree=1,next);
    /^-ppxonly$/ && ($DoCmd1=1,$DoCmd2=0,$Iter=1,$PpxOnly=1,next);
    /^-oldmode$/ && ($OldMode=1,next);
    /^-oldmode2$/ && ($OldMode2=1,next);
    /^-nomvdumps$/ && ($NoMvDumps=1,next);
    /^-d$/ && ($UseDump=1,next);
    /^-D$/ && ($UseDump=$DumpMode=1,next); # obslete
    /^-n$/ && ($Iter=shift,next);
    /^-t$/ && ($TblName=shift,next);
    /^-C$/ && ($CMode=1,next);
    /^-F$/ && ($FMode=shift,next);
    /^-L$/ && ($LArg=shift,next);
    /^-LC$/ && ($LArg=shift,$L1Const=1,next);
    /^-LV$/ && ($L1Vary=1,next);
    /^-M$/ && ($MArg=shift,next);
    /^-N$/ && ($NullShr=1,next);
    /^-Q$/ && ($EqlProb=1,next);
    /^-QC$/ && ($EqlProb=1,$Cutoff=1,next);
    /^-retrainR$/ && ($ReTrainR=1,next);
    /^-noupdateT$/ && ($NoUpdateT=1,next);
    /^-/ && (die "unrecognized option: ",$_,"\n");
    unshift(@ARGV,$_); last;
}

&get_nodelist unless $Host;	# PBS_NODEFILE -> %NumCPUs

# create command files first (easier to debug)

if ($DoCmd1) {
    &set_nodelist;		# %NumCPUs -> %Node1, %Node2
    &make_cmd1file;
}
if ($DoCmd2) {
    &set_nodelist;		# %NumCPUs -> %Node1, %Node2 (reset)
    &make_cmd2file;
}

# run commands

if ($NoRun) {
    print "please execute run[12].sh mannualy\n";
    exit 0;
}

if ($DoCmd1) {
    &check_nodes;
    &run_cmd1file;
    &wait_cmd1done;
    sleep(120) if $DoCmd2;	# for NFS sync
}
if ($DoCmd2) {
    &check_nodes;
    &run_cmd2file;
    &wait_cmd2done;
}

exit 0;


#
# PBS supplied nodelist functions
#

sub get_nodelist {
    local ($nodeFile);
    local ($badFile,%badNodes);

    # read nodes.bad if exist
    %badNodes=(); $badFile="nodes.bad";
    if (-e $badFile) {
	open(F,$badFile) || die "error opening $badFile: $!\n";
	while(<F>) {
	    chop;
	    $badNodes{$_}=1;
	}
	close(F);
    }

    # PBS_NODEFILE -> %NumCPUs 
    $nodeFile = $ENV{"PBS_NODEFILE"};
    die "PBS_NODEFILE not available\n" if $nodeFile eq "";
    %NumCPUs=();
    open(F,$nodeFile) || die "can't open nodeFile [$nodeFile]:$!\n";
    while(<F>) {
	chop;
	next if /hpc078/;	# hardware problem?
	next if $badNodes{$_};
	$NumCPUs{$_}++;
	$HeadNode=$_ unless $HeadNode;
    }
    close(F);
    system("cp $nodeFile ./nodes.list");

    # decrease NumCPU for this node
    $NumCPUs{$HeadNode}--;
}

sub set_nodelist {
    local ($node,$ncpu,$i);

    # set %Node1 for non-dedicated nodes, and %Node2 for dedicated nodes
    # %NumCPUs -> %Node1, %Node2
    @Node1=@Node2=();
    for $node (sort keys %NumCPUs) {
	$ncpu = $NumCPUs{$node};
	if ($ncpu == $MaxCpuPerNode) {
	    push(@Node2,$node);
	} else {
	    for (1..$ncpu) {
		push(@Node1,$node);
	    }
	}
    }
}

sub get_node {
    # get non-dedicated node
    local ($node,$ncpu);

    # quick hack for -host
    @Node1=($Host) if $Host;

    if (! @Node1) {
	# move one dedicated node to non-dedicated one
	die "no more nodes" unless @Node2;
	$node = shift(@Node2);
	$ncpu = $NumCPUs{$node};
	for (1..$ncpu) {
	    push(@Node1,$node);
	}
    }
    $node = shift(@Node1);
    $node;
}

sub get_full_node {
    # get dedicated node
    local ($node);

    # quick hack for -host
    @Node2=($Host) if $Host;

    die "no more dedicated nodes" unless @Node2;
    $node = shift(@Node2);
    $node;
}

sub check_nodes {
    local ($cmd,$nd);

    $|=1;
    print STDERR "checking nodes...";
    for $nd (keys %NumCPUs) {
	if ($NumCPUs{$nd}) {
	    print STDERR "[$nd]";
	    $cmd = "$Rsh $nd pwd";
	    system("$cmd < /dev/null > /dev/null");
	}
    }
    print STDERR "checking nodes done.\n";
}

#
# Job specific functions
#

#
# functions common to cmd1 and cmd2
#

# note: $Master has to be set before there are called.

sub make_tflag {
    local ($useDump) = @_;
    local ($tflag);

    # note: -eqlprob is not applied when useDump
    #       -cutoff is not applied here.
    # note: $TblName is modified when preparing dmp files in &make_cmd2file

    $tflag = "-host $Master";
    $tflag .= " -tbl $TblName.tbl-" if !$useDump;
    $tflag .= " -tbl $TblName.dmp- -nodmp" if $useDump;
    $tflag .= " -sharenull" if $NullShr;
    $tflag .= " -C" if $CMode;
    $tflag .= " -fmode $FMode" if $FMode;
    $tflag .= " -eqlprob" if ($EqlProb && !$useDump);
    $tflag .= " -oldmode" if $OldMode;
    $tflag .= " -oldmode2" if $OldMode2;
    $tflag .= " -lambda1vary" if $L1Vary;

    $tflag;
}

sub make_gflag {
    local ($gflag);

    $gflag = "-host $Master -lambda1 $LArg -lambda2 $MArg";
    $gflag .= " -lambda1const" if $L1Const;
    $gflag .= " -lambda1vary" if $L1Vary;
    $gflag .= " -C" if $CMode;

    $gflag;
}

#
# functions for cmd1
#

sub make_cmd1file {
    local ($tflag,$t0flag,$gflag,$node,$tidx,$gidx);
    local ($testFile,$useDump,$cutoff);
    
    open(F1,">$Cmd1File") || die "can't open Cmd1File [$Cmd1File] for write:$!\n";
    print F1 "#!/bin/sh\n\n";
    print F1 "# Command=[$0 $AllArgs] running on [$CurNode ($HeadNode)]\n\n";
    printf F1 "cd %s\n\n",$CurDir;

    # set local flags
    $testFile = "${TestLabel}.bxi";
    $testFile = "" unless -T $testFile;
    $cutoff = $Cutoff;
    $useDump = 0;

    # override local flags for ExtPhrase
    if ($ExtPhrase || $ShowF0 || $AllTree || $PpxOnly) {
	$testFile = "";
	$cutoff = 0;
	$useDump = 1;

	# also add extra args to $gflags. see below
    }


    # run master
    $Master = $CurNode;		# need to be set to call &make_tflag

    print F1 "./train -train $Label.bxi -nt $NumTServer";
    print F1 " -test $testFile" if $testFile;
    print F1 " -C" if $CMode;
    print F1 " -ng $NumGServer -n $Iter > $Label.out < /dev/null &\n";
    print F1 "echo sleeping 10sec...\n";
    print F1 "sleep 10\n\n";
    
    # tbl-0,1
    $tflag = &make_tflag($useDump);

    print F1 "echo starting tbl-0 and tbl-1\n";

    $node = &get_full_node;
    $t0flag = $tflag;
    $t0flag .= " -eqlprob" if $ReTrainR;
    print F1 "$Rsh $node \"cd $CurDir; (./tserver $t0flag -idx 0";
    print F1 " ) >& tbl-0.log \" < /dev/null &\n";

    # add -cutoff here
    $tflag .= " -cutoff" if $cutoff;

    $tflag .= " -noupdate" if $NoUpdateT;

    $node = &get_node;
    print F1 "$Rsh $node \"cd $CurDir; (./tserver $tflag -idx 1";
    print F1 " ) >& tbl-1.log \" < /dev/null &\n\n";
    
    # other t-servers
    print F1 "echo starting other tservers...\n";
    for $tidx (2..$NumTServer-1) {
	$node = &get_node;
	print F1 "$Rsh $node \"cd $CurDir; (./tserver $tflag -idx $tidx";
	print F1 " ) >& tbl-$tidx.log \" < /dev/null &\n";
    }
    print F1 "\n";

    # g-servers

    print F1 "echo starting gservers...\n";
    $gflag = &make_gflag;

    $gflag .= " -voce vocab.e -vocf vocab.f -sym sym.tbl"
	if ($ExtPhrase || $ShowF0 || $AllTree);
    $gflag .= " -phprob" if $ExtPhrase;
    $gflag .= " -showf0" if $ShowF0;
    $gflag .= " -uprob2 -tree" if $AllTree;
    $gflag .= " -testfloor $TestFloor" if $TestFloor;

    $numGNodes = int($NumGServer / $GServerPerNode);
    $numGNodes++ if $GServerPerNode * $numGNodes < $NumGServer;

    $gidx=1;
    while ($gidx <= $NumGServer) {
	$node = &get_full_node;
	for (1..$GServerPerNode) {
	    if ($gidx <= $NumGServer) {
		print F1 "$Rsh $node \"cd $CurDir; (./gserver $gflag";
		print F1 ") >& g$gidx.log \" < /dev/null &\n";
		$gidx++;
	    }
	}
    }

    print F1 "echo all started.\n";
    close(F1);
    chmod 0700,$Cmd1File;
}

sub run_cmd1file {
    system("./$Cmd1File > $Log1File 2>&1");
}

sub wait_cmd1done {
    local ($ps,$done,@lines,$line,@cmds,$cmd);
    $done=0;
    while (!$done) {
	#$ps = `/bin/ps x`;
	$ps = `$PSCmd`;
	@lines=split(/\n/,$ps);
	$done=1;
	for $line (@lines) {
	    $line =~ s/^\s+//g;
	    @cmds=split(/\s+/,$line);
	    $cmd = $cmds[4];
	    $cmd =~ /train/ && ($done=0,last);
	}
	last if $done;
	#print STDERR "sleeping.";
	sleep(30);
    }
}    

#
# functions for cmd2
#

sub make_cmd2file {
    local ($gflag,$node,$tidx);

    open(F2,">$Cmd2File") || die "can't open Cmd2File [$Cmd2File] for write:$!\n";
    print F2 "#!/bin/sh\n\n";
    print F2 "# Command=[$0 $AllArgs] running on [$CurNode ($HeadNode)]\n\n";
    printf F2 "cd %s\n\n",$CurDir;

    # prepare dump file
    if (! $NoMvDumps) {
	print F2 "# copy dmp files\n";
	print F2 "echo -n copying dmp files...\n";
	print F2 "rm -f Dumps/*\n";
	print F2 "dmps=`(cd Tables; echo *dmp)`\n";
	print F2 'for dmp in $dmps',"\n";
	print F2 "do\n";
	print F2 '  (cd Dumps; ln -s ../Tables/$dmp)',"\n";
	print F2 "done\n";
	print F2 "(cd Dumps; ../mvdump.sh prob 20)\n";
	print F2 "echo done\n\n";
    }

    $TblName = "Dumps/prob";

    # run master
    $Master = $CurNode;
    print F2 "./train -train ${AlignLabel}.bxi -nt $NumTServer";
    print F2 " -C" if $CMode;
    print F2 " -ng 1 -n 1 > ${AlignLabel}.out < /dev/null &\n";
    print F2 "echo sleeping 10sec...\n";
    print F2 "sleep 10\n\n";
    
    # tbl-0,1
    $tflag = &make_tflag(1);

    print F2 "echo starting tbl-0 and tbl-1\n";

    $node = &get_full_node;
    print F2 "$Rsh $node \"cd $CurDir; ./tserver $tflag -idx 0";
    print F2 " > /dev/null \" < /dev/null &\n";

    $node = &get_node;
    print F2 "$Rsh $node \"cd $CurDir; ./tserver $tflag -idx 1";
    print F2 " > /dev/null \" < /dev/null &\n\n";
    
    # other t-servers
    print F2 "echo starting other tservers...\n";
    for $tidx (2..$NumTServer-1) {
	$node = &get_node;
	print F2 "$Rsh $node \"cd $CurDir; ./tserver $tflag -idx $tidx";
	print F2 " > /dev/null \" < /dev/null &\n";
    }
    print F2 "\n";

    # g-server
    print F2 "echo starting a gserver...\n";
    $gflag = &make_gflag;
    $gflag .= " -voce vocab.e -vocf vocab.f -sym sym.tbl";
    $gflag .= " -trainfloor $TestFloor" if $AlignFloor;
    
    $node = &get_full_node;
    print F2 "$Rsh $node \"cd $CurDir; ./gserver $gflag -tree -uprob";
    print F2 " > ${AlignLabel}.tree \" < /dev/null &\n";

    close(F2);
    chmod 0700,$Cmd2File;
}

sub run_cmd2file {
    system("./$Cmd2File > $Log2File 2>&1");
}

sub wait_cmd2done {
    # same as wait_cmd1done
    &wait_cmd1done;
}
