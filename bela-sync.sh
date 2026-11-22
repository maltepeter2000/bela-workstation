#!/bin/sh
# Copies this folder from the Mac to the Bela Gem (when the Bela has no network for git pull).
# usage: sh bela-sync.sh [host]        (default: root@bela-multi.local)
# Your files on the Bela (user banks, ROMs, nvram) stay: rsync only adds and updates.
HOST=${1:-root@bela-multi.local}
DIR=/root/Bela/projects/pd-externals/bela-workstation
cd "$(dirname "$0")" || exit 1
ssh "$HOST" "mkdir -p $DIR" &&
rsync -av --exclude .git --exclude '*.pd_darwin' ./ "$HOST:$DIR/"
