#!/bin/sh

if test "$1" = "clean"; then
	set -x
	rm -f xpizarra;
else
	set -x
	cc pizarra.c utils.c main.c -o xpizarra -lxcb -lxcb-shm -lxcb-image -lxcb-keysyms -lxcb-cursor -lm
fi
