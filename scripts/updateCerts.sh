#!/bin/bash

cd /root;
rm -f cacert.pem*;
wget http://curl.haxx.se/ca/cacert.pem;
chattr -i /usr/share/curl/ca-bundle.crt;
chattr -i /etc/ssl/certs/ca-certificates.crt;
rm -f /usr/share/curl/ca-bundle.crt;
rm -f /etc/ssl/certs/ca-certificates.crt;
cp -f cacert.pem /usr/share/curl/ca-bundle.crt;
cp -f cacert.pem /etc/ssl/certs/ca-certificates.crt;
chmod 644 /usr/share/curl/curl-ca-bundle.crt;
chmod 644 /etc/ssl/certs/ca-certificates.crt;
chown root:root /usr/share/curl/ca-bundle.crt;
chown root:root /etc/ssl/certs/ca-certificates.crt;
chmod 755 /etc/ssl/;
chmod 755 /usr/share/curl/;
