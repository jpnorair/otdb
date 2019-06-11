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

/// There are three kinds of responses specified in DTerm2:
///
/// 1. error/ack outputs.  These have {"type":"err" ... } in JSON
///    These indicate a receipt or error of the command that was just sent.
///
/// 2. msg outputs.  These have {"type":"msg" ... } in JSON
///    These are just messages that the command may have produced.  They
///    could be sent to a console, for example.
///
/// 3. rxstat outputs.  These have {"type":"rxstat" ... } in JSON
///    These are messages that come back from the network.
///
/// MSG outputs are ignored here, but they could be forwarded to a console
/// in the future.
///
/// Error/Ack outputs will include an "sid" field in event that a message
/// was sent to the network.  If sid exists, devmgr should wait for the
/// rxstat containing a matching sid.

// Local Headers
#include "cliopt.h"
#include "cmds.h"
#include "debug.h"
#include "dterm.h"
#include "otdb_cfg.h"
#include "popen2.h"
#include "sockpush.h"

// HB Headers/Libraries
#include <bintex.h>
#include <cmdtab.h>
#include <argtable3.h>
#include <cJSON.h>
#include <otfs.h>
#include <hbdp/hb_cmdtools.h>       ///@note is this needed?

// Standard C & POSIX Libraries
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <poll.h>

#ifdef __linux__
#   include <stdio_ext.h>
#   define FPURGE   __fpurge
#else
#   define FPURGE   fpurge
#endif



// used by DB manipulation commands
extern struct arg_str*  devid_man;
extern struct arg_file* archive_man;
extern struct arg_lit*  compress_opt;
extern struct arg_lit*  jsonout_opt;

// used by file commands
extern struct arg_str*  devid_opt;
extern struct arg_str*  devidlist_opt;
extern struct arg_str*  fileblock_opt;
extern struct arg_str*  filerange_opt;
extern struct arg_int*  fileid_man;
extern struct arg_str*  fileperms_man;
extern struct arg_int*  filealloc_man;
extern struct arg_str*  filedata_man;

// used by all commands
extern struct arg_lit*  help_man;
extern struct arg_end*  end_man;





#define INPUT_SANITIZE() do { \
    if ((src == NULL) || (dst == NULL)) {   \
        *inbytes = 0;                       \
        return -1;                          \
    }                                       \
} while(0)

#if OTDB_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif


static struct timespec diff_timespec(struct timespec start, struct timespec end) {
    struct timespec result;
 
    if (end.tv_nsec < start.tv_nsec) {
        result.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
        result.tv_sec = end.tv_sec - 1 - start.tv_sec;
    }
    else {
        result.tv_nsec = end.tv_nsec - start.tv_nsec;
        result.tv_sec = end.tv_sec - start.tv_sec;
    }
 
    return result;
}


///@note DEPRECATED
static int sub_devmgr_subproc(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    struct pollfd fds[1];
    int rc = 0;
    int offset;
    char* curs;
    int rbytes;
    bool ack;
    char* scurs;
    childproc_t* subproc;
    
    subproc = dth->ext->devmgr;
    
    /// Purge the read pipe.  This is important to prevent any lingering data
    /// on the pipe from prepending the protocol response.
    ///@todo make sure this doesn't create dangling FILE pointers
    //FPURGE(fdopen(dth->ext->devmgr->fd_readfrom, "r"));
//    do {
//        rbytes = (int)read(dth->ext->devmgr->fd_readfrom, dst, dstmax);
//    } while (rbytes > 0);
    
    
    /// In verbose mode, Print the devmgr input to stdout
    VDSRC_PRINTF("[out] %.*s\n", *inbytes, (const char*)src);
    
    /// if dst == src, we need to create a temporary buffer for the src.
    if (dst == src) {
        scurs = talloc_size(dth->tctx, *inbytes);
        if (scurs == NULL) {
            return -1;
        }
    }
    else {
        scurs = (char*)src;
    }
    
    /// Purge any data that might be sitting on the pipe.  Devmgr is strictly
    /// command-response.
    fds[0].events   = (POLLIN | POLLNVAL | POLLHUP);
    fds[0].fd       = subproc->fd_readfrom;
    
    rc = poll(fds, 1, 0);
    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        rc = -2;
        goto cmd_devmgr_END;
    }
    else if (rc > 0) {
        do {
            rbytes = (int)read(subproc->fd_readfrom, dst, dstmax);
        } while (rbytes == dstmax);
    }
    
    /// Write the src packet to the pipe
    write(subproc->fd_writeto, scurs, *inbytes);
    
    /// poll & block on response for configurable timeout duration.
    rc = poll(fds, 1, cliopt_gettimeout());
    if (rc <= 0) {
        rc = -1;
        goto cmd_devmgr_END;
    }
    else if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        rc = -2;
        goto cmd_devmgr_END;
    }
    
    /// Devmgr format requires hex encoding, and the first hex byte is an
    /// ack/nack.  Make sure it is 00.
    rbytes = (int)read(subproc->fd_readfrom, dst, 2);
    if (rbytes != 2) {
        rc = -3;
        goto cmd_devmgr_END;
    }
    {   uint8_t local[4];
        cmd_hexnread(local, (const char*)dst, 2);
        ack = (local[0] == 0);
    }
    
    /// Subsequent bytes are variable length payload, encoded as hex.
    /// Even if getting a NACK, we still purge the buffer
    offset = 0;
    rc = poll(fds, 1, 0);
    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        rc = -2;
    }
    else if (rc > 0) {
        /// Even if getting a NACK, we still need to purge the buffer
        while (1) {
            curs    = (char*)&dst[offset];
            rbytes  = (int)read(subproc->fd_readfrom, curs, dstmax-offset-1);

            if (rbytes < 0) {
                rc = -3;
                goto cmd_devmgr_END;
            }

            curs[rbytes]= 0;
            if (rbytes == 0) {
                break;
            }

            offset += rbytes;
            curs = strchr(curs, '\n');
            if (curs != NULL) {
                *curs = 0;
                offset = (int)((void*)curs - (void*)dst);
                break;
            }

            if (poll(fds, 1, 2) <= 0) {
                break;
            }
        }

        /// In verbose mode, Print the devmgr input to stdout
        VDSRC_PRINTF("[in.%c] %.*s\n", ack?'v':'x', offset, (const char*)dst);
    }
    
    rc = ack ? offset : 0;
    
    cmd_devmgr_END:
    if (dst == src) {
        talloc_free(scurs);
    }
    return rc;
}




static cJSON* sub_json_gettype(cJSON* top, const char* typename) {
    top = cJSON_GetObjectItemCaseSensitive(top, "type");
    if (cJSON_IsString(top)) {
        if (strcmp(top->valuestring, typename) == 0) {
            return top;
        }
    }
    
    return NULL;
}


static int sub_json_getack(cJSON* top, uint32_t* sid) {
    uint32_t sidval = 0;
    int errval = -1;

    cJSON* obj;

    top = cJSON_GetObjectItemCaseSensitive(top, "data");
    if (cJSON_IsObject(top)) {
        obj = cJSON_GetObjectItemCaseSensitive(top, "err");
        if (cJSON_IsNumber(obj)) {
            errval = obj->valueint;
            
            if (sid != NULL) {
                obj = cJSON_GetObjectItemCaseSensitive(top, "sid");
                if (cJSON_IsNumber(obj)) {
                    sidval = obj->valueint;
                }
                *sid = sidval;
            }
        }
    }
    
    return errval;
}



static int sub_json_getframe(cJSON* top, cJSON** frame, int* qualtest) {
    int sid = -1;

    cJSON* obj;

    top = cJSON_GetObjectItemCaseSensitive(top, "data");
    if (cJSON_IsObject(top)) {
        obj = cJSON_GetObjectItemCaseSensitive(top, "sid");
        if (cJSON_IsNumber(obj)) {
            sid = obj->valueint;
            
            if (qualtest != NULL) {
                obj = cJSON_GetObjectItemCaseSensitive(top, "qual");
                if (cJSON_IsNumber(obj)) {
                    *qualtest = obj->valueint;
                }
            }
            if (frame != NULL) {
                *frame = cJSON_GetObjectItemCaseSensitive(top, "frame");
            }
        }
    }
    
    return sid;
}


static int sub_devmgr_socket(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    uint8_t dout[1024];
    
    int rc;
    int state;
    int qualtest;
    int cmd_err;
    uint32_t cmd_sid;
    struct timespec ref;
    struct timespec test;
    sp_reader_t reader;
    
    /// @todo timeout variables should be cliopt-style variables.  Currently fixed.
    cJSON* resp             = NULL;
    sp_handle_t sp_handle   = dth->ext->devmgr;
    void* ctx               = talloc_new(dth->tctx);
    int global_timeout      = 3000;
    int read_timeout        = 800;
    
    ///1. Create the synchronous reader instance for sockpush module
    reader = sp_reader_create(ctx, sp_handle);
    if (reader == NULL) {
        rc = -3;
        goto sub_devmgr_socket_END;
    }
    
    ///2. Set-up timeout reference
    ref.tv_sec   = 0;
    ref.tv_nsec  = 0;
    if (clock_gettime(CLOCK_MONOTONIC, &ref) != 0) {
        rc = -5;
        goto sub_devmgr_socket_TERM;
    }
    
    ///3. Write the output command to the socket.  This is the easy part
    sub_devmgr_socket_SENDCMD:
    DEBUG_PRINTF("Sending %i bytes to sp_sendcmd():\n%.*s\n", *inbytes, *inbytes, src);
    rc = sp_sendcmd(sp_handle, src, (size_t)*inbytes);
    if (rc < 0) {
        rc = -6;
        goto sub_devmgr_socket_TERM;
    }
    
    ///4. Wait for a message to come back on the socket.  We may need to get
    ///   more than one message.  There's a timeout enforced
    state = 0;
    cmd_sid = -1;
    while (1) {
        rc = sp_read(reader, dout, sizeof(dout), read_timeout);
        if (rc <= 0) {
            ERR_PRINTF("sp_read() timeout in cmd_devmgr(): %i ms\n", read_timeout);
            rc = -4; //-4 == retry
        }
        else {
            DEBUG_PRINTF("Read %i bytes from sp_read():\n%.*s\n", rc, rc, dout);
            rc = -1;
            qualtest = 0;
            resp = cJSON_Parse((const char*)dout);
            if (resp != NULL) {
                switch (state) {
                // State 0: looking for an ACK that has cmd string and sid int
                //{"type":"ack", "data":{"cmd":"(STRING)", "err":0, "sid":(INT)}}
                // - If err is non-zero, there was a problem with the command
                // - If sid is zero, this command doesn't have a packet, and
                //   thus the operation is complete.
                case 0:
                    if (sub_json_gettype(resp, "ack") != NULL) {
                        cmd_err = sub_json_getack(resp, &cmd_sid);
                        if (cmd_err != 0) {
                            ///@todo better error reporting
                            rc = -256 - abs(cmd_err);
                            goto sub_devmgr_socket_TERM;
                        }
                        if (cmd_sid == 0) {
                            rc = 0;
                            goto sub_devmgr_socket_TERM;
                        }
                        
                        // Success + wait for rxstat packet
                        state = 1;
                    }
                    break;
                
                // State 1: looking for an RXSTAT that has the saved sid value
                // {"type":"rxstat", "data":{"sid":(INT) ...
                // - If sid does not match saved sid, ignore
                // - If qualtest!=0, then data is corrupted: retry.
                // - If the frame is somehow invalid: retry
                // - If the frame is valid, rc set accordingly, and exit.
                case 1:
                    if (sub_json_gettype(resp, "rxstat") != NULL) {
                        cJSON* frame = NULL;
                    
                        if (cmd_sid == sub_json_getframe(resp, &frame, &qualtest)) {
                            state   = 2;
                            rc      = -4;   //-4 == retry
                            
                            if ((qualtest == 0) && (frame != NULL)) {
                                if ((cJSON_IsString(frame)) && (frame->valuestring != NULL)) {
                                    rc = (int)strlen(frame->valuestring);
                                    if (rc > (int)dstmax - 1) {
                                        ///@todo dstmax too small, flag error
                                        rc = (int)dstmax - 1;
                                    }
                                    memcpy(dst, frame->valuestring, rc+1);
                                }
                            }
                        }
                    }
                    break;
                    
                default: break;
                }
            
                // Received a message, and it is valid JSON, but it doesn't match
                // what we are looking for
                // ----------------------------------------------------------------
                ///@todo could do something here to propagate message to a console
                
                
                // ----------------------------------------------------------------
            
                cJSON_Delete(resp);
                resp = NULL;
            }
        
            // Received a message, but it's not JSON.
        }
        
        // Escape loop if the read is finished
        if (rc >= 0) {
            break;
        }
        
        // Retract the timeout
        if (clock_gettime(CLOCK_MONOTONIC, &test) != 0) {
            rc = -5;
            break;
        }
        test            = diff_timespec(ref, test);
        global_timeout -= (test.tv_sec * 1000) + (test.tv_nsec / 1000000);
        
        // Escape loop if:
        // - there's a timeout (goto termination)
        // - rc == -4, a reception error (retry the command)
        if (global_timeout <= 0) {
            goto sub_devmgr_socket_TERM;
        }
        if (rc == -4) {
//fprintf(stderr, "Retrying...\n");
            goto sub_devmgr_socket_SENDCMD;
        }
    }
    
    sub_devmgr_socket_TERM:
    cJSON_Delete(resp);
    sp_reader_destroy(reader);
    
    sub_devmgr_socket_END:
    talloc_free(ctx);
    
//fprintf(stderr, "Returning %i\n", rc);
    return rc;
}




///@note: this function got mangled by git merge
int cmd_devmgr(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int rc;
    
    if (dth == NULL) {
        return -1;
    }
    if (dth->ext->devmgr == NULL) {
        return -2;
    }
    
    if (dth->ext->use_socket) {
        rc = sub_devmgr_socket(dth, dst, inbytes, src, dstmax);
    }
    else {
        rc = sub_devmgr_subproc(dth, dst, inbytes, src, dstmax);
    }
    
    return rc;
}








