#!/bin/bash
if [ $# -eq 0 ]
then
	echo "usage: $0 <command>"
	exit 1
fi

if [ $# -gt 1 ]
then
	echo "usage: $0 <command>"
	exit 1
fi

printf $1 > /dev/ttyS0
cat /dev/ttyS0
