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
#include <otdb_cfg.h>

// HB Library Headers
#include <otfs.h>           

// Standard POSIX headers
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>


typedef struct {
    int     sockfd;
    struct  sockaddr_un sockaddr;
    int     connected;
    uint8_t* rxbuf;
    size_t  rxbuf_size;
} otdb_handle_t;





static char* sub_printhex(char* dst, uint8_t* src, size_t src_bytes) {
    static const char convert[] = "0123456789ABCDEF";

    while (src_bytes-- != 0) {
        *dst++ = convert[*src >> 4];
        *dst++ = convert[*src & 0x0f];
    }
    
    return dst;
}


static int sub_readhex(uint8_t* dst, char* src, size_t src_bytes) {
        static const uint8_t hexlut0[128] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 16, 32, 48, 64, 80, 96,112,128,144, 0, 0, 0, 0, 0, 0, 
        0,160,176,192,208,224,240,  0,  0,  0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,160,176,192,208,224,240,  0,  0,  0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static const uint8_t hexlut1[128] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 
        0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    
    uint8_t* start = dst;

    while (src_bytes > 1) {
        uint8_t byte;
        byte    = hexlut0[ *src++ & 0x7f];
        byte   += hexlut1[ *src++ & 0x7f];
        *dst++  = byte;
    }

    return (int)(dst - start);
}






static int sub_docmd(void* handle, const char* cmd, size_t cmdsize) {
    int rc;
    ssize_t bytes_out;
    ssize_t bytes_in;
    otdb_handle_t* otdb = handle;
    
    if (otdb == NULL) {
        return -1;
    }
    
    ///@todo test connection and only connect/disconnect if connection is not
    ///      present.  Might be done by testing send() output.
    rc = otdb_connect(otdb);
    
    if (rc >= 0) {
        
        // Send the command to OTDB, and go immediately into receive mode to 
        // get the response
        bytes_out   = send(otdb->sockfd, cmd, cmdsize, 0);
        
        ///@todo add a timeout to recv() somehow
        bytes_in    = recv(otdb->sockfd, otdb->rxbuf, otdb->rxbuf_size, MSG_WAITALL);
        
#       if OTDB_FEATURE_DEBUG
        if (bytes_out > 0) {
            fprintf(stdout, "Client sent %zd bytes:\n%*s\n\n", bytes_out, (int)bytes_out, otdb->rxbuf);
        }
        else {
            fprintf(stderr, "Client socket send error %d\n", errno);
        }
        if (bytes_in > 0) {
            fprintf(stdout, "OTDB received %zd bytes:\n%*s\n\n", bytes_in, (int)bytes_in, otdb->rxbuf);
        }
        else if (bytes_in == 0) {
            fprintf(stderr, "OTDB socket is closed, can't receive.\n");
        }
        else {
            fprintf(stderr, "OTDB socket receive error %d\n", errno);
        }
#       endif
        
        rc = (int)bytes_in;
    
        otdb_disconnect(handle);
    }
    
    return rc;
}



static int sub_get_errcode(void* handle) {
/// Extract an error code from a file operation
    otdb_handle_t* otdb = handle;
    int rc = 0;

    // Check if message is an error message.  If it's not an error message, 
    // then this can't be an error.  Return 0.
    // Error format that comes from OTDB is: "err %d"
    if (otdb->rxbuf_size > 4) {
        if (strncpy((char*)otdb->rxbuf, "err ", 4) == 0) {
            rc  = (int)strtol((const char*)&otdb->rxbuf[4], NULL, 10);
            
        }
    }
    return rc;
}






void* otdb_init(sa_family_t socktype, const char* sockpath, size_t rxbuf_size) {
    otdb_handle_t*  handle;

    handle = malloc(sizeof(otdb_handle_t));
    if (handle == NULL) {
        perror("Unable to create OTDB Client Instance.");
        goto otdb_init_END;
    }
    
    handle->rxbuf = malloc(rxbuf_size);
    if (handle->rxbuf == NULL) {
        perror("Unable to create OTDB Client RX Buffer.");
        goto otdb_init_TERM1;
    }
    
    handle->sockfd  = socket(socktype, SOCK_STREAM, 0);
    if (handle->sockfd < 0) {
        perror("Unable to create OTDB Client Socket.");
        goto otdb_init_TERM2;
    }
    
    memset(&handle->sockaddr, 0, sizeof(struct sockaddr_un));
    handle->sockaddr.sun_family = socktype;
    strncpy(handle->sockaddr.sun_path, sockpath, sizeof(handle->sockaddr.sun_path)-1);
    
    otdb_init_END:
    handle->connected = -1;
    return (void*)handle;
    
    otdb_init_TERM2:
    free(handle->rxbuf);
    
    otdb_init_TERM1:
    free(handle);
    return NULL;
}


void otdb_deinit(void* handle) {
    if (handle != NULL) {
        otdb_disconnect(handle);
        
        if (((otdb_handle_t*)handle)->rxbuf != NULL) {
            free(((otdb_handle_t*)handle)->rxbuf);
        }
        free(handle);
    }
}


int otdb_connect(void* handle) {
    int rc = -1;
    otdb_handle_t* otdb = handle;
    
    if (otdb != NULL) {
        if (otdb->connected < 0) {
            rc = connect(otdb->sockfd, (struct sockaddr*)&otdb->sockaddr, sizeof(struct sockaddr_un));
            otdb->connected = rc;
        }
        else {
            rc = 0;
        }
    }
    return rc;
}


int otdb_disconnect(void* handle) {
    int rc = -1;
    otdb_handle_t* otdb = handle;
    
    if (otdb != NULL) {
        if (otdb->connected < 0) {
            rc = 0;
        }
        else if (otdb->sockfd >= 0) {
            rc = close(otdb->sockfd);
            if (rc == 0) {
                otdb->connected = -1;
            }
        }
    }
    
    return rc;
}












int otdb_newdevice(void* handle, uint64_t device_id, const char* tmpl_path) {
    int rc;
    char argstring[256];
    char* cursor;
    int limit;
    int cpylen;
    
    cursor  = stpcpy(argstring, "dev-new ");
    limit   = sizeof(argstring) - 1 - (int)(cursor - argstring);
    
    if (device_id != 0) {
        int printsz;
        printsz = snprintf(cursor, 3+16+2, "-i %"PRIx64" ", device_id);
        limit  -= printsz;
        cursor += printsz;
    }
    
    cpylen  = (int)strlen(tmpl_path);
    limit  -= cpylen;
    
    if (limit > 0) {
        memcpy(cursor, tmpl_path, cpylen);
        cursor     += cpylen;
        *cursor++   = 0;
        rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
        if (rc > 0) {
            rc = sub_get_errcode(handle);
        }
    }
    else {
        rc = -1;
    }
    
    return rc;
}


int otdb_deldevice(void* handle, uint64_t device_id) {
    int rc;
    char argstring[32];
    char* cursor;
    int cpylen;
    
    cursor      = stpcpy(argstring, "dev-del ");
    cpylen      = snprintf(cursor, 16+2, "%"PRIx64, device_id);
    cursor     += cpylen;
    *cursor++   = 0;
    rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    if (rc > 0) {
        rc = sub_get_errcode(handle);
    }
    return rc;
}


int otdb_setdevice(void* handle, uint64_t device_id) {
    int rc;
    char argstring[32];
    char* cursor;
    int cpylen;
    
    cursor      = stpcpy(argstring, "dev-set ");
    cpylen      = snprintf(cursor, 16+2, "%"PRIx64, device_id);
    cursor     += cpylen;
    *cursor++   = 0;
    rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    if (rc > 0) {
        rc = sub_get_errcode(handle);
    }
    
    return rc;
}


int otdb_opendb(void* handle, const char* input_path) {
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
        memcpy(cursor, input_path, cpylen);
        cursor     += cpylen;
        *cursor++   = 0;
        rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
        if (rc > 0) {
            rc = sub_get_errcode(handle);
        }
    }
    else {
        rc = -1;
    }
    
    return rc;
}


int otdb_savedb(void* handle, bool use_compression, const char* output_path) {
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
        memcpy(cursor, output_path, cpylen);
        cursor     += cpylen;
        *cursor++   = 0;
        rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
        if (rc > 0) {
            rc = sub_get_errcode(handle);
        }
    }
    else {
        rc = -1;
    }
    
    return rc;
}





/** OTDB Filesystem Commands
  * -------------------------------------------------------------------------
  */


int otdb_delfile(void* handle, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    cursor  = stpcpy(argstring, "del ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %"PRIx64" ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "%u", file_id);
    cursor += cpylen;
    //limit  -= cpylen; 
    
    *cursor++   = 0;
    rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    
    if (rc > 0) {
        rc = sub_get_errcode(handle);
    }
    
    return rc;
}



int otdb_newfile(   void* handle, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id,
                    unsigned int file_perms, unsigned int file_alloc) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    cursor  = stpcpy(argstring, "new ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %"PRIx64" ", device_id);
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
    cursor += cpylen;
    //limit  -= cpylen; 
    
    *cursor++   = 0;
    rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    
    if (rc > 0) {
        rc = sub_get_errcode(handle);
    }
    
    return rc;
}


int otdb_read(void* handle, otdb_filedata_t* output_data, 
        uint64_t device_id, otdb_fblock_enum block, unsigned int file_id,
        unsigned int read_offset, int read_size) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo this is almost identical to readall (r*)
    
    cursor  = stpcpy(argstring, "r ");
    limit   = sizeof(argstring) - 1 - 2;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %"PRIx64" ", device_id);
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
    cursor += cpylen;
    //limit  -= cpylen; 

    *cursor++   = 0;
    rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    
    if (rc > 0) {
        rc = sub_get_errcode(handle);

        if (output_data != NULL) {
            otdb_handle_t* otdb = handle;
        
            // Data from otdb will be received as hex, convert to binary inplace
            output_data->ptr    = otdb->rxbuf;
            output_data->block  = block;
            output_data->fileid = file_id;
            output_data->offset = read_offset;
            output_data->length = sub_readhex(otdb->rxbuf, (char*)otdb->rxbuf, (size_t)rc);
        }
    }
    
    return rc;
}



int otdb_readall(void* handle, otdb_filehdr_t* output_hdr, otdb_filedata_t* output_data, 
        uint64_t device_id, otdb_fblock_enum block, unsigned int file_id,
        unsigned int read_offset, int read_size) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo this is almost identical to read (r)
    
    cursor  = stpcpy(argstring, "r* ");
    limit   = sizeof(argstring) - 1 - 3;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %"PRIx64" ", device_id);
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
    cursor += cpylen;
    //limit  -= cpylen; 

    *cursor++   = 0;
    rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    
    if (rc > 0) {
        int totalsize;
        otdb_handle_t* otdb = handle;
    
        rc = sub_get_errcode(handle);
    
        // Received Header is 10 bytes
        totalsize   = sub_readhex(otdb->rxbuf, (char*)otdb->rxbuf, (size_t)rc);

        if (output_hdr != NULL) {
            memset(output_hdr, 0, sizeof(otdb_filehdr_t));
            if (totalsize >= 10) {
                output_hdr->id      = otdb->rxbuf[0];
                output_hdr->perms   = otdb->rxbuf[1];
                memcpy(&output_hdr->length, &otdb->rxbuf[2], 2);
                memcpy(&output_hdr->alloc, &otdb->rxbuf[4], 2);
                memcpy(&output_hdr->timestamp, &otdb->rxbuf[6], 4);
            }
        }
        
        if (output_data != NULL) {
            memset(output_data, 0, sizeof(otdb_filedata_t));
            
            totalsize -= 10;
            if (totalsize > 0) {
                output_data->ptr    = &otdb->rxbuf[10];
                output_data->block  = block;
                output_data->fileid = file_id;
                output_data->offset = read_offset;
                output_data->length = totalsize;
            }
        }
    }
    
    return rc;
}



int otdb_restore(void* handle, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo almost identical to restore, readhdr, readperms
    
    cursor  = stpcpy(argstring, "z ");
    limit   = sizeof(argstring) - 1 - 2;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %"PRIx64" ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "%u", file_id);
    cursor += cpylen;
    //limit  -= cpylen; 

    *cursor++   = 0;
    rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    
    if (rc > 0) {
        rc = sub_get_errcode(handle);
    }
    
    return rc;
}



int otdb_readhdr(void* handle, otdb_filehdr_t* output_hdr, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo almost identical to restore, readhdr, readperms
    
    cursor  = stpcpy(argstring, "rh ");
    limit   = sizeof(argstring) - 1 - 3;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %"PRIx64" ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "%u", file_id);
    cursor += cpylen;
    //limit  -= cpylen; 

    *cursor++   = 0;
    rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    
    ///@todo read-out error code
    if (rc > 0) {
        rc = sub_get_errcode(handle);

        if (output_hdr != NULL) {
            int totalsize;
            otdb_handle_t* otdb = handle;
            
            // Received Header is 10 bytes
            totalsize   = sub_readhex(otdb->rxbuf, (char*)otdb->rxbuf, (size_t)rc);
            
            memset(output_hdr, 0, sizeof(otdb_filehdr_t));
            
            if (totalsize >= 10) {
                output_hdr->id      = otdb->rxbuf[0];
                output_hdr->perms   = otdb->rxbuf[1];
                memcpy(&output_hdr->length, &otdb->rxbuf[2], 2);
                memcpy(&output_hdr->alloc, &otdb->rxbuf[4], 2);
                memcpy(&output_hdr->timestamp, &otdb->rxbuf[6], 4);
            }
        }
    }
    
    return rc;
}



int otdb_readperms(void* handle, unsigned int* output_perms, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo almost identical to restore, readhdr, readperms
    
    cursor  = stpcpy(argstring, "rp ");
    limit   = sizeof(argstring) - 1 - 3;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %"PRIx64" ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "%u", file_id);
    cursor += cpylen;
    //limit  -= cpylen; 

    *cursor++   = 0;
    rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    
    if ((rc > 0) && (output_perms != NULL)) {
        int totalsize;
        otdb_handle_t* otdb = handle;
        
        totalsize = sub_readhex(otdb->rxbuf, (char*)otdb->rxbuf, (size_t)rc);
        if (totalsize >= 2) {
            *output_perms = otdb->rxbuf[0];
        }
    }
    
    return rc;
}



int otdb_writedata(void* handle, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id, 
        unsigned int data_offset, unsigned int data_size, uint8_t* writedata) {
    int rc;
    char* argstring;
    char* cursor;
    int cpylen;
    int limit;
    
    ///@todo consider storing this statically to minimize reallocation.
    limit       = 48 + (2 * data_size);
    argstring = calloc(limit, sizeof(char));
    if (argstring == NULL) {
        return -2;
    }
    
    cursor  = stpcpy(argstring, "w ");
    limit   = limit - 1 - 2;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %"PRIx64" ", device_id);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    if (block != BLOCK_isf) {
        cpylen  = snprintf(cursor, limit, "-b %u ", (int)block);
        cursor += cpylen;
        limit  -= cpylen; 
    }
    
    cpylen  = snprintf(cursor, limit, "-r %u:%u ", data_offset, data_offset+data_size);
    cursor += cpylen;
    limit  -= cpylen; 
    
    cpylen  = snprintf(cursor, limit, "%u ", file_id);
    cursor += cpylen;
    limit  -= cpylen; 

    *cursor++   = '[';
    cursor      = sub_printhex(cursor, writedata, data_size);
    *cursor++   = ']';

    *cursor++   = 0;
    rc          = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    
    if (rc > 0) {
        rc = sub_get_errcode(handle);
    }
    
    free(argstring);
    
    return rc;
}




int otdb_writeperms(void* handle, uint64_t device_id, otdb_fblock_enum block, unsigned int file_id, unsigned int file_perms) {
    int rc;
    char argstring[48];
    char* cursor;
    int cpylen;
    int limit;
    
    cursor  = stpcpy(argstring, "new ");
    limit   = sizeof(argstring) - 1 - 4;
    
    if (device_id != 0) {
        cpylen  = snprintf(cursor, limit, "-i %"PRIx64" ", device_id);
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
    cursor += cpylen;
    
    *cursor++ = 0;
    rc      = sub_docmd(handle, (const char*)argstring, (size_t)(cursor-argstring));
    
    if (rc > 0) {
        rc = sub_get_errcode(handle);
    }
    
    return rc;
}



