#!/bin/bash

  echo "**** start script ****"
  # open config file
  ret="$(cat lan_reader_cfg.txt | grep host)"
  serverip="${ret:6:20}"
  ret="$(cat lan_reader_cfg.txt | grep port)"
  serverport="${ret:6:20}"
  echo "serverip" $serverip "serverport" $serverport

while :
do
  #exec 3<>/dev/tcp/10.0.0.237/5001
  exec 3<>/dev/tcp/$serverip/$serverport
  
  # // funkcni jen v rpi1 ip= "$(ip addr | grep 'state UP' -A2 | tail -n1 | awk '{print $2}' | cut -f1  -d'/')" 
  ip="$(ip addr | grep 'eth0' -A2 | grep 'brd' | grep 'inet' | awk '{print $2}'   | cut -f1  -d'/')"
  mac="$(ip addr | grep 'eth0' -A2 | tail -n4 | grep 'link/ether' | awk '{print $2}')"
  # GDA ... Get Dpoint active ?
  stimulus="GDA ${ip} ${mac}\r\n"
  #echo ip=$ip mac=$mac $stimulus
  echo -e $stimulus >&3

  # returned answer of GDA
  read RET <&3 
  #echo $ip $RET 
  
  # decompose $RET
  RT="${RET:0:3}"
  #echo $ip $RT
  if [ "$RT" = "NCK" ]; then
    # ok proces is zombie
    pid="$(ps -A -o pid,cmd|grep simple | grep -v grep |head -n 1 | awk '{print $1}')"  
    if [ "$pid" = "" ]; then
      # pid not found, i don't know what to kill
      echo trying restart process
      sudo ./simple &
    else
      echo $pid
      sudo kill -9 $pid &
    fi
  fi

  exec 3<&-
  exec 3>&-
  sleep 10
done
echo "hello"
