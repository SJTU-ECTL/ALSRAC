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

for file in data/sop/*
do
    if test -f $file
    then
        name=`basename $file`
		filename="${name%%.*}"
       	if [[ "$name" == *.blif ]]
       	then
            echo ${filename}
            (nohup ./main -i ${file} -n 1024 -m 10 > log/${filename}.log &)
		fi
    fi
done


