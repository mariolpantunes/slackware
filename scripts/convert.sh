#!/bin/bash

for FILE in `ls | grep -v *.sh`; do
    NAME=`echo "$FILE" | cut -d'.' -f1`
    convert $FILE $NAME.jpg
    rm $FILE
done
