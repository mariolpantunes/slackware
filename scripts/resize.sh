#!/bin/bash

for FILE in *.png
do
    FILESIZE=$(expr $(stat -c%s "$FILE") / 1024)
    echo -e "Size of $FILE = $FILESIZE kilobytes."
    if [[ $FILESIZE -gt 256 ]]; then
        echo -e "Resize  $FILE by 75%"
        mogrify -resize 75% "$FILE"
    fi
done
