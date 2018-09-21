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

// Local Headers
#include "otdb_cli.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>



static int sub_sendcmd(const char* cmd) {
    
    return 0;
}



int otdb_newdevice(uint64_t device_id, const char* tmpl_path) {
    int rc;
    char argstring[256];
    char* cursor;
    int limit;
    int path_len;
    
    cursor  = stpcpy(argstring, "dev-new ");
    limit   = sizeof(argstring) - 1 - (int)(cursor - argstring);
    
    if (device_id != 0) {
        int printsz;
        printsz = snprintf(cursor, 3+16+2, "-i %16llX ", device_id);
        limit  -= printsz;
        cursor += printsz;
    }
    
    path_len    = (int)strlen(tmpl_path);
    limit      -= path_len;
    
    if (limit > 0) {
        cursor[path_len] = 0;
        memcpy(cursor, tmpl_path, path_len);
        rc = sub_sendcmd((const char*)argstring);
    }
    else {
        rc = -1;
    }
    
    return rc;
}


int otdb_deldevice(uint64_t device_id) {
    int rc;
    char argstring[32];
    char* cursor;
    int cpylen;
    
    cursor  = stpcpy(argstring, "dev-del ");
    cpylen  = snprintf(cursor, 16+2, "%16llX", device_id);
    rc      = sub_sendcmd((const char*)argstring);
    
    return rc;
}


int otdb_setdevice(uint64_t device_id) {
    int rc;
    char argstring[32];
    char* cursor;
    int cpylen;
    
    cursor  = stpcpy(argstring, "dev-set ");
    cpylen  = snprintf(cursor, 16+2, "%16llX", device_id);
    rc      = sub_sendcmd((const char*)argstring);
    
    return rc;
}


int otdb_opendb(const char* input_path) {
    int rc;
    char argstring[256];
    char* cursor;
    int limit;
    int cpylen;
    
    cursor  = stpcpy(argstring, "open ");
    limit   = sizeof(argstring) - 1 - (int)(cursor - argstring);
    cpylen  = (int)strlen(input_path);
    limit  -= cpylen;
    
    if (limit > 0) {
        cursor[cpylen] = 0;
        memcpy(cursor, input_path, cpylen);
        rc = sub_sendcmd((const char*)argstring);
    }
    else {
        rc = -1;
    }
    
    return rc;
}


int otdb_savedb(bool use_compression, const char* output_path) {
    int rc;
    char argstring[256];
    char* cursor;
    int limit;
    int cpylen;
    
    cursor = stpcpy(argstring, "save ");
    if (use_compression) {
        cursor  = stpcpy(cursor, "-c ");
    }
    
    limit   = sizeof(argstring) - 1 - (int)(cursor - argstring);
    cpylen  = (int)strlen(output_path);
    limit  -= cpylen;
    
    if (limit > 0) {
        cursor[cpylen] = 0;
        memcpy(cursor, output_path, cpylen);
        rc = sub_sendcmd((const char*)argstring);
    }
    else {
        rc = -1;
    }
    
    return rc;
}





/** OTDB Filesystem Commands
  * -------------------------------------------------------------------------
  */


int otdb_delfile(uint64_t device_id, otdb_fblock_enum block, unsigned int file_id) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    cursor  = stpcpy(argstring, "del ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %16llX ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "%u", file_id);
    //cursor += cpylen;
    //limit  -= cpylen; 
    
    rc      = sub_sendcmd((const char*)argstring);
    
    return rc;
}



int otdb_newfile(   uint64_t device_id, otdb_fblock_enum block, unsigned int file_id,
                    unsigned int file_perms, unsigned int file_alloc) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    cursor  = stpcpy(argstring, "new ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %16llX ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "%u ", file_id);
    cursor += cpylen;
    limit  -= cpylen; 
    
    cpylen  = snprintf(cursor, limit, "%o ", (file_perms & 63));
    cursor += cpylen;
    limit  -= cpylen; 
    
    cpylen  = snprintf(cursor, limit, "%u", file_alloc);
    //cursor += cpylen;
    //limit  -= cpylen; 
    
    rc      = sub_sendcmd((const char*)argstring);
    
    return rc;
}


int otdb_read(otdb_filedata_t* output_data, 
        uint64_t device_id, otdb_fblock_enum block, unsigned int file_id,
        unsigned int read_offset, int read_size) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo this is almost identical to readall (r*)
    
    cursor  = stpcpy(argstring, "r ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %16llX ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (read_size <= 0) {
        read_size = 65535;
    }
    else {
        read_size += read_offset;
    }
    
    cpylen  = snprintf(cursor, limit, "-r %u:%u ", read_offset, read_size);
    cursor += cpylen;
    limit  -= cpylen; 
    
    cpylen  = snprintf(cursor, limit, "%u ", file_id);
    //cursor += cpylen;
    //limit  -= cpylen; 

    rc      = sub_sendcmd((const char*)argstring);
    
    ///@todo write-out header and data
    
    return rc;
}



int otdb_readall(otdb_filehdr_t* output_hdr, otdb_filedata_t* output_data, 
        uint64_t device_id, otdb_fblock_enum block, unsigned int file_id,
        unsigned int read_offset, int read_size) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo this is almost identical to read (r*)
    
    cursor  = stpcpy(argstring, "r* ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %16llX ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (read_size <= 0) {
        read_size = 65535;
    }
    else {
        read_size += read_offset;
    }
    
    cpylen  = snprintf(cursor, limit, "-r %u:%u ", read_offset, read_size);
    cursor += cpylen;
    limit  -= cpylen; 
    
    cpylen  = snprintf(cursor, limit, "%u", file_id);
    //cursor += cpylen;
    //limit  -= cpylen; 

    rc      = sub_sendcmd((const char*)argstring);
    
    ///@todo write-out header and data
    
    return rc;
}



int otdb_restore(uint64_t device_id, otdb_fblock_enum block, unsigned int file_id) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo almost identical to restore, readhdr, readperms
    
    cursor  = stpcpy(argstring, "z ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %16llX ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "%u", file_id);
    //cursor += cpylen;
    //limit  -= cpylen; 

    rc      = sub_sendcmd((const char*)argstring);
    
    return rc;
}



int otdb_readhdr(otdb_filehdr_t* output_hdr, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo almost identical to restore, readhdr, readperms
    
    cursor  = stpcpy(argstring, "rh ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %16llX ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "%u", file_id);
    //cursor += cpylen;
    //limit  -= cpylen; 

    rc      = sub_sendcmd((const char*)argstring);
    
    ///@todo write-out header
    
    return rc;
}



int otdb_readperms(unsigned int* output_perms, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo almost identical to restore, readhdr, readperms
    
    cursor  = stpcpy(argstring, "rp ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %16llX ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "%u", file_id);
    //cursor += cpylen;
    //limit  -= cpylen; 

    rc      = sub_sendcmd((const char*)argstring);
    
    ///@todo write-out perms
    
    return rc;
}



int otdb_writedata(uint64_t device_id, otdb_fblock_enum block, unsigned int file_id, 
        unsigned int data_offset, unsigned int data_size, uint8_t* writedata) {
    
    
        
}




int otdb_writeperms(uint64_t device_id, otdb_fblock_enum block, unsigned int file_id, unsigned int file_perms) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    cursor  = stpcpy(argstring, "new ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %16llX ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "%u ", file_id);
    cursor += cpylen;
    limit  -= cpylen; 
    
    cpylen  = snprintf(cursor, limit, "%o", (file_perms & 63));
    //cursor += cpylen;
    //limit  -= cpylen; 
    
    rc      = sub_sendcmd((const char*)argstring);
    
    return rc;
}



