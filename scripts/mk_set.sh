#!/bin/bash

CATEGORIES=(
'apple'
'banana'
'bell_pepper'
'bottle'
'digital_camera'
'dinosaur'
'electric_iron'
'eyeglasses'
'fax_machine'
'fork'
'frying_pan'
'laptop'
'orange'
'ping_pong_paddle'
'plastic_cup'
'pumpkin'
'remote_control'
'rolling_suitcase'
'saucepan'
'scientific_calculator'
'soccer_ball'
'toy_car'
'ulu'
'upright_vacuum_cleaner')

NIMAGESPERCAT=${NIMAGESPERCAT:-"10"}
NCATEGORIES=${#CATEGORIES[@]}
TOTALIMAGES=$(($NIMAGESPERCAT*$NCATEGORIES))
FOLDER=${FOLDER:-"testSet"}

printf "$TOTALIMAGES $NCATEGORIES $FOLDER\n"
for ITEM in ${CATEGORIES[*]}
do
    for ((I = 0; I < $NIMAGESPERCAT; I++)); do
        printf "%s%02d.jpg %s\n" $ITEM $I $ITEM
    done
done
