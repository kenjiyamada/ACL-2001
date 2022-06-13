#!/usr/bin/perl

# setup directories and files for training on localhost

# Usage: % mkdir LVset ; cd LVset ; create train.conf ; /usr/local/tools/Stx/scp/setup-train.pl

# assumes ../Data/ exists

$Base="/usr/local/tools/Stx/Train";		# for scp/train.conf
$Wrk=`pwd`;
$Conf="train.conf";

&read_config("$Base/$Conf");
&read_config("$Conf");
#&show_params;exit 0;

#
# setup main directory (where this command is executed)
#

$ROOT=$Base;
$BinFiles="train gserver tserver";
$ScpFiles="run-hpc.pl mvdump.sh";
$DataFiles="train.bxi test.bxi align.bxi vocab.e vocab.f sym.tbl";
$JobFile="job.sh";


die "../Data/ directory missing" if (! -d "../Data");
&setup_links;
&setup_tables;

$PhDir="Ph";
if ($Param{"MakePh"} eq "yes") {
    mkdir($PhDir,0755) if (! -d $PhDir);
    chdir($PhDir);
    symlink("../Data","Data");
    &setup_links;
    &setup_dumps;
    chdir("..");
}

$F0Dir="F0";
if ($Param{"MakeF0"} eq "yes") {
    mkdir($F0Dir,0755) if (! -d $F0Dir);
    chdir($F0Dir);
    symlink("../Data","Data");
    &setup_links;
    &setup_dumps;
    chdir("..");
}

&make_job;

exit 0;

sub read_config {
    local ($file) = @_;
    local ($line,$name,$val);

    if (-f $file) {
	print STDERR "reading $file...\n";
	open(F,$file) || die "can't open config [$file]: $! \n";
	while(<F>) {
	    chop;
	    s/#.*$//g;		# remove comments
	    s/^\s*//g;		# remove leading spaces
	    s/\s*$//g;		# remove training spaces
	    next if /^$/;
	    if (/^(.*)\s*=\s*"(.*)"$/) {
		($name,$val) = ($1,$2);
	    } elsif (/^(.*)\s*=\s*(\S+)$/) {
		($name,$val) = ($1,$2);
	    } else {
		die "unrecognized conf line: [$_]\n";
	    }
	    $Param{$name}=$val;
	}
	close(F);
    }
}

sub show_params {
    local ($key,$val);

    while (($key,$val) = each %Param) {
	printf "%s=[%s]\n", $key,$val;
    }
}

sub setup_links {
    local ($f);

    symlink("$ROOT/bin","bin");
    symlink("$ROOT/scp","scp");
    symlink("../Data","Data");

    for $f (split(" ",$BinFiles)) {
	symlink("bin/$f",$f);
    }

    for $f (split(" ",$ScpFiles)) {
	symlink("scp/$f",$f);
    }

    for $f (split(" ",$DataFiles)) {
	symlink("Data/$f",$f);
    }
}

sub setup_tables {
    local ($i,$f);

    mkdir("Tables",0755);
    mkdir("Dumps",0755);

    # use Data/Tables/prob.tbl-1 as Tables/prob.tbl-1
    for $i (1..20) {
	$f = "prob.tbl-$i";
	symlink("../Data/Tables/$f","Tables/$f");
    }
}

sub setup_dumps {
    local ($i,$f,$d);
    
    mkdir("Tables",0755);
    mkdir("Dumps",0755);

    # use ../Tables/prob.tbl-1.dmp as Tables/prob.dmp-1
    for $i (0..20) {
	$f="prob.tbl-$i.dmp";
	$d="prob.dmp-$i";
	symlink("../../Tables/$f","Tables/$d");
    }
}

sub make_job {
    local ($n,$cmd,$lmode);
    local ($phcmd,$f0cmd);

    open(F,">$JobFile") || die "can't make job file [$JobFile]: $!\n";
    print F "#!/bin/sh\n\n";
    print F "#PBS -k eo\n";
    printf F "#PBS -l walltime=%s,nodes=%d:ppn=2\n\n",$Param{"Walltime"},$Param{"Nodes"};
    print F "[ ! -z \"\$PBS_O_WORKDIR\" ] && cd \$PBS_O_WORKDIR\n\n";
#    print F "cd \$PBS_O_WORKDIR\n\n";

    $lmode=$Param{"LMode"};
    $n=$Param{"NIter"};
    $cmd = &make_cmd($lmode,"-n $n");
    print F "$cmd\n\n";

    if ($Param{"MakePh"} eq "yes") {
	print F "cd $PhDir\n";
	print F "sleep 120\n";
	$cmd = &make_cmd($lmode,"-phprob");
	$phcmd = &make_phcmd;
	print F "$cmd\n\n";
	print F "sleep 120\n";
	print F "$phcmd\n\n";
	print F "cd ..\n\n";
    }

    if ($Param{"MakeF0"} eq "yes") {
	print F "cd $F0Dir\n";
	$lmode=$Param{"LModeF0"};
	$lmode=$Param{"LMode"} if $lmode eq "";
	$cmd = &make_cmd($lmode,"-showf0");
	$f0cmd = &make_f0cmd;

	print F "$cmd\n";
	print F "sleep 120\n";
	print F "$f0cmd\n\n";
	print F "cd ..\n\n";
    }

    close(F);
    chmod(0755,$JobFile);
    
}

sub make_cmd {
    local ($lmode,$xarg) = @_;
    local ($cmd);

    $cmd=sprintf("./run-hpc.pl %s -pscmd \"%s\" %s -ng %s", 
		  $Param{"Host"}, $Param{"PScmd"}, $Param{"GProcs"}, $Param{"GSrvs"});
    $cmd.=sprintf(" %s", $Param{"Mode"});
    $cmd.=sprintf(" %s -testfloor %s -alignfloor %s", 
                  $Param{"CFlag"}, $Param{"TestFloor"},$Param{"AlignFloor"});
    $cmd.=sprintf(" %s %s %s", $Param{"BaseArgs"},$lmode,$xarg);

    $cmd;
}

sub make_phcmd {
    local ($cmd);

    $cmd = "grep -h ProbPH: g*log | sort | uniq -c | sort -rn > train.phc";
    $cmd;
}

sub make_f0cmd {
    local ($cmd,$f0w,$f0r);

    $f0w = $Param{"NumF0Words"};
    $f0r = $Param{"NumF0Rules"};

    $cmd =  "cat g*.log | grep '^F0Word:' | sort | uniq -c | sort -rn > f0w.out\n";
    $cmd .= "cat g*.log | grep '^F0Rule:' | sort | uniq -c | sort -rn > f0r.out\n";
    $cmd .= "grep -v \"UNK\" f0w.out | head -${f0w} | awk '{print \$NF}' > fert0.wd\n";
    $cmd .= "head -${f0r} f0r.out | sed 's/^.*: //g' | scp/cnv-sym3-f0.pl > fert0.rl\n";
    $cmd .= "awk 'NF==5' fert0.rl > fert0.rlx\n";

    $cmd;
}


