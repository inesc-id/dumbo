#!/bin/sh

rm lib/*.o || true

rm Defines.common.mk
rm Makefile
rm Makefile.flags
rm lib/thread.h
rm lib/thread.c
rm lib/tm.h

cd bank
rm *.o || true
rm bank
cd ..
