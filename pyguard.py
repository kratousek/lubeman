#!/usr/bin/python
import time
import sys
import threading
import os
import signal
from subprocess import check_output, CalledProcessError
import socket
import uuid
import datetime

# 3rd party modules
import sysv_ipc


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
    ret = os.system("sudo ./simple &")
    print "restarted ", ret
    
    time.sleep(20)  # give the "./simple" process time to connect to the server

IsMessage=0
MessageQueue=""

# message queue is read in this thread
class PrimeNumber(threading.Thread):
   lock = threading.Lock()
    
   def __init__(self, number): 
        threading.Thread.__init__(self) 
        self.Number = number
 
   def run(self): 
        global IsMessage
        global MessageQueue
        while True:
          try:
            s = mq.receive()
            PrimeNumber.lock.acquire()
            MessageQueue = s
            IsMessage=1
            PrimeNumber.lock.release()
            #print ">>>", MessageQueue, IsMessage
          except:   
            print "Error" 


#### Subroutine helpers ####
def isServerOK():
  ServerIp=getVal("_host")
  ServerPort=int(getVal("_port"))
  mac= ':'.join(['{:02x}'.format((uuid.getnode() >> i) & 0xff) for i in range(0,8*6,8)][::-1])
  mylocalip = getNetworkIp(ServerIp)
  #print "Start script on ",datetime.datetime.now()
  #print "mac=",mac,"mylocip=",mylocalip,"SrvIp=",ServerIp,":",ServerPort
  try :
    s = socket.socket()         # Create a socket object
    s.settimeout(10)            # socket timeout/10 seconds trying to connecting the server
    s.connect((ServerIp, ServerPort))
    message = "GDA %s %s\r\n" %(mylocalip, mac)  #stimulus line send to the server
    s.sendall(message)
    reply=s.recv(32)
    s.close
    if (reply[0:3]=="ACK"):
      return(1)
    else:
      return(0)
    time.sleep(10)            

  except socket.error:
    #Send failed
    print 'Server doesnt respond'
    return(-1);
    #sys.exit()
  

def getNetworkIp(IpAdress):
   s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
   s.connect((IpAdress, 0))
   return s.getsockname()[0]

def getVal(ParName):
   with open("lan_reader_cfg.txt") as f:
     for line in f:
       if ParName in line:
         return(line.split("=",1)[1]);


################################### MAIN ##########################
#clean up old key
try:
  mq = sysv_ipc.MessageQueue(42)
  mq.remove()
except:
  print "Cannot clean messageQueue"

# Create the message queue.
#mq = sysv_ipc.MessageQueue(params["KEY"], sysv_ipc.IPC_CREX)
mq = sysv_ipc.MessageQueue(42, sysv_ipc.IPC_CREX)
thread = PrimeNumber(1);
thread.start()

restartProcess("simple")

mq.send("wait")
mq.send("ahoj")
startTime=time.time()
sokTime  =time.time()
sok_restart=False
while True:
  try:
    # got message from simple process
    if IsMessage == 1:
      print "x>>>", MessageQueue[0],time.time()-startTime
      if MessageQueue[0] >= "SOK":
          print "SOK ..."
          sokTime = time.time()

      if (time.time()-sokTime)>120:
          sokTime = time.time()
          print "SOK not OK checking server connection"
          si = isServerOK()
          if si == 1:
              print "Server OK -> Restart SOK"
              sok_restart=True
          elif si == 0:
              print "Server not responding"
          else:
              print "Big socket error"

      IsMessage = 0
      MessageQueue=0
      startTime=time.time()

    if ((time.time()-startTime)>60) or (sok_restart==True) :
      print "Restart process"
      restartProcess("simple")
      startTime=time.time()
      sok_restart=False
     
  except:
    print "Error"
   
mq.remove()
