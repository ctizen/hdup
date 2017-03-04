#!/bin/sh

# this script should be used when 'no history = yes' in hdup's configuration
# What it does: it copies the static archives to a save place.
# It takes three arguments: 
#   1. the current archive name
#   2. the directory to which #1 should be copied
#   3. the current scheme

# This script can serve as a basis. Feel free to extend it.
# (c) Miek Gieben, distributed under GPL v2
# It currently does not: check if encryption is used - there not 
#   copying the archive
# It also does not take chunk size into account

ar=$1
to=$2
scheme=$3

if [ $scheme == "restore" -o $scheme == "remote" ]; then
    echo "Wrong scheme"
    exit 1;
fi;

if [ ! -f $ar ]; then
    echo "Cannot find archive"
    exit 1;
fi

if [ -f $to ]; then
     echo "Destination directory is a file" 
     exit 1;
fi

if [ ! -e $to ]; then
    mkdir -p $to
fi

# copy it
cp -f $ar $to
