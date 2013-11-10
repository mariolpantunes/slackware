#!/bin/bash

CATEGORIES=('apple' 'banana' 'bottle' 'digital_camera'
'dinosaur' 'electric_iron' 'eyeglasses' 'fax_machine' 'fork'
'frying_pan' 'laptop' 'orange' 'pumpkin' 'red_bell_pepper'
'red_ping_pong_paddle' 'red_plastic_cup' 'remote_control'
'rolling_suitcase' 'saucepan' 'scientific_calculator' 'toy_car' 'ulu'
'upright_vacuum_cleaner' 'white_soccer_ball')

for ITEM in ${CATEGORIES[*]}
do
    for ((I = 0; I <= 20; I++)); do
        OLD=$(printf "%s%02d.png" $ITEM $(($I+1)))
        NEW=$(printf "%s%02d.png" $ITEM $I)
        echo -e "$OLD -> $NEW"
        mv -vf "$OLD" "$NEW"
    done
done
