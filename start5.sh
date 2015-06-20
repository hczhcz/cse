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
      echo "starting ./lock_server $LOCK_PORT $port > lock_server$x.log 2>&1 &"
      ./lock_server $LOCK_PORT $port > lock_server$x.log 2>&1 &
      sleep 1
    done
else
    echo "starting ./lock_server $LOCK_PORT > lock_server.log 2>&1 &"
    ./lock_server $LOCK_PORT > lock_server.log 2>&1 &
    sleep 1
fi

unset RPC_LOSSY

echo "starting ./extent_server $EXTENT_PORT1 > extent_server1.log 2>&1 &"
./extent_server $EXTENT_PORT1 > extent_server1.log 2>&1 &
sleep 1

# first start
rm -rf $YFSDIR1
mkdir $YFSDIR1 || exit 1
sleep 1
echo "starting ./yfs_client $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT > yfs_client1.log 2>&1 &"
./yfs_client $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT > yfs_client1.log 2>&1 &
sleep 2

#make sure FUSE is mounted where we expect
pwd=`pwd -P`
if [ `mount | grep "$pwd/yfs1" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount YFS properly at ./yfs1"
    exit -1
fi

# [Before crash] add here any file system test program
ls -a yfs1


# kill
killall yfs_client
sleep 2

# recover
fusermount -u $YFSDIR1
echo "restarting ./yfs_client $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT > yfs_client1.log 2>&1 &"
./yfs_client $YFSDIR1 $EXTENT_PORT1 $LOCK_PORT >> yfs_client1.log 2>&1 &
sleep 1

# [After recovery] add here any file system test program
ls -a yfs1

# make sure FUSE is mounted where we expect
#pwd=`pwd -P`
#if [ `mount | grep "$pwd/yfs1" | grep -v grep | wc -l` -ne 1 ]; then
#    sh stop.sh
#    echo "Failed to mount YFS properly at ./yfs1"
#    exit -1
#fi

