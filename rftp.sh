#!/bin/sh
# check FTP server is running
ENABLED='0'
HOST='90.177.112.119'
# HOST='192.168.1.50'
PORT='4214'
# PORT='2021'
# USER='FTP_Public'
USER='FTP_User'
# PASS='ftp2tomst4public'
PASS='ftomst7p'
CDIR='lanwr_dev'
FILE='lan_reader_cfg.txt'
if [ $ENABLED -eq 0 ]
then
  logger -p user.error -t TomstLanPesReader "Configuration download from FTP server is disabled."
  exit 0
fi
nc -w 5 -z $HOST $PORT
if [ $? -ne 0 ]
then
  logger -p user.error -t TomstLanPesReader "No FTP server."
  exit 1
fi
ncftpls -u $USER -p $PASS -P $PORT 'ftp://'$HOST'/'$CDIR | grep -x $FILE >/dev/null
if [ $? -ne 0 ]
then
  logger -p user.error -t TomstLanPesReader "No config file on server."
  exit 1
fi
ftp -n $HOST $PORT <<END_SCRIPT
quote USER $USER
quote PASS $PASS
quote CWD $CDIR
bin
get $FILE
quit
END_SCRIPT
logger -p user.error -t TomstLanPesReader "Successfull config file download."
exit 0
