# lubeman
guard tour p3 device from tomst.com 

required packages, mainly for guardian "pyguard.py" 
python packages
they are used for ipc communication queue between main and guardian process
when the main process halts, guardian kill zombie and start new client

- sudo apt-get install python-pip python-dev build-essential 
- sudo pip install http://semanchuk.com/philip/sysv_ipc/sysv_ipc-0.7.0.tar.gz

1. to compile project just enter make 
2. edit lan_reader_cfg.txt and set ip:port of the server
3. install service, if doesn't exists in /etc/init.d/lubemand.sh
- copy lubemand.sh into /etc/init.d/ 
- enter sudo update-rc.d lubemand.sh defaults

client connect server on selected TCP port (see. lan_reader_cfg.txt)
On server you should see following 

TESTING
```
CHK 
USR rpitest
PES 2017-02-08 15:30:10 31259145
BTN 2017-02-08 15:30:07 F75D86
BTN 2017-02-08 15:30:08 F75D86
END 
CHK 
```

- USR is taken from lan_reader_cfg.txt
- PES is timestamp with PES id number on the END
- BTN is timestamp followed by the dallas ID button (3 bytes) 
- The CHK and END lines must be  acknowledged by the "ACK"+#13#10  string
