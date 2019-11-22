#!/bin/bash

make rebuild

errorBound=(0.001 0.003 0.005 0.008 0.01 0.03 0.05)
rm -rf log/
mkdir log/
rm -rf appNtk/
mkdir appNtk/

for error in ${errorBound[*]}
do
    logPath=log/${error}/
    appNtkPath=appNtk/${error}/
    mkdir ${logPath}
    mkdir ${appNtkPath}
    for file in data/su/*
    do
        if test -f $file
        then
            name=`basename $file`
            filename="${name%%.*}"
            if [[ "$name" == *.blif ]]
            then
                echo ${filename} ${error}
                (nohup ./main -i ${file} -b ${error} -o ${appNtkPath} > ${logPath}/${filename}.log &)
            fi
        fi
    done
done
