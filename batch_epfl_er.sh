#!/bin/bash

make rebuild

rm -rf lognew/
mkdir lognew

rm -rf appntknew/
mkdir appntknew

for file in data/epfl/size/random/*
do
    if test -f $file
    then
        name=`basename $file`
        filename="${name%%.*}"
        if [[ "$name" == *.blif ]]
        then
            echo ${filename}
            (nohup ./main -i ${file} -r 0.01 -t 1 > lognew/${filename}.log &)
        fi
    fi
done
