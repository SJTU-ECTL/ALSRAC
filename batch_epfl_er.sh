#!/bin/bash

make rebuild

rm -rf log/
mkdir log

rm -rf appntk/
mkdir appntk

for file in data/epfl/size/random/*
do
    if test -f $file
    then
        name=`basename $file`
        filename="${name%%.*}"
        if [[ "$name" == *.blif ]]
        then
            echo ${filename}
            (nohup ./main -i ${file} -b 0.01 -t 1 -o appntk/ > log/${filename}.log &)
        fi
    fi
done
