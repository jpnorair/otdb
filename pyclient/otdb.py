#! python3

import sys
import os
import signal
import argparse
import socket
import json

#-------------------------------------------------------------------------------------
# PROGRAM DEFAULTS



#-------------------------------------------------------------------------------------
# GENERIC STATIC FUNCTIONS

def printf(fmt, *params):
    sys.stdout.write(fmt % params)


# TODO put static functions here


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


    # ------------------------------------------------------------
    # These functions may be static

    def _print_uid(self, uid):
        if isinstance(uid, bytes) or isinstance(uid, bytearray):
            return uid[:8].hex()
        elif isinstance(uid, int):
            return (uid.to_bytes(8, byteorder='big', signed=False)).hex()
        else:
            return "0000000000000000"

    def _print_range(self, offset, length):
        rangestr = str(max(offset, 0)) + ":"
        if length >= 0:
            rangestr = rangestr + str(length)
        return rangestr

    def _validate_jsonresp(self, jsonresp):
        try:
            msg = json.loads(jsonresp)
        except ValueError:
            return False, None

        if "err" in msg:
            if msg["err"] == 0
                return True, msg

        return False, msg

    # End of static functions
    # ------------------------------------------------------------


    def newdevice(self, new_uid, new_data=None):
        """
        Create a new device, given a supplied UID, according to the DB template.
        returns True or False, if command has succeeded or failed
        new_uid : a 64bit bytes object or an unsigned integer
        """
        cmdstr = "dev-new -j " + self._print_uid(self, new_uid)
        if os.path.isdir(new_data):
            cmdstr = cmdstr + " " + str(new_data)

        resp = self._sendcmd(self, cmdstr)
        test, _ = self._validate_jsonresp(self, resp)
        return test


    def deldevice(self, del_uid):
        """
        Delete a device from the OTDB, given a supplied UID.
        returns True or False, if command has succeeded or failed
        del_uid : a 64bit bytes object or an unsigned integer
        """
        cmdstr = "dev-del -j " + self._print_uid(self, del_uid)
        resp = self._sendcmd(self, cmdstr)
        test, _ = self._validate_jsonresp(self, resp)
        return test


    def setdevice(self, set_uid):
        """
        Set the default UID of the OTDB.
        Don't use this method unless you really know what you're doing.
        It can cause problems if OTDB is handling multiple clients.
        returns True or False, if command has succeeded or failed
        set_uid : a 64bit bytes object or an unsigned integer
        """
        cmdstr = "dev-set -j " + self._print_uid(self, set_uid)
        resp = self._sendcmd(self, cmdstr)
        test, _ = self._validate_jsonresp(self, resp)
        return test


    def load(self, dbpath=""):
        """
        Load a new database template (and device list) into the OTDB context.
        Don't use this method unless you really know what you're doing.
        The OTDB context is typically set when the OTDB daemon is started.
        returns True or False, if command has succeeded or failed
        dbpath : path to the DB archive to load.
        """
        if os.path.isdir(dbpath):
            cmdstr = "open -j " + str(dbpath)
            resp = self._sendcmd(self, cmdstr)
            test, _ = self._validate_jsonresp(self, resp)
            return test
        else:
            return False


    def save(self, compress=False, uidlist=None, arcpath="./"):
        """
        Save the current OTDB context into a archive at a supplied path.
        returns True or False, if command has succeeded or failed.
        compress : true or false (default false) to compress archive
        uidlist : a list containing UIDs to save.  Default is None, which
            will save all the UIDs in the OTDB
        arcpath : path to save the DB archive.  Default is "./".
        """
        if not os.path.isdir(arcpath):
            return False

        cmdstr = "save -j "

        if compress:
            cmdstr = cmdstr + "-c "

        if isinstance(uidlist, list):
            for uid in uidlist:
                cmdstr = cmdstr + self._print_uid(self, uid) + " "

        cmdstr = cmdstr + str(arcpath)
        resp = self._sendcmd(self, cmdstr)
        test, _ = self._validate_jsonresp(self, resp)
        return test


    def f_del(self, uid=0, block=3, file=0):
        """
        Delete a file from a device.
        returns True or False, if command has succeeded or failed.
        uid : ID of device on which to delete file.
        block : block where file resides (default ISF)
        file : ID of file
        """
        cmdstr = "del -j -i " \
                 + self._print_uid(self, uid) + \
                 " -b " + str(block) + \
                 " " + str(file)
        resp = self._sendcmd(self, cmdstr)
        test, _ = self._validate_jsonresp(self, resp)
        return test


    def f_new(self, uid=0, block=3, file=0, perms=0, alloc=0):
        """
        Add a new file to a device.  If the file ID is not in the DB template,
        newfile() will fail, and no file will be created.
        returns True or False, if command has succeeded or failed.
        uid : ID of device on which to delete file.
        block : block where file resides (default ISF)
        file : ID of file to create.
        """
        cmdstr = "del -j -i " \
                 + self._print_uid(self, uid) + \
                 " -b " + str(block) + \
                 " " + str(file) + \
                 " " + oct(perms) + \
                 " " + str(alloc)
        resp = self._sendcmd(self, cmdstr)
        test, _ = self._validate_jsonresp(self, resp)
        return test


    def f_restore(self, uid=0, block=3, file=0):
        """
        Restore a file on a device to the default value specified in the DB template.
        uid : ID of device on which to restore file.
        block : block where file resides (default ISF)
        file : ID of file to restore.
        """
        cmdstr = "z -j -i " \
                 + self._print_uid(self, uid) + \
                 " -b " + str(block) + \
                 " " + str(file)
        resp = self._sendcmd(self, cmdstr)
        test, _ = self._validate_jsonresp(self, resp)
        return test


    def f_readperms(self, age=-1, uid=0, block=3, file=0):
        """
        Read file permissions from a specified file.
        Permissions are returned as an integer which corresponds to octal
        formatted perm bits.  If -1 is returned, there was an error.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        """
        cmdstr = "rp -j -i "
        if age >= 0:
            cmdstr = cmdstr + "-a " + str(age)
        cmdstr = cmdstr + " -i " + self._print_uid(self, uid) + \
                 " -b " + str(block) + \
                 " " + str(file)

        # Send the command and then return the permissions if no error
        resp = self._sendcmd(self, cmdstr)
        test, jsonresp = self._validate_jsonresp(self, resp)
        if test:
            if "mod" in jsonresp:
                return int(jsonresp["mod"])

        return -1


    def f_readheader(self, age=-1, uid=0, block=3, file=0):
        """
        Read file header from a specified file.
        Header is returned as a dictionary -- see data definitions for more information.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        """
        cmdstr = "rh -j -i "
        if age >= 0:
            cmdstr = cmdstr + "-a " + str(age)
        cmdstr = cmdstr + " -i " + self._print_uid(self, uid) + \
                 " -b " + str(block) + \
                 " " + str(file)

        # Send the command and then return the part of the dictionary
        # with header components, if successful
        resp = self._sendcmd(self, cmdstr)
        test, jsonresp = self._validate_jsonresp(self, resp)
        if test:
            try:
                del jsonresp["cmd"]
            except KeyError:
                return None
            return jsonresp

        return None


    def f_read(self, age=-1, uid=0, block=3, file=0, offset=0, length=-1):
        """
        Read file data from a specified file.
        Data is returned as a dictionary -- see data definitions for more information.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        offset: byte offset into file, to begin reading.  Default 0.
        length: number of bytes to return (max).  -1 means read all remaining bytes.  Default -1.
        """
        cmdstr = "r -j -i "
        if age >= 0:
            cmdstr = cmdstr + "-a " + str(age)
        cmdstr = cmdstr + " -i " + self._print_uid(self, uid) + \
                 " -b " + str(block) + \
                 " -r " + self._print_range(self, offset, length) + \
                 " " + str(file)

        # Send the command and then return the part of the dictionary
        # with header components, if successful
        resp = self._sendcmd(self, cmdstr)
        test, jsonresp = self._validate_jsonresp(self, resp)
        if test:
            try:
                del jsonresp["cmd"]
            except KeyError:
                return None
            return jsonresp


    def f_readall(self, age=-1, uid=0, block=3, file=0, offset=0, length=-1):
        """
        Read file header and data from a specified file.
        Data is returned as a dictionary -- see data definitions for more information.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        offset: byte offset into file, to begin reading.  Default 0.
        length: number of bytes to return (max).  -1 means read all remaining bytes.  Default -1.
        """
        cmdstr = "r* -j -i "
        if age >= 0:
            cmdstr = cmdstr + "-a " + str(age)
        cmdstr = cmdstr + " -i " + self._print_uid(self, uid) + \
                 " -b " + str(block) + \
                 " -r " + self._print_range(self, offset, length) + \
                 " " + str(file)

        # Send the command and then return the part of the dictionary
        # with header components, if successful
        resp = self._sendcmd(self, cmdstr)
        test, jsonresp = self._validate_jsonresp(self, resp)
        if test:
            try:
                del jsonresp["cmd"]
            except KeyError:
                return None
            return jsonresp


    def f_writeperms(self, uid=0, block=3, file=0, file_perms=0):
        """
        Write file permissions to a specified file.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        file_perms : a bitfield (unsigned 8bit integer) -- see data definitions
        """
        cmdstr = "wp -j -i "
        cmdstr = cmdstr + " -i " + self._print_uid(self, uid) + \
                 " -b " + str(block) + \
                 " -r " + self._print_range(self, offset, length) + \
                 " " + str(file)

        # Send the command and then return true/false
        resp = self._sendcmd(self, cmdstr)
        test, _ = self._validate_jsonresp(self, resp)
        return test

    def f_write(self, uid=0, block=3, file=0, offset=0, data=None):
        """
        Write data to a specified file.
        uid : ID of device on which to read permissions.
        block : block where file resides (default ISF)
        file : ID of file to read.
        offset : byte offset into file, to begin writing.  Default 0.
        data : bytes object for data to write
        """
        #Right now, data must be bytes, but in the future it could be JSON optionally
        if isinstance(data, bytes) or isinstance(data, bytearray):
            datalen = len(data)
        else:
            return False

        cmdstr = "w -j -i "
        cmdstr = cmdstr + " -i " + self._print_uid(self, uid) + \
                 " -b " + str(block) + \
                 " -r " + self._print_range(self, offset, datalen) + \
                 " " + str(file) + \
                 " " + data.hex()

        # Send the command and then return true/false
        resp = self._sendcmd(self, cmdstr)
        test, _ = self._validate_jsonresp(self, resp)
        return test





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

