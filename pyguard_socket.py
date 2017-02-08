#!/usr/bin/python

import time
import sys
import datetime
import socket               # Import socket module
import uuid                 # mac adress
import os                   # communication with the system 
import signal               # killing process
from subprocess import check_output, CalledProcessError


#### Format the line    ####
class Printer():
    """Print things to stdout on one line dynamically"""
    def __init__(self,data):
        sys.stdout.write("\r\x1b[K"+data.__str__())
        sys.stdout.flush()

#### Subroutine helpers ####
def getNetworkIp(IpAdress):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect((IpAdress, 0))
    return s.getsockname()[0]

# trying find the process against its name, kill it and restart again
def restartProcess(process):
    try:
        pidlist = map(int, check_output(["pidof", process]).split())
    except  CalledProcessError:
        pidlist = []
    #print ' list of PIDs = ' + ', '.join(str(e) for e in pidlist)
    for e in pidlist:
      print e;
      os.kill(e,signal.SIGKILL)   
    os.system("sudo ./simple &");
    time.sleep(20)  # give the "./simple" process time to connect to the server


def getVal(ParName):
   with open("lan_reader_cfg.txt") as f:
     for line in f:
        if ParName in line:
	   return(line.split("=",1)[1]);
           
 
###### Main part #####
# Setup serverIP and its port
ServerIp=getVal("_host")
ServerPort=int(getVal("_port"))

# Get local ip adress and respective MAC adress
mac= ':'.join(['{:02x}'.format((uuid.getnode() >> i) & 0xff) for i in range(0,8*6,8)][::-1])
mylocalip = getNetworkIp(ServerIp)
print "Start script on ",datetime.datetime.now()
print "mac=",mac,"mylocip=",mylocalip,"SrvIp=",ServerIp,":",ServerPort

##### Send request and get the answer
i=0
while True:
  try :
    s = socket.socket()         # Create a socket object
    s.settimeout(10)            # socket timeout/10 seconds trying to connecting the server
    s.connect((ServerIp, ServerPort))
    message = "GDA %s %s\r\n" %(mylocalip, mac)  #stimulus line send to the server
    s.sendall(message)
    reply=s.recv(32)
    s.close
    if (reply[0:3]=="NCK"):
      print datetime.datetime.now()," NCK"
      restartProcess("simple")
      i = 0
    else:
      s = "ACK ",i
      #Printer(s)
      i=i+1
    time.sleep(10)            

  except socket.error:
    #Send failed
    print 'Server doesnt respond'
    #sys.exit()
