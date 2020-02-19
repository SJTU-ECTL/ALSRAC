#!/bin/bash

make rebuild

rm -rf log/
mkdir log

rm -rf appntk/
mkdir appntk

for file in data/epfl/size/arith/*
do
    if test -f $file
    then
        name=`basename $file`
        filename="${name%%.*}"
        if [[ "$name" == *.blif ]]
        then
            echo ${filename}
            # (nohup ./main -i ${file} -m 0.048828125 > log/${filename}.log &)
            (nohup ./main -i ${file} -m raem -b 0.001953125 -t 1 > log/${filename}.log &)
        fi
    fi
done
