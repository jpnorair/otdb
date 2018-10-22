#! /usr/local/bin/python3

import os
import sys
import signal
import socket
import binascii
import threading
import time
import json

# Necessary for reading I/O from otdb
from subprocess import call, check_output, Popen, PIPE, STDOUT
import select




# Application/demo variables
app_name        = "otdb-client"
otdb_app        = "./bin/darwin/otdb"
otdb_sockpath   = "./testsocket"



def printf(format, *args):
    sys.stdout.write(format % args)




def signal_handler(signal, frame):
    global otdb_outthread
    global otdb_errthread
    global main_thread
    global pipes_wait

    pipes_wait.release()

    otdb_outthread.do_run = False
    otdb_errthread.do_run = False
    main_thread = False
    # sys.exit(0)



# Thread (used twice) for Sniffing Otter STDERR and STDOUT
def reporter(outfile, filename):
    global otdb_pipe
    #global main_thread

    t = threading.currentThread()
    with outfile:
        for line in iter(outfile.readline, b''):
            printf("%s> %s", filename, line.decode('utf8'))
            if getattr(t, "do_run", False):
                break




# ------- Program startup ---------
if __name__ == '__main__':
 
    # Preliminary optional step: Delete the socket file
    call("rm -rf " + str(otdb_sockpath), shell=True)
 
    # This semaphore blocks demo startup until pipes are ready.
    pipes_wait = threading.Semaphore(0)
    main_thread = True
    
    # Startup OTDB
    otdb_pipe = Popen([otdb_app, "--debug", "-S", otdb_sockpath], stdout=PIPE, stdin=PIPE, stderr=PIPE, bufsize=1)

    # Allow sigint (^C) to pass through to children
    signal.signal(signal.SIGINT, signal_handler)
    
    # Open threads (2) that report stdin/out/err from Otter
    otdb_outthread = threading.Thread(target=reporter, args=(otdb_pipe.stdout, "otdb-out"))
    otdb_outthread.start()
    otdb_errthread = threading.Thread(target=reporter, args=(otdb_pipe.stderr, "otdb-err"))
    otdb_errthread.start()
    
    # There is no condition in this demo for holding the pipe reader, except for the 
    # pipe to exist in the first place.  That should be guaranteed within a short time of
    # open.  A better way would be to poll against the pipe file, but we're lazy here.
    time.sleep(1)
    pipes_wait.release()
    
    # -----------------------------------------------------------------------------------
    # Done starting OTDB.  Now start Client socket
    # -----------------------------------------------------------------------------------
    
    # Create a UDS socket
    clisock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    # Connect the socket to the port where the server is listening
    print('connecting to {}'.format(otdb_sockpath))
    try:
        clisock.connect(otdb_sockpath)
    except socket.error as msg:
        print(msg)
        sys.exit(1)

    while (main_thread == True):
        time.sleep(0.5)
        message = bytes(input('client~ '), 'utf-8')
        
        # Send data
        print('sending {!r}'.format(message))
        clisock.sendall(message)

        # Receive reply
        extra_space = 0
        while (extra_space <= 0):
            data = clisock.recv(4096)
            printf("%s\n", data.decode('utf8'))
            extra_space = 4096 - len(data)
            
    print('closing socket')
    clisock.close()
        
        

    # --------------------------------

    # Wait for all threads to close
    otdb_outthread.join()
    otdb_errthread.join()
    
    otdb_pipe.send_signal(signal.SIGINT)
    otdb_pipe.wait()

    sys.exit(0)

