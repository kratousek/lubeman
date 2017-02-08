#!/usr/bin/python

# Python modules
import time
import sys
import threading
import os
import signal
from subprocess import check_output, CalledProcessError


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
    os.system("sudo ./simple &");
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
while True:
  try:
    if IsMessage == 1:
      #print "x>>>", MessageQueue[0],time.time()-startTime
      IsMessage = 0
      MessageQueue=0
      startTime=time.time()

    if (time.time()-startTime)>60 :
      print "Restart process"
      restartProcess("simple")
      startTime=time.time()
     
  except:
    print "Error"
   
mq.remove()
