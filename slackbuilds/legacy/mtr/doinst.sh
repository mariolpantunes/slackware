if [ -e usr/sbin/mtr-packet ]; then
  chown root:root usr/sbin/mtr-packet
  chmod 4755 usr/sbin/mtr-packet
fi
# Older versions of mtr used to have the suid bit on the mtr binary itself
if [ -e usr/sbin/mtr ]; then
  chown root:root usr/sbin/mtr
  chmod 0755 usr/sbin/mtr
fi
