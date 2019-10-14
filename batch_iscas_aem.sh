#!/bin/bash

make rebuild

rm -rf log/
mkdir log

rm -rf appntk/
mkdir appntk

for file in data/su-aem/*
do
    if test -f $file
    then
        name=`basename $file`
        filename="${name%%.*}"
        if [[ "$name" == *.blif ]]
        then
            echo ${filename}
            (nohup ./main -i ${file} -m 0.003051758 > log/${filename}.log &)
        fi
    fi
done
