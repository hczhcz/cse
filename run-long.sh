#!/bin/bash
TEST_FILE5_1=$1
TEST_FILE5_2=$2
TEST_FILE5_3=$3
SGOOD2=$4
SBAD2=$5
TIMES=15
nohup ./test-lab-5-long.sh $TEST_FILE5_1 $TIMES $SGOOD2  >/dev/null 2>&1 &
nohup ./test-lab-5-long.sh $TEST_FILE5_2 $TIMES $SBAD2  >/dev/null 2>&1 &
nohup ./test-lab-5-long.sh $TEST_FILE5_3 $TIMES $SGOOD2  >/dev/null 2>&1 &

