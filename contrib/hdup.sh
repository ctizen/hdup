#!/bin/bash

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABLILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details. You should have received a
# copy of the GNU General Public License along with this program; if
# not, write to the Free Software Foundation, Inc, 59 Temple Place -
# Suite 330, Boston, MA 02111-1307, USA.

# On Debian systems, the complete text of the GNU General Public
# License can be found in /usr/share/common-licenses/GPL file.

# For questions please contact: Juraj.Kubelka@email.cz

function snapshot_name {
# $1 is name of LVM Logical Volume
# Return name of Snapshot for this LVM Logical Volume
echo "${1##*/}_SNAPSHOT"
}

function snapshot_name_long {
# $1 is name of LVM Logical Volume
# Return name of Snapshot for this LVM Logical Volume
#        - in logn form (/dev/.../...)
echo "${1%/*}/$(snapshot_name "$1")"
}

function snapshot_directory {
# $1 is name of LVM Logical Volume
# Return name of directory where you can mount its snapshot
echo "/$(snapshot_name "$1")"
}

function create_snapshot {
# $1 is name of LVM Logical Volume for what you want to create snapshot
#    ... required
# $2 is name of directory where you want to mount this snapshot
# $3 is size of snapshot you want to create

if [ -z "$1" ] ; then
    echo "Specify LVM Logical Volume for what you want to create snapshot" >& 2
fi

SNAP_NAME="$(snapshot_name "$1")"
SNAP_NAME_LONG="$(snapshot_name_long "$1")"
SNAP_MOUNT_POINT="${2:-$(snapshot_directory "$1")}"
SNAP_SIZE=${3:-4G}

lvcreate -L"$SNAP_SIZE" -s -n "$SNAP_NAME" "$1" &&
mkdir "$SNAP_MOUNT_POINT" &&
mount -o ro "$SNAP_NAME_LONG" "$SNAP_MOUNT_POINT"

return $?
} # create_snapshot

function remove_snapshot {
# $1 is name of LVM Logical Volume you want to umount and remove its Snapshot
#    ... required
# $2 is name of directory where this snapshot is mounted

if [ -z "$1" ] ; then
    echo "Specify LVM Logical Volume you want to remove its snapshot" >& 2
fi

SNAP_NAME="$(snapshot_name "$1")"
SNAP_NAME_LONG="$(snapshot_name_long "$1")"
SNAP_MOUNT_POINT="${2:-$(snapshot_directory "$1")}"

umount "$SNAP_MOUNT_POINT"
rmdir "$SNAP_MOUNT_POINT"
lvremove -f "$SNAP_NAME_LONG"
} # remove_snapshot


LOCK_FILE="/tmp/hdup.lock"
    
function unlock_backups {

    rm -f $LOCK_FILE
} # unlock_backups

function lock_backups {
# $1 is scheme you want to use (daily, weekly, monthly)

    lockfile -0 -r1 $LOCK_FILE && \
	trap 'unlock_backups ; exit 1' 1 2 3 15 || \
	( 
	echo "HDUP is runnig (scheme: $(cat $LOCK_FILE))." >& 2
	return 1
	) || return 1

    chmod u+w $LOCK_FILE &&
    echo -n "$1" > $LOCK_FILE &&
    chmod u-w $LOCK_FILE ||
    ( 
	unlock_backups
	return 1
    ) || return 1
} # lock_backups

function backup_host {
# $1 is scheme you want to use (daily, weekly, monthly)
# $2 is name of host you want to backup

    echo ""
    echo "+++ Start hdup backup: $1, $2 +++" 
    nice -n 2 hdup -q -q "$1" "$2"
    RET=$?
    echo "+++  Done hdup backup: $1, $2 +++"
    echo ""
    return $RET
 } # backup_host

function backup_all_hosts {
# $1 is scheme you want to use (daily, weekly, monthly)
    
    for i in $(cat /etc/hdup/hdup.conf \
	| grep -e '^\[[^[]*\]' \
	| grep -v global \
	| sed -e 's/\[//g' -e 's/\]//g')
      do 
      backup_host "$1" "$i" || \
	  return 1
    done

    run_post_backups "$1"
} # backup_all_hosts

function backup_all_hosts_with_lock {
# $1 is scheme you want to use (daily, weekly, monthly)
    
    if lock_backups "$1" ; then
	backup_all_hosts "$1" &&
	unlock_backups "$1" ||
	(
	    echo "An error during backup." >& 2
	    echo "  Write \`$0 unlock\` before new start." >& 2
	    return 1
	) || return 1
    fi
} # backup_all_hosts_with_lock

POST_BACKUPS_ADR="/etc/hdup/post-backup.d"

function run_post_backups {
# $1 is scheme you want to use (daily, weekly, monthly)

    export HDUP_SCHEME="$1"

    [ ! -d "$POST_BACKUPS_ADR" ] && return 0

    echo ""
    echo "+++ Start post backups: $1 +++"

    ls "$POST_BACKUPS_ADR" \
	| \
	while read backup_script
      do
      [ ! -x "$POST_BACKUPS_ADR/$backup_script" ] && continue
      
      echo -n "Script ($backup_script): "
      "$POST_BACKUPS_ADR/$backup_script" && \
	  echo "done." || \
	  echo "An ERROR!!!"
    done
    
    echo "+++ Done post backups: $1 +++"
    echo ""
} # run_post_backups

HDUP_OUTPUT="/tmp/hdup.output.$$.txt"

function backup_and_send_email {
# Backup and send email with backup results.
# $1 is scheme you want to use (daily, weekly, monthly)
    
    HDUP_START_DATE="$(date)"
    backup_all_hosts_with_lock $* 2>&1 | tee "$HDUP_OUTPUT"
    HDUP_END_DATE="$(date)"

    (
	echo "HDUP start time: $HDUP_START_DATE."
	echo "HDUP end time  : $HDUP_END_DATE."
	echo ""
	cat "$HDUP_OUTPUT"
    ) \
	| mail -s "HDUP backup, scheme $1." root@localhost

    rm -f "$HDUP_OUTPUT"
} # backup_and_send_email

function help {

cat <<EOF

Wrapper for hdup application 
 (see http://miek.nl/projects/hdup2/index.html).

Questions/remarks should go to: Juraj.Kubelka@email.cz

USAGE: ${0##*/} <command> [parameters]

Commands for backup:

  backup <scheme>
        - backup all hosts
  backup-by-email <scheme>
        - backup all hosts and send report by email
          Email is sent to root@localhost.
          There is used screen application.
  backup-by-email-no-screen <scheme>
        - backup all hosts and send report by email
          There isn't used screen application.

  parameters:
     scheme = { daily | weekly | monthly }
        - see man hdup

Commands for support backup:

  lock  
        - lock backup process
          Preserve before run backup process. It can be used for
          disable automatic run backup process.

  unlock
        - unlock backup process

  run-post-backups [scheme]
        - rerun post backup process manually
          Usually it is run after all hosts are backup.
          Scripts are located in ${POST_BACKUPS_ADR} directory.

          When it is used automatically after backup process, variable
          HDUP_SCHEME is set to scheme { daily | weekly | monthly }.
          So it can be used inside scripts.

  create-snapshot <LVM-name> [mount-point] [Snapshot-size]
        - Create snapshot of LVM-name (see man lvcreate). Size will be
          set to Snapshot-size. Mount point will be mount-point.

  remove-snapshot <LVM-name> [mount-point]
        - Remove snapshot of LVM-name.

          When I've tested it last time (about February 2005), there
          were some kernel problems with allocating memory for
          "snapshot table". So now I don't use it.

          Idea:
          Write in hdup config file (/etc/hdup/hdup.conf) scheme:

          [scheme-usr-local]
          dir = /SYSTEM-USR-SNAPSHOT/local
          prerun = ${0} create-snapshot /dev/SYSTEM/USR /SYSTEM-USR-SNAPSHOT
          postrun = ${0} remove-snapshot /dev/SYSTEM/USR /SYSTEM-USR-SNAPSHOT

EOF

} # help

# --------------------------------------------

if [ "$1" = "create-snapshot" ] ; then
    shift
    create_snapshot $*
elif [ "$1" = "remove-snapshot" ] ; then
    shift
    remove_snapshot $*

elif [ "$1" = "lock" ] ; then
    lock_backups testing
elif [ "$1" = "unlock" ] ; then
    unlock_backups 

elif [ "$1" = "backup" ] ; then
    shift
    backup_all_hosts_with_lock $*
elif [ "$1" = "backup-by-email" ] ; then
    shift
    screen -t hdup.backup."$1" bash -c "$0 backup-by-email-no-screen $*"
elif [ "$1" = "backup-by-email-no-screen" ] ; then
    shift
    backup_and_send_email $*

elif [ "$1" = "run-post-backups" ] ; then
    shift
    run_post_backups $*
elif [ \( "$1" = "help" \) -o \( "$1" = "-h" \) -o \( "$1" = "--help" \) ] ; then
    help
fi
