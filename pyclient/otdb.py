#! python3

import sys
#import os
import time
import signal
import threading
import argparse
import socket
import json
import struct

#-------------------------------------------------------------------------------------
# PROGRAM DEFAULTS



#-------------------------------------------------------------------------------------
# GENERIC STATIC FUNCTIONS

def printf(fmt, *params):
    sys.stdout.write(fmt % params)



class otdb:
    def __init__(self, sockpath="/opt/otdb/otdb.sock"):
        self.sockpath = sockpath
        self.sockobj = None
        self.sockfile = None
        self.isconnected = False


    def connect(self):
        """
        Just connect to the OTDB socket.
        Returns True or False on Success or Error.
        """
        if not self.isconnected:
            # connect to the socket: socket will stay open.
            try:
                self.sockobj = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.sockobj.connect(self.sockpath)
            except socket.error as exc:
                printf("OTDB: Socket connection error : %s\n", exc)
                return False
            try:
                self.sockfile = self.sockobj.makefile()
            except OSError:
                self.sockobj.close()
                return False

            self.isconnected = True
            return True


    def disconnect(self):
        if self.isconnected:
            try:
                self.sockobj.close()
            except (OSError, AttributeError):
                return False
            self.isconnected = False

        return not self.isconnected


    def _sendcmd(self, cmd=None):
        otdb_resp = None
        if self.isconnected:
            try:
                self.sockfile.sendall(cmd)
                otdb_resp = self.sockfile.readline()
            except socket.error:
                print("OTDB: socket command failure")
                return None

        return otdb_resp


    def newdevice(self, new_uid):
        """
        Create a new device, given a supplied UID, according to the DB template.
        new_uid : a 64bit bytes object or an unsigned integer
        """


    def deldevice(self, del_uid):
        """
        Delete a device from the OTDB, given a supplied UID.
        del_uid : a 64bit bytes object or an unsigned integer
        """


    def setdevice(self, set_uid):
        """
        Set the default UID of the OTDB.
        Don't use this method unless you really know what you're doing.
        It can cause problems if OTDB is handling multiple clients.
        set_uid : a 64bit bytes object or an unsigned integer
        """


    def load(self, dbpath=""):
        """
        Load a new database template (and device list) into the OTDB context.
        Don't use this method unless you really know what you're doing.
        The OTDB context is typically set when the OTDB daemon is started.
        dbpath : path to the DB archive to load.
        """


    def save(self, savepath):
        """
        Save the current OTDB context into a archive at a supplied path.
        savepath : path to save the DB archive
        """


    def delfile(self, uid=0, block=3, file=0):
        """
        Delete a file from a device.
        uid : ID of device on which to delete file.
        block : block where file resides (default ISF)
        file : ID of file
        """

    def newfile(self, uid=0, block=3, file_id=0):
        """
        Add a new file to a device.  If the file ID is not in the DB template,
        newfile() will fail, and no file will be created.
        uid : ID of device on which to delete file.
        block : block where file resides (default ISF)
        file : ID of file to create.
        """

    def restore(self, uid=0, block=3, file_id=0):
        """
        Restore a file on a device to the default value specified in the DB template.
        uid : ID of device on which to restore file.
        block : block where file resides (default ISF)
        file : ID of file to restore.
        """

    def readperms(self, uid=0, block=3, file_id=0):
        """
        Read file permissions from a specified file.
        Permissions are returned as a bitfield (unsigned 8bit integer)
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        """

    def readheader(self, uid=0, block=3, file_id=0):
        """
        Read file header from a specified file.
        Header is returned as a dictionary -- see data definitions for more information.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        """

    def read(self, uid=0, block=3, file_id=0, offset=0, length=-1):
        """
        Read file data from a specified file.
        Data is returned as a dictionary -- see data definitions for more information.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        offset: byte offset into file, to begin reading.  Default 0.
        length: number of bytes to return (max).  -1 means read all remaining bytes.  Default -1.
        """

    def readall(self, uid=0, block=3, file_id=0, offset=0, length=-1):
        """
        Read file header and data from a specified file.
        Data is returned as a dictionary -- see data definitions for more information.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        offset: byte offset into file, to begin reading.  Default 0.
        length: number of bytes to return (max).  -1 means read all remaining bytes.  Default -1.
        """

    def writeperms(self, uid=0, block=3, file_id=0, file_perms=0):
        """
        Write file permissions to a specified file.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        file_perms : a bitfield (unsigned 8bit integer) -- see data definitions
        """

    def write(self, uid=0, block=3, file_id=0, offset=0, data=None):
        """
        Write data to a specified file.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        offset : byte offset into file, to begin writing.  Default 0.
        data : bytes object for data to write
        """





#-------------------------------------------------------------------------------------
# AppHandler: class for reading otter outputs and doing something with them
class AppHandler:

# Internal Functions --------------------------------

    def _appLoop(self):

        
        # Main Run Loop: wait for message to come over socket.  
        # It will be JSON.  We care about 'type':'rxstat' messages.
        sock_err = 0
        while getattr(self.appthread, "do_run", True):
            try:
                msg = sock_file.readline()
            except (socket.error, socket.timeout):
                sock_err = -5
                break
            try:
                msg = json.loads(msg)
            except ValueError:
                continue
            
            # Only accept rxstat messages with data objects, and pass app parser
            if 'type' in msg.keys():
                if msg['type'] == 'rxstat':
                    if 'data' in msg.keys():
                        msg = msg['data']
                        try:
                            qual = msg['qual']
                            alp = msg['alp']
                        except ValueError:
                            continue
                        if (qual == 0) and isinstance(alp, dict):
                            otdbmsg = self._parsealp(alp)
                            errcode = self._updateDB(otdbmsg)
                            if errcode == -1:
                                printf("APP: Data could not be parsed\n")
                            elif errcode == -2:
                                printf("APP: Parsed data lacks 'msg' field\n")
                            elif errcode == -3:
                                printf("APP: Error connecting to socket %s\n", self.dbsock)
                            elif errcode == -4:
                                printf("APP: Error opening socket as file\n")
                            elif errcode == -5:
                                printf("APP: Error in sending data to socket\n")
                            elif errcode == -6:
                                printf("APP: Error parsing data read from socket\n")
                            elif errcode != 0:
                                printf("APP: Command error on OTDB\n")
        
        sock_file.close()
        self.devsockobj.close()
        return sock_err


    def _parsealp(self, alp=None):
        """
        This function could be greatly expanded, or perhaps fed a list of application
        callbacks, but right now it just looks for something along the lines of: 
        "alp":{
            "id":4, "cmd":2, "len":102, "fmt":"text", 
            "dat":{
                "tgloc":{
                    "token":"00000101", "acc":104, "rssi":-75, "link":75, 
                    "elat":377769216, "elon":-1223957248
                }
            }
        }
        """
        if isinstance(alp, dict):
            if ("id" in alp.keys()) and \
                ("cmd" in alp.keys()) and \
                ("len" in alp.keys()) and \
                ("fmt" in alp.keys()) and \
                ("dat" in alp.keys()):
                # This is where application callbacks could come into play.
                # Right now we care only about id=4, cmd=2, fmt=text, dat={tgloc:...}
                if (alp['id'] == 4) and (alp['cmd'] == 2) and isinstance(alp['dat'], dict):
                    return self._parsetgloc(alp['dat'])
    
        return None


    def _parsetgloc(self, tgloc=None):
        """ 
        Application handler for tgloc data
        """
        try:
            if "tgloc" in tgloc.keys():
                tgloc = tgloc['tgloc']
                istgloc = True
            else:
                istgloc = False
        except ValueError:
            return []
            
        if not istgloc:
            return []
            
        param = dict()
        param['msg'] = "tgloc"
        param['token'] = 0
        param['acc'] = 0
        param['rssi'] = 0
        param['link'] = 0
        param['elat'] = 0
        param['elon'] = 0

        if 'token' in tgloc:
            param['token'] = int(tgloc['token'], 16)
        if 'acc' in tgloc:
            param['acc'] = int(tgloc['acc'])
        if 'rssi' in tgloc:
            param['rssi'] = int(tgloc['rssi'])
        if 'link' in tgloc:
            param['link'] = int(tgloc['link'])
        if 'elat' in tgloc:
            param['elat'] = float(tgloc['elat'])
        if 'elon' in tgloc:
            param['elon'] = float(tgloc['elon'])
            
        # This list will store commands to send to OTDB
        DBcmd = list()
        serbytes = bytearray(20)
        struct.pack_into("s", serbytes, 0, b'\x00')  # flags
        struct.pack_into("<B", serbytes, 1, abs(int(param['acc'])))
        struct.pack_into("<B", serbytes, 2, abs(int(param['rssi'])))
        struct.pack_into("<B", serbytes, 3, abs(int(param['link'])))
        struct.pack_into("<i", serbytes, 4, int(param['elat']))
        struct.pack_into("<i", serbytes, 8, int(param['elon']))
        struct.pack_into("4s", serbytes, 12, b'\x00\x00\x00\x00')  # TODO convert token into UID
        struct.pack_into("<i", serbytes, 16, int(param['token']))
        hextoken = format(param['token'], 'x')
        DBcmd.append("dev-new -j " + hextoken + " NULL\n")
        otdbcmd = "w -j -i " + hextoken + " 25 [" + serbytes.hex() + "]\n"
        DBcmd.append(otdbcmd)
        
        return DBcmd


    def _updateDB(self, DBcmd=None):
        """
        :param DBcmd: list of commands to send to OTDB.
        :return: 0 on success, else a negative integer

        This function contains the main application logic, as far as what to do with the DB depending on data received
        from the log message.
        """
        # TODO other msg expanders could go here
        sock_err = 0

        if len(DBcmd) > 0:
            # connect to the socket
            try:
                self.dbsockobj = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.dbsockobj.connect(self.dbsock)
            except socket.error as exc:
                printf("LOG: Socket connection error : %s\n", exc)
                return -3
            try:
                sock_file = self.dbsockobj.makefile()
            except OSError:
                self.dbsockobj.close()
                return -4

            while len(DBcmd) > 0:
                otdbcmd = DBcmd.pop(0)
                print("LOG: Sending to OTDB: " + str(otdbcmd))
                try:
                    self.dbsockobj.sendall(otdbcmd.encode('utf-8'))
                    otdb_resp = sock_file.readline()
                except (socket.error, socket.timeout):
                    sock_err = -5
                    break
                print(otdb_resp)
                try:
                    otdb_resp = json.loads(otdb_resp)
                except ValueError:
                    sock_err = -6
                    break
                if 'err' in otdb_resp:
                    errcode = int(otdb_resp['err'])
                    if errcode != 0:
                        printf("LOG: Error %d on command: %s\n", errcode, otdbcmd)

            sock_file.close()
            self.dbsockobj.close()

        return sock_err

# END OF LogHandler
#-------------------------------------------------------------------------------------




#-------------------------------------------------------------------------------------
# PROGRAM FRONTEND

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Terminal Wrapper for Application")
    parser.add_argument('--otdb', default='/opt/otdb/otdb.sock', help="OTDB Socket")
    parser.add_argument('--otter', default='/opt/otdb/otter.sock', help="OTTER Socket")
    parser.add_argument('--local', action='store_true', help="Run webserver as localhost")

    args = parser.parse_args()

    assetList = list()

    otdb_sockfile="/opt/otdb/otdb.sock"
    otter_sockfile="/opt/otdb/otter.sock"

    # Need to do this ahead of process startup
    log_app = AppHandler(otter_sockfile, otdb_sockfile)

    # Start Application Thread
    if log_app.start() != 0:
        print("Log Application could not be started: Exiting.")
    else:
        # This is a semaphore that waits for Ctrl-C
        print("-------- Application Logic is started --------")
        signal.sigwait([signal.SIGINT])
        print("\n-------- SIGINT received, terminating app --------")
        log_app.stop()

    sys.exit(0)

