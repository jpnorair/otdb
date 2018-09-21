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

#ifndef cmdsearch_h
#define cmdsearch_h

// Local Dependencies
#include "cmds.h"
#include "dterm.h"

// External Dependencies
#include <cmdtab.h>


typedef enum {
    BLOCK_gfb   = 1,
    BLOCK_iss   = 2,
    BLOCK_isf   = 3
} otdb_fblock_enum;

typedef struct {
    uint64_t            device_id;
    uint32_t            timestamp;
    otdb_fblock_enum    block;
    uint8_t             id;
    uint8_t             perms;
    uint16_t            length;
    uint16_t            alloc;
} otdb_filehdr_t;

typedef struct {
    uint8_t*    data;
    uint16_t    offset;
    uint16_t    length;
} otdb_filedata_t;



///@todo otdb start client and get socket handle




/** @brief Loads new device FS in the database
  * @param device_id    (uint64_t) Device ID for new device
  * @param tmpl_path    (const char*) Path to saved device FS template file
  * @retval             Returns 0 on success
  *
  * If tmpl_path is NULL, then a new entry will be created for the default FS
  * using the latest data.
  *
  * If device_id is 0, the device ID(s) will be taken from the device FS.
  * Situations where device_id=0 and tmpl_path=NULL can often fail, as the 
  * default ID(s) are likely to exist in the DB already.
  */
int otdb_newdevice(uint64_t device_id, const char* tmpl_path);


/** @brief Deletes a device FS from the database
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @retval             Returns 0 on success
  */
int otdb_deldevice(uint64_t device_id);


/** @brief Sets the active device ID
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @retval             Returns 0 on success
  */
int otdb_setdevice(uint64_t device_id);


/** @brief Opens a database file and loads into memory
  * @param input_path   (const char*) Path to saved database file 
  * @retval             Returns 0 on success
  */
int otdb_opendb(const char* input_path);


/** @brief Saves the active database to file
  * @param use_compression  (bool) Enables compression of save file
  * @param output_path      (const char*) Path to save file
  * @retval                 Returns 0 on success
  *
  */
int otdb_savedb(bool use_compression, const char* output_path);





/** OTDB Filesystem Commands
  * -------------------------------------------------------------------------
  */


/** @brief Delete a file on a device
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @param block        (otdb_fblock_enum) File Block (usually BLOCK_isf)
  * @param file_id      (unsigned int) File ID
  * @retval             Returns 0 on success
  */
int otdb_delfile(uint64_t device_id, otdb_fblock_enum block, unsigned int file_id);


/** @brief Create a new file on a device
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @param block        (otdb_fblock_enum) File Block (usually BLOCK_isf)
  * @param file_id      (unsigned int) File ID
  * @param file_perms   (unsigned int) New File Permissions
  * @param file_alloc   (unsigned int) New File Allocation in Bytes
  * @retval             Returns 0 on success
  */
int otdb_newfile(uint64_t device_id, otdb_fblock_enum block, unsigned int file_id,
        unsigned int file_perms, unsigned int file_alloc);


/** @brief Read a file from a device
  * @param output_data  (otdb_filedata_t*) result parameter for read data
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @param block        (otdb_fblock_enum) File Block (usually BLOCK_isf)
  * @param file_id      (unsigned int) File ID
  * @param read_offset  (unsigned int) file byte offset to begin read
  * @param read_size    (int) number of bytes to read, from offset. -1 reads entire file.
  * @retval             Returns 0 on success
  */
int otdb_read(otdb_filedata_t* output_data, 
        uint64_t device_id, otdb_fblock_enum block, unsigned int file_id,
        unsigned int read_offset, int read_size);


/** @brief Read a file and file headers from a device
  * @param output_hdr   (otdb_filehdr_t*) result parameter for read header
  * @param output_data  (otdb_filedata_t*) result parameter for read data
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @param block        (otdb_fblock_enum) File Block (usually BLOCK_isf)
  * @param file_id      (unsigned int) File ID
  * @param read_offset  (unsigned int) file byte offset to begin read
  * @param read_size    (int) number of bytes to read, from offset. -1 reads entire file.
  * @retval             Returns 0 on success
  */
int otdb_readall(otdb_filehdr_t* output_hdr, otdb_filedata_t* output_data, 
        uint64_t device_id, otdb_fblock_enum block, unsigned int file_id,
        unsigned int read_offset, int read_size);


/** @brief Restore a file on a device to its defaults
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @param block        (otdb_fblock_enum) File Block (usually BLOCK_isf)
  * @param file_id      (unsigned int) File ID
  * @retval             Returns 0 on success
  */
int otdb_restore(uint64_t device_id, otdb_fblock_enum block, unsigned int file_id);


/** @brief Read the header from a file on a device
  * @param output_hdr   (otdb_filehdr_t*) result parameter for read header
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @param block        (otdb_fblock_enum) File Block (usually BLOCK_isf)
  * @param file_id      (unsigned int) File ID
  * @retval             Returns 0 on success
  */
int otdb_readhdr(otdb_filehdr_t* output_hdr, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id);


/** @brief Read permissions from a file
  * @param output_perms (unsigned int*) result parameter for read permissions
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @param block        (otdb_fblock_enum) File Block (usually BLOCK_isf)
  * @param file_id      (unsigned int) File ID
  * @retval             Returns 0 on success
  */
int otdb_readperms(unsigned int* output_perms, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id);


/** @brief Write data to a file on a device
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @param block        (otdb_fblock_enum) File Block (usually BLOCK_isf)
  * @param file_id      (unsigned int) File ID
  * @param data_offset  (unsigned int) Byte offset into file
  * @param data_size    (unsigned int) size of data to write
  * @param writedata    (uint8_t*) Bytewise data to write to file
  * @retval             Returns 0 on success
  */
int otdb_writedata(uint64_t device_id, otdb_fblock_enum block, unsigned int file_id, 
        unsigned int data_offset, unsigned int data_size, uint8_t* writedata);



/** @brief Write permissions to a file on a device
  * @param device_id    (uint64_t) 64 bit Device ID.  Use 0 to specify last-used Device.
  * @param block        (otdb_fblock_enum) File Block (usually BLOCK_isf)
  * @param file_id      (unsigned int) File ID
  * @param file_perms   (unsigned int) New File Permissions
  * @retval             Returns 0 on success
  */
int otdb_writeperms(uint64_t device_id, otdb_fblock_enum block, unsigned int file_id, unsigned int file_perms);




#endif
