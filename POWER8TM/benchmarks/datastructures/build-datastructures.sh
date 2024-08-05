#!/bin/bash

FOLDERS="hashmap redblacktree"

if [ $# -eq 0 ] ; then
    echo " === ERROR At the very least, we need the backend name in the first parameter. === "
    exit 1
fi

backend=$1  # e.g.: herwl

# if pstm, make sure you run make in
# ../../backends/spht/deps/tinystm/

htm_retries=10
rot_retries=10
batch_ratio=1

# if [ $# -eq 4 ] ; then
#     htm_retries=$2 # e.g.: 5
#     rot_retries=$3 # e.g.: 2, this can also be retry policy for tle
#     batch_ratio=$4  # between 1 and 10
# fi

rm lib/*.o || true

rm Defines.common.mk
rm Makefile
rm Makefile.flags
rm lib/thread.h
rm lib/thread.c
rm lib/tm.h

cp ../../backends/extra_MACROS.h lib/
cp ../../backends/POWER_common.h lib/
cp ../../backends/extra_threadc.h lib/
cp ../../backends/seq_log.h lib/


cp ../../backends/$backend/Defines.common.mk .
cp ../../backends/$backend/Makefile .
cp ../../backends/$backend/Makefile.flags .
cp ../../backends/$backend/thread.h lib/
cp ../../backends/$backend/thread.c lib/
cp ../../backends/$backend/tm.h lib/
cp ../../backends/extra_MACROS.h lib/
cp ../../backends/extra_threadc.h lib/
if [[ $backend == p8tm-psi || $backend == p8tm-psi-v2 ]] ; then
    rm lib/rdtsc.h
    cp ../../backends/p8tm-psi/rdtsc.h lib/
fi

if [[ $backend == spht* || $backend == pstm ]] ; then
    rm lib/rdtsc.h
    bash ../../backends/$backend/copy_spht.sh $backend ./lib
fi

for F in $FOLDERS
do
    cd $F
    rm *.o || true
    rm $F
	if [[ $backend == htm-sgl || $backend == hybrid-norec ]] ; then
		make_command="make -f Makefile HTM_RETRIES=-DHTM_RETRIES=$htm_retries RETRY_POLICY=-DRETRY_POLICY=0 BATCH_RATIO=-DBATCH_RATIO=$batch_ratio"
	else
        	make_command="make -f Makefile HTM_RETRIES=-DHTM_RETRIES=$htm_retries ROT_RETRIES=-DROT_RETRIES=$rot_retries BATCH_RATIO=-DBATCH_RATIO=$batch_ratio"
	fi
    if [[ $backend == htm-sgl-sr || $backend == hybrid-norec ]] ; then
		make_command="make -f Makefile HTM_RETRIES=-DHTM_RETRIES=$htm_retries RETRY_POLICY=-DRETRY_POLICY=0 BATCH_RATIO=-DBATCH_RATIO=$batch_ratio"
	fi
    echo "$make_command"
    $make_command
    rc=$?
    if [[ $rc != 0 ]] ; then
        echo ""
        echo "=================================== ERROR BUILDING $F - $name ===================================="
        echo ""
        exit 1
    fi
    cd ..
done

