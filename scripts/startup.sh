#!/bin/bash
# sleep 3
setxkbmap -layout pt
#xbacklight -set 25
echo 5 | sudo tee /sys/class/backlight/asus_laptop/brightness

host "gaia.av.it.pt"
INSIDE_IT="$?"
if [[ $INSIDE_IT -eq 0 ]]; then
    sudo /sbin/route add -net 10.115.0.0 netmask 255.255.0.0 gw gaia.av.it.pt
    sudo /sbin/route add -net 10.15.1.0 netmask 255.255.255.0 gw gaia.av.it.pt
fi

IN="LVDS"
EXT="HDMI-0"
if (xrandr | grep "$EXT disconnected"); then
    xrandr --output $EXT --off --output $IN --auto
else
    xrandr --output $EXT --pos 0x0 --auto --output $IN --auto --right-of $EXT
fi

#Dirty fix
xfce4-power-manager --restart
