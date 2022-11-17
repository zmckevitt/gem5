#!/bin/bash

SPEC="/home/zamc2229/cpu2017/benchspec/CPU" # path to SPEC benchmarks
GEM5_PATH="/home/zamc2229/gem5" # path to gem5 project
EXPERIMENT="${GEM5_PATH}/configs/example/se.py" # gem5 experiment location
CFG_LABEL="ML_X86" # label defined in the SPEC .cfg of choice
BENCHMARK="625.x264_s" # full benchmark name
BINARY="x264_s_base" # binary file generated from SPEC
INPUT="--pass 1 --stats x264_stats.log --bitrate 1000 --frames 1000 -o BuckBunny_New.264 BuckBunny.264 1280x720" # input to binary

echo "running ${CFG_LABEL}.${BENCHMARK}.baseline..."

CWD=$PWD #save current working directory location

cd $SPEC/$BENCHMARK/run/run_base_refspeed_$CFG_LABEL-m64.0000 # go to benchmark run location

# gem5 simulation commands
$GEM5_PATH/build/X86/gem5.opt \
--outdir=$GEM5_PATH/evaluation/run/$BENCHMARK.$CFG_LABEL \
$EXPERIMENT \
--mem-size=8GB \
--cpu-type=DerivO3CPU \
--cpu-clock 2GHz --sys-clock 2GHz \
--l1d_size 32kB --l1d_assoc 8 --l1i_size 32kB \
--l1i_assoc 8 --l2_size 2MB --l2_assoc 16 --l2cache --caches \
--cmd=../../exe/$BINARY.$CFG_LABEL-m64 \
--options="${INPUT}" \
--fast-forward=500000000 \
--maxinsts=500000000 

cd $CWD # return to evaluation folder

echo "finished ${CFG_LABEL}.${BENCHMARK}"
