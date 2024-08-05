#!/bin/bash

SPINS_EXEC=1
EXPERIMENT_TIME=10000000 # 10s

make clean ; make ### TODO: optimization breaks

mkdir data

for FLUSH_LAT in 100 10000
do

### Physical Clock
rm /tmp/data.tsv
for i in $(seq 12)
do
  ./test_spins FLUSH_LAT=$FLUSH_LAT SPINS_EXEC=$SPINS_EXEC EXPERIMENT_TIME=$EXPERIMENT_TIME FORCE_LEARN=1 \
    spinInCycles=1 NB_THREAD=$i | tail -n 1 >> /tmp/data.tsv
done
mv /tmp/data.tsv data/phys_$FLUSH_LAT.tsv

rm /tmp/data.tsv
for i in $(seq 12)
do
  ./test_spins FLUSH_LAT=$FLUSH_LAT SPINS_EXEC=$SPINS_EXEC EXPERIMENT_TIME=$EXPERIMENT_TIME FORCE_LEARN=1 \
    spinInCycles=1 useLogicalClocks=1 NB_THREAD=$i | tail -n 1 >> /tmp/data.tsv
done
mv /tmp/data.tsv data/logi_$FLUSH_LAT.tsv

rm /tmp/data.tsv
for i in $(seq 12)
do
  ./test_spins FLUSH_LAT=$FLUSH_LAT SPINS_EXEC=$SPINS_EXEC EXPERIMENT_TIME=$EXPERIMENT_TIME FORCE_LEARN=1 \
    spinInCycles=1 useEpochCommit1=1 NB_THREAD=$i | tail -n 1 >> /tmp/data.tsv
done
mv /tmp/data.tsv data/epo1_$FLUSH_LAT.tsv

rm /tmp/data.tsv
for i in $(seq 12)
do
  ./test_spins FLUSH_LAT=$FLUSH_LAT SPINS_EXEC=$SPINS_EXEC EXPERIMENT_TIME=$EXPERIMENT_TIME FORCE_LEARN=1 \
    spinInCycles=1 useEpochCommit2=1 NB_THREAD=$i | tail -n 1 >> /tmp/data.tsv
done
mv /tmp/data.tsv data/epo2_$FLUSH_LAT.tsv

done
