# bela-workstation for Pd

The instruments and effects of [bela-workstation](https://github.com/maltepeter2000/bela-workstation)
ready to use: one folder for Pd on the Mac (`.pd_darwin`, universal) and on the Bela Gem
(`.pd_linux`, aarch64). Made from main by `make dist`; do not edit here.

## Install

Mac:

    cd ~/Documents/Pd/externals
    git clone -b pd --single-branch https://github.com/maltepeter2000/bela-workstation.git bela-workstation

then add `~/Documents/Pd/externals/bela-workstation` once under Pd's Preferences -> Path.

Bela Gem (with network; else clone on the Mac and copy the folder, see `bela-sync.sh`):

    cd /root/Bela/projects/pd-externals
    git clone -b pd --single-branch https://github.com/maltepeter2000/bela-workstation.git bela-workstation

and in the project's `_main.pd`: `[declare -path ../pd-externals/bela-workstation]`.

## Update

    git pull

Your user banks (`*user-bank.*`), the ROMs (`d50_pcm.bin`, `tx81z-v1.6.bin`) and `*.nvram`
go into this folder too; they are not in git, so `git pull` never touches them.

## Contents

| Object | | | Help |
|---|---|---|---|
| `cz1~` | Mac | Bela Gem | `cz1~-help.pd` |
| `cz4~` | Mac | Bela Gem | `cz4~-help.pd` |
| `d50~` | Mac | Bela Gem | `d50~-help.pd` |
| `juno~` | Mac | Bela Gem | `juno~-help.pd` |
| `pdoly8~` | Mac | Bela Gem | `pdoly8~-help.pd` |
| `tx81zn~` | Mac | Bela Gem | `tx81zn~-help.pd` |
| `tx81z~` | Mac | Bela Gem | `tx81z~-help.pd` |
| `vrs23~` (abstraction) | Mac | Bela Gem | `vrs23~-help.pd` |
