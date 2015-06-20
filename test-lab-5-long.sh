#!/bin/bash
FILE=$1
TIMES=$2
STRING=$3

COUNTER=0

while [ $COUNTER -ne $TIMES ]
do
	let COUNTER=COUNTER+1
	echo $STRING > $FILE # 2> /dev/null 
	rm $FILE >/dev/null 2>&1
done

