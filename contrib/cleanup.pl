#!/usr/bin/perl -T -w

### Clean up old backup files according to desired schedule.
### This can be run daily on each machine where backups are stored;
### it will clean up the directories for all client machines.

###
# Distributed under the GPL version 2
# This is free software.  There is NO warranty; not even for MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.
#
# Contributed by: Boris Goldowsky

use strict;

our $usage = "USAGE: $0 [options]
Options:
  -v, --verbose  Give details about what was done and why.
  -n, --dry-run  Show what would be done, but do not actually run any backups.
  -h, --help     Show this help message.\n";

### Customize the variables below for your site
### Or, put all your settings in a configuration file and list it here:

our $conf_file = "/opt/hdup/backup-script.conf";

# Path should be sufficient to find "df"

$ENV{PATH}="/opt/hdup/sbin:/usr/sbin:/usr/bin:/bin";
$ENV{BASH_ENV} = '';

# Directory where hdup backup files are stored
our $basedir = "/backup";

# Keep all daily backups from the last N days
our $daily_keep_days = 13;

# Keep all weekly backups from the last N days 
# For dailies to be useful, this value should be >= $daily_keep_days,
# and you should keep all monthlies through this time period as well.
our $weekly_keep_days = 30;

# List desired approximate ages for monthlies to be kept.
# The number of elements in this list is the number of monthly dumps that
# will be kept at all times.
# Each element defines a target age; the best matches are kept.
# The default keeps the newest monthly, plus approx. 1, 3, and 6 months ago.
our @monthly_keep = (180, 90, 30, 0);

######################################################
### You shouldn't have to change anything below here
######################################################

# Function prototypes
sub delete_file_and_dir ($);
sub remove_backups ($$);
sub age_of ($);

# Command line options defauls here, so conf file can override them:
our $verbose = 0;
our $dry_run = 0;

if (-r $conf_file) {
    do $conf_file;
    warn "Couldn't parse $conf_file: $@" if $@;
}

# Parse command line
while ($_ = shift) {
    if (/^-h$/ or /^--help$/) { 
	print "$usage";
	exit 0;
    }
    if (/^-v$/ or /^--verbose$/) {
	$verbose = 1;
	next;
    }
    if (/^-n$/ or /^--dry-run$/) {
	$dry_run = 1;
	next;
    }
    print STDERR "Unknown option: $_\n$usage";
    exit 1;
}

if ($weekly_keep_days < $daily_keep_days) {
    print STDERR "Unreasonable parameters:
  \$weekly_keep_days ($weekly_keep_days) is less than \$daily_keep_days ($daily_keep_days).
  But the older daily backups will not be useful without preceding weeklies.
";
    exit 1;
}

remove_backups ($daily_keep_days, "*daily*");
	
remove_backups ($weekly_keep_days, "*weekly*");

# Algorithm for monthlies:
#   Generate list of ages of existing monthlies.
#   For each desired age, starting with oldest:
#    Find closest age match from existing backups.
#    Mark that one as a keeper, and remove it from the list (can't use twice)
#   Finally, delete monthlies that are not marked as keepers.

# Future enhancements:
#   should check for zero-length files.
#   should force keeping monthlies up to and just past $weekly_keep_days.

foreach my $dir (glob ("$basedir/*")) {
    ## Security check on names:
    if ($dir =~ m{\A($basedir/\w[\w .-]*)\Z}) {
	$dir = $1;
    } else {
	print STDERR "Directory does not match expected pattern: $dir";
	next;
    }
    my %ages = ();
    my @files = glob ("$dir/*/*monthly.tar*");
    if ($#files <= $#monthly_keep) {
	# print "\nNo extra monthlies in $dir\n" if ($verbose);
    } else {
	print "\nAges of monthlies in $dir:\n" if ($verbose);
	for my $f (@files) {
	    ## For security, check filename against expected pattern
	    if ($f =~ m{\A($dir/\w[\w .-]*/\w[\w .-]*)\Z}i) {
		my $file = $1;
		my $age = age_of($file);
		printf "%4d %s\n", $age, $file;
		$ages{$age} = $file;
	    } else {
		print STDERR "File does not match expected pattern: $f";
	    }
	}
	
	for my $want (@monthly_keep) {
	    my $best_match = -1;
	    for my $age (keys %ages) {
		if ($best_match < 0
		    || abs($age-$want) < abs($best_match-$want)) {
		    $best_match = $age;
		}
	    }
	    if ($best_match>=0) {
		print "Best match for desired age $want is $best_match days old\n" if ($verbose);
		delete $ages{$best_match}; # remove from future consideration
	    }
	}

	# Remaining files are extraneous
	for my $age (keys %ages) {
	    if ($age < $weekly_keep_days) {
		print STDERR "Algorithm says to delete $ages{$age},
  but I'm keeping it so that weekly backups are still valid.
  You should change your setting of \@monthly_keep to keep all monthly backups
  for at least $weekly_keep_days days (the value of \$weekly_keep_days).\n";
	    } else {
		delete_file_and_dir ($ages{$age});
	    }
	}
    }
}

print "\n" . `df -lkh $basedir` if ($verbose);

sub remove_backups ($$)
{
    my ($age, $glob) = @_;
    my $header_printed = 0;
    foreach my $file (glob ("$basedir/*/*/$glob")) {
	## Security check on filename.  This is not just a good idea, it's required by -T.
	if ($file =~ m{\A($basedir/\w[\w .-]*/\w[\w .-]*/\w[\w .-]*)\Z}) {
	    $file = $1;
	} else {
	    print STDERR "File doesn't match expected pattern: $file\n";
	    next;
	}
	my $fileage = age_of($file);
	if ($fileage > $age) {
	    if ($verbose && not $header_printed) {
		print "Cleaning up $glob files older than $age days:\n";
		$header_printed = 1;
	    }
	    delete_file_and_dir ($file);
	}
    }
}

sub delete_file_and_dir ($)
{
    my $file = shift;
    my ($directory) = ($file =~ m{\A(.*)/[^/]+\Z});
    
    printf ("  Removing (age %2d) %s\n", age_of($file), $file) if ($verbose);
    if (not $dry_run) {
	if (unlink ($file) != 1) {
	    print STDERR "Problem deleting: $!\n";
	}
    }
    printf ("           and directory %s\n", $directory) if ($verbose);
    if (not $dry_run) {
	rmdir $directory or warn $!;
    }
}

## Return age of file, in days
sub age_of ($)
{
    return int ((time() - (stat($_[0]))[9])/3600/24);
}
