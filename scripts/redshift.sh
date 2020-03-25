#!/bin/bash

function parse_json()
{
    echo $1 | \
    sed -e 's/[{}]/''/g' | \
    sed -e 's/", "/'\",\"'/g' | \
    sed -e 's/" ,"/'\",\"'/g' | \
    sed -e 's/" , "/'\",\"'/g' | \
    sed -e 's/","/'\"---SEPERATOR---\"'/g' | \
    awk -F=':' -v RS='---SEPERATOR---' "\$1~/\"$2\"/ {print}" | \
    sed -e "s/\"$2\"://" | \
    tr -d "\n\t" | \
    sed -e 's/\\"/"/g' | \
    sed -e 's/\\\\/\\/g' | \
    sed -e 's/^[ \t]*//g' | \
    sed -e 's/^"//'  -e 's/"$//'
}

LOCATION="`wget -qO- http://api.hostip.info/get_json.php?position=true`"
#echo -e $LOCATION
LAT="`parse_json $LOCATION lat`"
LON="`parse_json $LOCATION lng`"
#echo -e $LAT";"$LON
redshift -v -l $LAT:$LON -t 5500:4000
