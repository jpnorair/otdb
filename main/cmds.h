/* Copyright 2014, JP Norair
  *
  * Licensed under the OpenTag License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  */

#ifndef cmds_h
#define cmds_h

// Local Headers
#include "dterm.h"

// POSIX & Standard C Libraries
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#define ARGFIELD_DEVICEID       (1<<0)
#define ARGFIELD_DEVICEIDOPT    (1<<1)
#define ARGFIELD_DEVICEIDLIST   (1<<2)
#define ARGFIELD_ARCHIVE        (1<<3)
#define ARGFIELD_COMPRESS       (1<<4)
#define ARGFIELD_JSONOUT        (1<<5)
#define ARGFIELD_AGEMS          (1<<6)
#define ARGFIELD_BLOCKID        (1<<7)
#define ARGFIELD_FILEID         (1<<8)
#define ARGFIELD_FILEPERMS      (1<<9)
#define ARGFIELD_FILEALLOC      (1<<10)
#define ARGFIELD_FILERANGE      (1<<11)
#define ARGFIELD_FILEDATA       (1<<12)

typedef struct {
    unsigned int    fields;
    const char*     archive_path;
    uint8_t*        filedata;
    int             filedata_size;
    uint64_t        devid;
    const char**    devid_strlist;
    int             devid_strlist_size;
    int             age_ms;
    uint8_t         jsonout_flag;
    uint8_t         compress_flag;
    uint8_t         block_id;
    uint8_t         file_id;
    uint8_t         file_perms;
    uint16_t        file_alloc;
    uint16_t        range_lo;
    uint16_t        range_hi;
} cmd_arglist_t;



// arg1: dst buffer
// arg2: src buffer
// arg3: dst buffer max size
typedef int (*cmdaction_t)(dterm_handle_t*, uint8_t*, int*, uint8_t*, size_t);



void cmd_init_args(void);


int cmd_hexread(uint8_t* dst, const char* src);
int cmd_hexnread(uint8_t* dst, const char* src, size_t dst_max);
int cmd_hexwrite(char* dst, const uint8_t* src, size_t src_bytes);
int cmd_hexnwrite(char* dst, const uint8_t* src, size_t src_bytes, size_t dst_max);


int cmd_extract_args(cmd_arglist_t* data, void* args, const char* cmdname, const char* src, int* src_bytes);



int cmd_jsonout_err(char* dst, size_t dstmax, bool jsonflag, int errcode, const char* cmdname);

int cmd_jsonout_fmt(char** dst, size_t* dstmax, bool jsonflag, int errcode, const char* cmdname, const char* fmt, ...);

int cmd_jsonout_data(char** dst, size_t* dstmax, bool jsonflag, int errcode, uint8_t* src, uint16_t offset, size_t srcbytes);


int cmd_rmdir(const char *dir);



int cmd_devmgr(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);


/** OTDB Internal Commands
  * -------------------------------------------------------------------------
  */

/** @brief Prints a list of commands supported by this command interface
  * @param dth      (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * cmdlist takes no further input.
  */
int cmd_cmdlist(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Quits OTDB
  * @param dth      (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * quit takes no further input.  It returns nothing, and the pipe or socket
  * used for interfacing with OTDB will go down.
  */
int cmd_quit(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);





/** OTDB DB Manipulation Commands
  * -------------------------------------------------------------------------
  */

/** @brief Creates a new device FS in the database
  * @param dth      (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * dev-new [-j] [-i ID] infile
  *
  * infile:     Input file.  This is either a directory or a compressed archive
  *             of the directory.
  */
int cmd_devnew(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Deletes a device FS from the database
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * dev-del [-j] ID
  *
  * ID:         Bintex formatted Device ID.  This device FS will be deleted.
  */
int cmd_devdel(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Sets the active device ID
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * @note if multiple users requesting data from OTDB, the Active ID cannot be
  * relied-on to have the same value across communication sessions.  Therefore,
  * the best practice is to use dev-set at the start of a communication, or not 
  * at all -- all commands can take an optional Device ID input.
  *
  * Protocol usage: text input
  * dev-set [-j] ID
  * 
  * ID is a bintex expression.
  */
int cmd_devset(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Opens a database file and loads into memory
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * open [-j] infile
  *
  * infile:     Input file.  This is either a directory or a compressed archive
  *             depending on the way it is saved (-c option or not).
  */
int cmd_open(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Saves/Exports the active database to file
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * save [-jc] outfile [IDlist]
  *
  * -c:         Optional argument to compress output.  Compression is 7z type.
  *
  * outfile:    File name of the saved output.
  *             If compression is not used, the output will be a directory with
  *             this name, with an internal structure of subdirectories and 
  *             JSON files.
  * 
  * ID is a bintex expression.
  */
int cmd_save(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);






/** OTDB Filesystem Commands
  * -------------------------------------------------------------------------
  */


/** @brief Delete a file on a device
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * del [-j] [-i ID] [-b block] file_id
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  */
int cmd_del(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Create a file on a device
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * new [-j] [-i ID] [-b block] file_id perms alloc
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  *
  * alloc:  the maximum data contained by the file in bytes
  * perms:  Octal permission string, with two permission digits
  */
int cmd_new(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Read a file from a device
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * r [-j] [-i ID] [-b block] [-r range] file_id
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  *
  * range:  A byte range such as 0:16 (first 16 bytes), :16 (first 16 bytes),
  *         8: (all bytes after 7th), etc.  Defaults to 0:
  */
int cmd_read(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Read a file and file headers from a device
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * r* [-j] [-i ID] [-b block] [-r range] file_id 
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  *
  * range:  A byte range such as 0:16 (first 16 bytes), :16 (first 16 bytes),
  *         8: (all bytes after 7th), etc.  Defaults to 0:
  *
  * This function is very similar to read, but it also returns the file headers
  * before the file data.  Headers and data are separated with whitespace in
  * the output.
  */
int cmd_readall(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Restore a file on a device to its defaults
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * z [-j] [-i ID] [-b block] file_id
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  */
int cmd_restore(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Read the header from a file on a device
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * rh [-j] [-i ID] [-b block] file_id
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  */
int cmd_readhdr(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Read the permissions from a file on a device
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * rp [-j] [-i ID] [-b block] file_id
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  */
int cmd_readperms(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Write data to a file on a device
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * w [-j] [-i ID] [-b block] [-r range] file_id writedata
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  *
  * If range is supplied and if the writedata goes beyond the range, it will be 
  * clipped to fit within the range.
  *
  * If range is not supplied, range will start at 0 and be derived from the 
  * length of the supplied writedata.
  * 
  * writedata is sent as BINTEX
  */
int cmd_write(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Write permissions to a file on a device
  * @param dth       (dterm_handle_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * wp [-j] [-i ID] [-b block] file_id perms
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  *
  * Perms are an octal string of two digits, as with other permissions
  */
int cmd_writeperms(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);




#endif
