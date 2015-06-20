#!/bin/bash
TEST_FILE=$1
SRCFILE=$2

dd if=${SRCFILE} of=${TEST_FILE} bs=1k seek=3 count=20 >/dev/null 2>&1


