#!/usr/bin/env zsh
filename="${${0:A:h}##*/}"
gcc $(pkg-config --cflags glib-2.0) \
-march=native \
-flto \
-Ofast \
-mtune=native \
-o $filename main.c $(pkg-config --libs glib-2.0)

bindir="$HOME/.local/bin"
[[ -d $bindir ]] || mkdir -p $bindir
destination="${bindir}/${filename}"
[[ ! -f "${destination}" ]] || rm "$destination"
ln -s "${${${(%):-%N}:A}%/*}/$filename" "$destination"
