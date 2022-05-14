#!/bin/sh
# $1: 1/0 for cleaning the pass
# $2: 1/0 for remaking the pass
# $3: 1/0 for cleaning app
# $4: 1/0 for cleaning libpacarana
set -e

if [ "$1" -eq 1 ]; then
make clean
fi

if [ "$2" -eq 1 ]; then
make all TEST_ISR=1 TEST_MOD=1 TEST_ATOMIC=1
fi

if [ "$4" -eq 1 ]; then
  rm -f ../ext/libp/bld/gcc/*
  ( cd ../ ; make bld/clang/dep )
fi

if [ "$3" -eq 1 ]; then
  rm -f ../bld/clang/*
  #( cd ../../ ; make bld/clang/all USE_SENSORS=1 TEMP=1 RATE=0x80 FUNC=fft INPUTS="b2,b3,b1,b4" HPVLP=0 REENABLE=0 CHECK_FUNC=fft_check )
  ( cd ../ ; make bld/clang/clean; make bld/clang/all RUN_BUGS=1 USE_SENSORS=; )
fi

opt-7  -load ./tasks.so -tasks  ../bld/clang/harness.a.bc -o harness.out > results.txt
