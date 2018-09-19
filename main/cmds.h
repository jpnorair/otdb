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





// arg1: dst buffer
// arg2: src buffer
// arg3: dst buffer max size
typedef int (*cmdaction_t)(dterm_t*, uint8_t*, int*, uint8_t*, size_t);


/** @brief Delete a file on a device
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * del [-i ID] [-b block] file_id
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  */
int cmd_del(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);


/** @brief Create a file on a device
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * new [-i ID] [-b block] file_id alloc perms
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  *
  * alloc:  the maximum data contained by the file in bytes
  * perms:  Octal permission string, with two permission digits
  */
int cmd_new(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);


/** @brief Read a file from a device
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * r [-i ID] [-b block] file_id [range]
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  *
  * range:  A byte range such as 0:16 (first 16 bytes), :16 (first 16 bytes),
  *         8: (all bytes after 7th), etc.  Defaults to 0:
  */
int cmd_read(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Read a file and file headers from a device
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * r* [-i ID] [-b block] file_id [range]
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
int cmd_readall(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Restore a file on a device to its defaults
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * z [-i ID] [-b block] file_id
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  */
int cmd_restore(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Read the header from a file on a device
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * rh [-i ID] [-b block] file_id
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  */
int cmd_readhdr(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Read the permissions from a file on a device
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * rp [-i ID] [-b block] file_id
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  */
int cmd_readperms(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Write data to a file on a device
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * w [-i ID] [-b block] file_id range writedata
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  *
  * Range is mandatory.  If the writedata goes beyond the range, it will be 
  * clipped to fit within the range.
  * 
  * writedata is sent as BINTEX
  */
int cmd_write(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);




/** @brief Write permissions to a file on a device
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * Protocol usage: text input
  * wp [-i ID] [-b block] file_id perms
  *
  * if -i is missing, it defaults to the active device
  * if -b is missing, it defaults to isf0
  *
  * Perms are an octal string of two digits, as with other permissions
  */
int cmd_writeperms(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



int cmd_open(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);
int cmd_save(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);




int cmd_setid(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Prints a list of commands supported by this command interface
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * cmdlist takes no further input.
  */
int cmd_cmdlist(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/** @brief Quits OTDB
  * @param dt       (dterm_t*) Controlling interface handle
  * @param dst      (uint8_t*) Protocol output buffer
  * @param inbytes  (int*) Protocol Input Bytes.  Also outputs adjusted input bytes.
  * @param src      (uint8_t*) Protocol input buffer
  * @param dstmax   (size_t) Maximum size of dst (Protocol output buffer)
  *
  * quit takes no further input.  It returns nothing, and the pipe or socket
  * used for interfacing with OTDB will go down.
  */
int cmd_quit(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);






#endif
