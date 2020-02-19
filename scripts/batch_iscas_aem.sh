#!/bin/bash

make rebuild

errorBound=(0.0000153 0.0000305 0.0000610 0.0001221 0.0002441 0.0004883 0.0009766 0.0019531)
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
    for file in data/su-aem/*
    do
        if test -f $file
        then
            name=`basename $file`
            filename="${name%%.*}"
            if [[ "$name" == *.blif ]]
            then
                echo ${filename} ${error}
                (nohup ./main -i ${file} -b ${error} -o ${appntkPath} -f 32 -m aemr > ${logPath}/${filename}.log &)
            fi
        fi
    done
done
