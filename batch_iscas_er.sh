#!/bin/bash

make rebuild

errorBound=(0.001 0.003 0.005 0.008 0.01 0.03 0.05)
rm -rf log/
mkdir log/
rm -rf appntk/
mkdir appntk/

for error in ${errorBound[*]}
do
    logPath=log/${error}/
    appntkPath=appntk/${error}/
    mkdir ${logPath}
    mkdir ${appntkPath}
    for file in data/su/*
    do
        if test -f $file
        then
            name=`basename $file`
            filename="${name%%.*}"
            if [[ "$name" == *.blif ]]
            then
                echo ${filename} ${error}
                (nohup ./main -i ${file} -b ${error} -o ${appntkPath} -f 32 > ${logPath}/${filename}.log &)
            fi
        fi
    done
done
