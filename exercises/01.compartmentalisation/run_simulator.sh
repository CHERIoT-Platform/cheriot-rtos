#!/bin/sh

if [ ! -p input ] ; then
	mkfifo input
fi

xmake && xmake run < input
