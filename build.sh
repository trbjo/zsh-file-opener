#!/usr/bin/env zsh
gcc $(pkg-config --cflags glib-2.0 gio-2.0) -march=native -flto -Ofast -mtune=native -o open main.c $(pkg-config --libs glib-2.0 gio-2.0)
