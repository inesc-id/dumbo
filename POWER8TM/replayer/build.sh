#!/bin/bash

make clean

if [[ $1 == "seq_log" ]]
then
  make test BENCH_TYPE=-DSEQ_LOG
fi

if [[ $1 == "naive" ]]
then
  make test BENCH_TYPE=-DNAIVE
fi

if [[ $1 == "log_link" ]]
then
  make test BENCH_TYPE=-DLOG_LINK
fi
