#!/bin/sh

#export PATH=/bin:/usr/bin:/sbin:/usr/sbin:/jffs/sbin:/jffs/bin:/jffs/usr/sbin:/jffs/usr/bin:/mmc/sbin:/mmc/bin:/mmc/usr/sbin:/mmc/usr/bin:/opt/sbin:/opt/bin:/opt/usr/sbin:/opt/usr/bin

SERIES_FILE="series.txt"
WATCHED_FOLDER="/tmp/"
RSS="http://www.ezrss.it/search/index.php?mode=rss&"
DATE=`date -d "-1 day" '+%Y-%m-%d'`
QUALITY="HDTV"

if [[ -s $SERIES_FILE && -d $WATCHED_FOLDER ]]; then
    #echo "Reading File"
    while read line
    do
        NAME="`echo $line | sed 's/[ \t]/+/g'`"
        echo -e "NAME: $NAME\n"
        URL=$RSS"show_name="$NAME"&date="$DATE"&quality="$QUALITY
        echo -e "URL: $URL\n"
        PAGE="`wget -qO- $URL`"
        echo -e "PAGE:\n$PAGE\n"
        TORRENTS="`echo $PAGE | grep -o -e \<magnetURI\>[^" "]*\<\/magnetURI\> | sed 's/<magnetURI><\!\[CDATA\[//g' | sed 's/\]\]><\/magnetURI>//g'`"
        if [[ -n $TORRENTS ]]; then
            echo -e "TORRENTS:\n$TORRENTS\n"
            echo $TORRENTS | xargs wget -q -N -P $WATCHED_FOLDER
        fi
    done <$SERIES_FILE
else
    #echo "No series file or watched directory"
    exit 1
fi
