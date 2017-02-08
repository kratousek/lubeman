#! /bin/sh
# /etc/init.d/lubemand

### BEGIN INIT INFO
# Provides:             lubeman
# Required-Start:       $all $local_fs $network $syslog $portmap $time
# Required-Stop:        
# Default-Start:        2 3 4 5
# Default-Stop:         0 1 6
# Short-Description:    TOMST PES READER
### END INIT INFO

recursiveKill() { # Recursively kill a process and all subprocesses
    CPIDS=$(pgrep -P $1);
    for PID in $CPIDS
    do
        recursiveKill $PID
    done
    sleep 3 && kill -9 $1 2>/dev/null & # hard kill after 3 seconds
    kill $1 2>/dev/null # try soft kill first
}


checkKill() {
   PID=`ps -A -o pid,cmd|grep $1 | grep -v grep |head -n 1 | awk '{print $1}'`
   #echo par=$1 pid=$PID
   if [ -z "$PID" ]
   then
     echo  $1 " " $pid "process stopped"
   else
     echo "Kill the zombie !!!" $PID
     kill $PID
   fi
}

# Some things that run always
NAME=/home/krata/lubeman/pyguard.py
touch $NAME

# Carry out specific functions when asked to by the system
# Carry out specific functions when asked to by the system
case "$1" in
  start)
    rmmod ftdi_sio
    rmmod ftdi_sio
    cd /home/krata/lubeman
    logger -p user.error -t TomstLanPesReader "1st starting script lubemand"

    echo "Starting $NAME ..."
    if [ -f "$WORKDIR/$NAME.pid" ]
        then
            echo "Already running according to $WORKDIR/$NAME.pid"
            #exit 1
    fi
    /home/krata/lubeman/simple "T"
    rc=$?
    cc=1
    #echo "Jedna" $rc $CC
    while [ $rc -eq 1 ]
    do
      sleep 2s
      cc=`expr $cc + 1`
      logger -p user.error -t TomstLanPesReader "Next starting script lubemand "$cc
      $NAME > /home/krata/lubeman/out.txt 2>&1 &
      #/home/pi/krata/lubeman/simple > /home/pi/krata/lubeman/out.txt 2>&1 &
      rc=$?
    done

    PID=$!
    echo $PID > "$WORKDIR/$NAME.pid"
    echo "Started with pid $PID - Logging to $WORKDIR/$NAME.log" && exit 0

    ;;
  stop)
    echo "Stopping script lubemand"
    echo "Stopping $NAME ..."
    # does the pid exists in the folder ? 
    if [ ! -f "$WORKDIR/$NAME.pid" ]
    then
            # NO, there is no such file
            echo "Already stopped!"
            checkKill "pyguard.py"
            checkKill "simple"
            checkKill "guard.sh"
            exit 1
    fi
    PID=`cat "$WORKDIR/$NAME.pid"`
    recursiveKill $PID
    rm -f "$WORKDIR/$NAME.pid"
    checkKill "simple"
    checkKill "guard.sh"
    checkKill "pyguard.py"
    echo "stopped $NAME" && exit 0

    ;;

  *)
    echo "Usage: /etc/init.d/lubemand.sh {start|stop}"
    exit 1
    ;;

esac
exit 0
