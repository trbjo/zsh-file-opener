#!/usr/bin/env zsh

dirname="${${0:A:h}##*/}"
output=${1:-$dirname}
input=${2:-main.c}

gcc $(pkg-config --cflags glib-2.0) \
-march=native \
-flto \
-Ofast \
-mtune=native \
-o $output $input $(pkg-config --libs glib-2.0)

bindir="$HOME/.local/bin"
[[ -d $bindir ]] || mkdir -p $bindir
destination="${bindir}/${output}"
ln -fs "${${${(%):-%N}:A}%/*}/$output" "$destination"
