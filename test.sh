#!/bin/bash

make rebuild

rm -rf log/
if [ ! -d log ]
then
    mkdir log
fi

rm -rf appntk/
if [ ! -d appntk ]
then
    mkdir appntk
fi

filename='data/sop/c880.blif'
pis=(30 40 50 60)
nframes=(8192 16384 32768 65536 131072)

for pi in ${pis[@]}
do
    for nframe in ${nframes[@]}
    do
        echo ${pi} ${nframe}
        (nohup ./main -i data/sop/c880.blif -m ${pi} -n ${nframe} > log/c880-${nframe}-${pi}.log &)
    done
done
