#!/usr/bin/env bash

ulimit -c unlimited

LOSSY=$1
NUM_LS=$1

if [ -z $NUM_LS ]; then
    NUM_LS=0
fi

BASE_PORT1=$RANDOM
BASE_PORT1=$[BASE_PORT1+2000]
EXTENT_PORT1=$BASE_PORT1
YFS1_PORT=$[BASE_PORT1+2]
LOCK_PORT=$[BASE_PORT1+6]

YFSDIR1=$PWD/yfs1

SHELLERR='[ERROR! If you see this error, your shell or some of your basic tools may be incompatable with this script.]'

#=========================== preparation ============================
echo -n Preparing
if [ "$LOSSY" ]; then
    export RPC_LOSSY=$LOSSY
fi

if [ $NUM_LS -gt 1 ]; then
    x=0
    rm config
    while [ $x -lt $NUM_LS ]; do
      port=$[LOCK_PORT+2*x]
      x=$[x+1]
      echo $port >> config
    done
    x=0
    while [ $x -lt $NUM_LS ]; do
      port=$[LOCK_PORT+2*x]
      x=$[x+1]
#      echo "starting ./lock_server $LOCK_PORT $port > lock_server$x.log 2>&1 &"
      ./lock_server $LOCK_PORT $port > lock_server$x.log 2>&1 &
      sleep 1
    done
else
#    echo "starting ./lock_server $LOCK_PORT > lock_server.log 2>&1 &"
    ./lock_server $LOCK_PORT > lock_server.log 2>&1 &
    sleep 1
fi

echo -n ...

unset RPC_LOSSY

#echo "starting ./extent_server $EXTENT_PORT1 > extent_server1.log 2>&1 &"
./extent_server $EXTENT_PORT1 > extent_server1.log 2>&1 &
sleep 1

echo -n ...

# first start
rm -rf $YFSDIR1
mkdir $YFSDIR1 || exit 1
sleep 1

echo -n ...

#echo "starting ./yfs_client $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT > yfs_client1.log 2>&1 &"
#./yfs_client $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT > yfs_client1.log 2>&1 &
rm yfs_client1.log > /dev/null 2>&1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
#./yfs_client $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT >> yfs_client1.log 2>&1 &
sleep 1 

echo Done

#make sure FUSE is mounted where we expect
pwd=`pwd -P`
if [ `mount | grep "$pwd/yfs1" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount YFS properly at ./yfs1"
    exit -1
fi
#=====================================================================

TOTAL=90
SCORE=0

SBAD='Lei is a bad TA #-_-'$RANDOM
SGOOD='Lei is a good TA ^u^'$RANDOM

#=========================== Test 1 ======================== ==========

echo -n [1] Crash and restart ..............................
# [Before crash]

# [Crash]
killall yfs_client
sleep 1 

# [Recover]
fusermount -u $YFSDIR1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
sleep 1

# [After recovery]

# [Check]
if [ `ps | grep yfs_client | wc -l` -eq 1 ]; then
	echo -n '[OK] '
	let SCORE=SCORE+10
else
	echo -n '[FAIL] '
fi
echo [$SCORE/$TOTAL]

#======================================================================

#=========================== Test 2 ======================== ==========

echo -n [2] Simple recovery - read/write/create ............
$TEST2
# [Before crash]
echo $SGOOD > yfs1/file1
echo $SBAD > yfs1/file2

# [Crash]
killall yfs_client
sleep 1 

# [Recover]
fusermount -u $YFSDIR1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
sleep 1

# [After recovery]

# [Check]
if [[ ( `ps | grep yfs_client | wc -l` -eq 1 ) && ( `cat yfs1/file1` = $SGOOD ) && ( `cat yfs1/file2` = $SBAD ) ]] 
then
	echo -n '[OK] '
	let SCORE=SCORE+15
else
	echo -n '[FAIL] '
fi
echo [$SCORE/$TOTAL]

#======================================================================

#=========================== Test 3 ======================== ==========

echo -n [3] Simple recovery - mkdir/symlink/unlink .........
# [Before crash]
mkdir yfs1/test3/
mkdir yfs1/test3/dir1
echo $SBAD > yfs1/test3/file1
rm yfs1/test3/file1
echo $SBAD > yfs1/test3/dir1/file2
echo $SGOOD > yfs1/test3/file1
echo $SGOOD > yfs1/test3/dir1/file1
tree -a yfs1 > test-lab-5.log

# [Crash]
killall yfs_client
sleep 1 

# [Recover]
fusermount -u $YFSDIR1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
sleep 1

# [After recovery]

# [Check]
if [[ ( `ps | grep yfs_client | wc -l` -eq 1 ) && ( `cat yfs1/test3/file1` = $SGOOD ) && ( `cat yfs1/test3/dir1/file1` = $SGOOD ) && ( `cat yfs1/test3/dir1/file2` = $SBAD ) ]] 
then
	echo -n '[OK] '
	let SCORE=SCORE+15
else
	echo -n '[FAIL] '
fi
echo [$SCORE/$TOTAL]

#======================================================================

#=========================== Test 4 ======================== ==========

echo -n [4] Crash around concurrent big file writing .......
# [Before crash]
TEST_FILE4_0=test-lab-5-file-comp4
TEST_FILE4_1=yfs1/test4/file1$RANDOM
TEST_FILE4_2=yfs1/test4/file2$RANDOM
TEST_FILE4_3=yfs1/test4/file3$RANDOM
TEST_FILE4_4=yfs1/test4/file4$RANDOM
SRCFILE4=test-lab-5-file-rand
mkdir yfs1/test4

I4=0
CT4=50
SC4=0

while [ $I4 -lt $CT4 ]
do
dd if=/dev/urandom of=${SRCFILE4} bs=1k count=400 >/dev/null 2>&1
dd if=${SRCFILE4} of=${TEST_FILE4_0} bs=1k seek=3 count=20 >/dev/null 2>&1
./test-lab-5-blob.sh $TEST_FILE4_2 $SRCFILE4 &
./test-lab-5-blob.sh $TEST_FILE4_1 $SRCFILE4 
./test-lab-5-blob.sh $TEST_FILE4_3 $SRCFILE4 &
./test-lab-5-blob.sh $TEST_FILE4_4 $SRCFILE4 &

# [Crash]
export RPC_LOSSY=100
killall yfs_client
killall dd >/dev/null 2>&1
unset RPC_LOSSY

# [Recover]
fusermount -u $YFSDIR1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
sleep .1
# [After recovery]


# [Check]
if [[ ( `ps | grep yfs_client | wc -l` -eq 1 ) && \
	( -a $TEST_FILE4_1 ) && ( `diff $TEST_FILE4_0 $TEST_FILE4_1 | wc -l ` -eq 0 ) && \
	( !( -a $TEST_FILE4_2 ) || ( `xxd -p $TEST_FILE4_0 | tr -d '\n'` = `xxd -p $TEST_FILE4_2 | tr -d '\n'`*  ) ) && \
	( !( -a $TEST_FILE4_3 ) || ( `xxd -p $TEST_FILE4_0 | tr -d '\n'` = `xxd -p $TEST_FILE4_3 | tr -d '\n'`*  ) ) && \
	( !( -a $TEST_FILE4_4 ) || ( `xxd -p $TEST_FILE4_0 | tr -d '\n'` = `xxd -p $TEST_FILE4_4 | tr -d '\n'`*  ) ) ]]
then
	let SC4=SC4+1
fi
let I4=I4+1
done
	
if [ $SC4 -eq $CT4 ]
then
	echo -n '[OK] '
	let SCORE=SCORE+25
else
	echo -n '[FAIL] '
fi
echo [$SCORE/$TOTAL]

# [Clean Up]
rm ${SRCFILE4}

#======================================================================
sleep 1

#=========================== Test 5 ======================== ==========

echo -n '[5] Long running '
# [Before crash]
echo -n ..
mkdir yfs1/test5
TEST_FILE5_0=yfs1/test5/file0$RANDOM
TEST_FILE5_1=yfs1/test5/file1$RANDOM
TEST_FILE5_2=yfs1/test5/file2$RANDOM
TEST_FILE5_3=yfs1/test5/file3$RANDOM
SGOOD2='LeiIsAGoodTA'$RANDOM
SBAD2='LeiIsABadTA'$RANDOM
TIMES=10

I5=0
CT5=50
SC5=0

echo -n ..

while [ $I5 -lt $CT5 ]
do
./run-long.sh $TEST_FILE5_1 $TEST_FILE5_2 $TEST_FILE5_3 $SGOOD2 $SBAD2 &
./test-lab-5-long.sh $TEST_FILE5_0 $TIMES $SGOOD2  >/dev/null 2>&1 
echo $SGOOD2 > $TEST_FILE5_0

# [Crash]
export RPC_LOSSY=100
killall yfs_client > /dev/null 2>&1 
killall test-lab-5-long.sh > /dev/null 2>&1 
killall echo >/dev/null 2>&1 
killall rm >/dev/null 2>&1 
unset RPC_LOSSY
echo -n .

# [Recover]
fusermount -u $YFSDIR1
./run-client.sh $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT
#./yfs_client $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT >> yfs_client1.log 2>&1 &
sleep .1

# [After recovery]

# [Check]
if [[ ( `ps | grep yfs_client | wc -l` -eq 1 ) &&
	( -a $TEST_FILE5_0 ) && ( `cat $TEST_FILE5_0` = $SGOOD2 ) && \
	( !( -a $TEST_FILE5_1 ) || ( $SGOOD2 = `cat $TEST_FILE5_1`* ) ) && \
	( !( -a $TEST_FILE5_2 ) || ( $SBAD2 = `cat $TEST_FILE5_2`* ) ) && \
	( !( -a $TEST_FILE5_3 ) || ( $SGOOD2 = `cat $TEST_FILE5_3`* ) ) ]]
then
	let SC5=SC5+1
fi
echo -n .
rm $TEST_FILE5_0
let I5=I5+1
done
echo -n ......
echo -n .....
	
if [ $SC5 -eq $CT5 ]
then
	echo -n '[OK] '
	let SCORE=SCORE+25
else
	echo -n '[FAIL] '
fi
echo [$SCORE/$TOTAL]

#======================================================================

if [ $SCORE -eq $TOTAL ]
then
	echo [You\'ve passed all the test! Good Job!]
fi
