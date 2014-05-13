#!/bin/bash
#
# Install ATNoG certificate into a Linux based OS.
# Based on the work of Andre Rainho.
# 
# Author : Mário Antunes
# EMail  : mario.antunes@av.it.pt
# Date   : 23-08-2012
#
# Version: 3.0

TMP="/tmp"
CERT_LINK="http://atnog.av.it.pt/sites/default/files/ca/"
CERT_NAME="ATNoG_Class_3_Root.crt"
JAVA_KEYTOOL_PASS="changeit"
JAVA_CERT_ALIAS="ATNoG_cert"

START=$(date +%s)
echo "Installing ATNoG certificate..."
wget -N -q --directory-prefix=$TMP $CERT_LINK$CERT_NAME
RESULT=$?

if [ $RESULT -ne "0" ]; then 
    echo -e "\e[00;31mError: ATNoG certificate could no be retrieved!\e[00m"
else
    echo -e "\e[00;32mATNoG certificate retrived\e[00m"
    
    echo -e "Installing certificate in SSL..."
    if sudo -i which update-ca-certificates &> /dev/null; then  
        sudo mkdir -p /usr/local/share/ca-certificates/atnog
        sudo cp $TMP/$CERT_NAME /usr/local/share/ca-certificates/atnog
        sudo -i update-ca-certificates &> /dev/null
        echo -e "\e[00;32mInstallion in SSL complete.\e[00m"
    else
        echo -e "\e[00;31mError: update-ca-certificates command not found!\e[00m"
        echo -e "\e[00;31mError: installion in SSL aborted.\e[00m"
    fi

    echo "Installing certificate in Java Keystore..."
    if [ -e $JAVA_HOME/jre/lib/security/cacerts ]; then
        sudo keytool -delete -alias $JAVA_CERT_ALIAS -keystore $JAVA_HOME/jre/lib/security/cacerts -storepass $JAVA_KEYTOOL_PASS &> /dev/null     
        sudo keytool -importcert -alias $JAVA_CERT_ALIAS -keystore $JAVA_HOME/jre/lib/security/cacerts \
            -storepass $JAVA_KEYTOOL_PASS -noprompt -trustcacerts -file $TMP/$CERT_NAME &> /dev/null
        echo -e "\e[00;32mInstallion in Java Keystore complete.\e[00m"
    else
        echo -e "\e[00;31mError: JAVA_HOME not found!\e[00m"
        echo -e "\e[00;31mError: Installion in Java Keystore aborted.\e[00m"
    fi

    echo "Installing certificate in Curl CA Bundle..."
    if sudo which curl-config > /dev/null; then
        sudo cat `curl-config --ca` $TMP/$CERT_NAME > /tmp/ca-bundle.crt
        sudo cp `curl-config --ca` `curl-config --ca`.backup
        sudo cp $TMP/ca-bundle.crt `curl-config --ca`
        sudo chmod 644 `curl-config --ca`
        sudo chown root:root `curl-config --ca`
        echo -e "\e[00;32mInstallion in Curl CA Bundle complete.\e[00m"
    else
        echo -e "\e[00;31mError: command curl-config not found!\e[00m"
        echo -e "\e[00:31mInstallion in Curl CA Bundle aborted.\e[00m"
    fi

    #echo "Installing certificate in SSL CA Bundle..."
    #if sudo which curl-config > /dev/null; then
    #    sudo cat /etc/ssl/certs/ca-certificates.crt $TMP/$CERT_NAME > /tmp/ca-bundle.crt
    #    sudo cp /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-certificates.crt.backup
    #    sudo cp $TMP/ca-bundle.crt /etc/ssl/certs/ca-certificates.crt
    #    sudo chmod 644 /etc/ssl/certs/ca-certificates.crt
    #    sudo chown root:root /etc/ssl/certs/ca-certificates.crt
    #    echo -e "\e[00;32mInstallion in SSL CA Bundle complete.\e[00m"
    #else
    #    echo -e "\e[00;31mError: command curl-config not found!\e[00m"
    #    echo -e "\e[00:31mInstallion in SSL CA Bundle aborted.\e[00m"
    #fi
fi

#installCertCurlBundle
STOP=$(date +%s)
TIME=$((STOP-START))
echo "Script finished ($TIME seconds)."
