#!/usr/bin/perl -T -w

### Determine what type of backup to run, and start hdup process to do it.

###
# Distributed under the GPL version 2
# This is free software.  There is NO warranty; not even for MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.
#
# Contributed by: Boris Goldowsky

use strict;
use POSIX;

our $usage="USAGE: $0 [options]
Options:
  -v, --verbose  Give details about what was done and why.
  -n, --dry-run  Show what would be done, but do not actually run any backups.
  -h, --help     Show this help message.\n";

### Customize the variables below for your site
### Or, put all your settings in a configuration file and list it here:

our $conf_file = "/opt/hdup/backup-script.conf";

# Make PATH sufficient to find hdup, hostname, and ls:

$ENV{PATH}="/opt/hdup/sbin:/usr/sbin:/usr/bin:/bin";
$ENV{BASH_ENV} = '';

# Name of this host

our $host="hostname";

# Directory where hdup backup files are stored

our $basedir="/backup";

# Set these to the number of days that backups are to be considered current.
# So if you want weekly backups done every 7 days, set week to 6 (6 days good,
# the next day they'll be forced to be re-run).  If you want to run a daily
# backup every time this script is run, set day to 0.

our $month = 30;
our $week  =  6;
our $day   =  0;

# Additional args to pass to hdup running on this host:
our $hdup_args = "";

# Remote user & host to store backups on, expressed as @user@host.com
# For local backups, set to empty string:
our $remote = "";

######################################################
### You shouldn't have to change anything below here
######################################################

# Filenames to be checked to determine when previous backups were performed.

our $monthlyfile="inclist.monthly";
our $weeklyfile="inclist.weekly";
our $dailyfile="inclist.daily";

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

# Function CHECKFILE.
# takes two arguments: a FILENAME and an AGE.
# returns success (0) if and only if:
#  FILENAME exists
#  FILENAME is not zero length
#  FILENAME is newer than AGE days old.
# ie, returns true if there is a good-enough backup there.
sub checkfile ($$) {
    my $file = shift;
    my $age = shift;
    if ( ! -f $file ) {
	print "No $file\n";
	return 1;
    }

    if ( ! -s $file ) {
	print "$file is zero length\n";
	print `ls -l $file` if ($verbose);
	return 2;
    }

    my $daysold = POSIX::ceil ((time() - (stat($file))[9])/3600/24);
    if ($daysold > $age) {
	print "$file is $daysold days old: more than $age\n" if ($verbose);
	print `ls -l $file` if ($verbose);
	return 3;
    }

    print "$file is new enough ($daysold days old is <= $age):\n"
	. `ls -l $file` if ($verbose);
    return 0;
}

# Location of inclist files
my $indexdir="$basedir/$host/etc";

my $today = "";
if (checkfile ("$indexdir/$monthlyfile", $month )) {
    $today = "monthly";
} elsif (checkfile ("$indexdir/$weeklyfile", $week)) {
    $today = "weekly";
} elsif (checkfile ("$indexdir/$dailyfile", $day)) {
    $today = "daily";
} else {
    print "No need to run backup.\n";
    exit 0;
}

print "Running $today\n";

my $cmd = "hdup $hdup_args $today $host $remote";
print "  Command is $cmd\n";
if (!$dry_run) {
    print `hdup $hdup_args $today $host $remote`;
    exit $?;
}
