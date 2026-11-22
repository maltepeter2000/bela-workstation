#!/bin/sh
# Copies this folder from the Mac to the Bela Gem (when the Bela has no network for git pull).
# usage: sh bela-sync.sh [host]        (default: root@bela-multi.local)
# Your files on the Bela (user banks, ROMs, nvram) stay: rsync only adds and updates.
HOST=${1:-root@bela-multi.local}
DIR=/root/Bela/projects/pd-externals/bela-workstation
cd "$(dirname "$0")" || exit 1
ssh "$HOST" "mkdir -p $DIR" || exit 1
if ssh "$HOST" "command -v rsync" >/dev/null 2>&1; then
  rsync -av --exclude .git --exclude '*.pd_darwin' ./ "$HOST:$DIR/"
else          # no rsync on the board: copy what is in git (your files there stay)
  git ls-files | grep -v '\.pd_darwin$' | tar cf - -T - | ssh "$HOST" "tar xf - -C $DIR"
fi
