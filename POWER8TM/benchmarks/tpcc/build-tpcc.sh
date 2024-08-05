#!/bin/bash

if [ $# -eq 0 ] ; then
    echo " === ERROR At the very least, we need the backend name in the first parameter. === "
    exit 1
fi

backend=$1 # e.g: herwl

htm_retries=10
rot_retries=10
retry_policy=1

# if [ $# -ge 2 ] ; then
#     htm_retries=$2 # e.g.: 5
# fi
# if [ $# -ge 3 ] ; then
#     rot_retries=$3 # e.g.: 2, this can also be retry policy for tle
# fi
# if [ $# -ge 4 ] ; then
#     retry_policy=$4
# fi

# cp ../../backends/extra_MACROS.h lib/
# cp ../../backends/POWER_common.h lib/
# cp ../../backends/extra_threadc.h lib/

cp ../../backends/extra_MACROS.h code/
cp ../../backends/POWER_common.h code/
cp ../../backends/extra_threadc.h code/
cp ../../backends/seq_log.h code/

echo "Copy from ../../backends/$backend"
cp ../../backends/$backend/tm.h code/
cp ../../backends/$backend/thread.c code/
cp ../../backends/$backend/thread.h code/
cp ../../backends/$backend/Makefile .
# cp ../../backends/$backend/Makefile.common .
cp ../../backends/$backend/Makefile.flags .
cp ../../backends/$backend/Defines.common.mk .

# if pstm, make sure you run make in
# ../../backends/spht/deps/tinystm/

if [[ $backend == spht* || $backend == pstm ]] ; then
    rm lib/rdtsc.h
    mkdir lib/
    bash ../../backends/$backend/copy_spht.sh $backend ./lib ./code
fi
cd code;
rm tpcc

if [[ $backend == htm-sgl || $backend == hybrid-norec ]]; then
	make_command="make HTM_RETRIES=-DHTM_RETRIES=$htm_retries RETRY_POLICY=-DRETRY_POLICY=$retry_policy"
else
	make_command="make HTM_RETRIES=-DHTM_RETRIES=$htm_retries ROT_RETRIES=-DROT_RETRIES=$rot_retries  RETRY_POLICY=-DRETRY_POLICY=$retry_policy"
fi
$make_command
