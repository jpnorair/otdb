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
#include "cmds.h"
#include "dterm.h"
#include "cliopt.h"
#include "otdb_cfg.h"
#include "debug.h"

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






int cmd_devmgr(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    struct pollfd fds[1];
    int rc = 0;
    char* curs;
    int rbytes;
    
    if (dth == NULL) {
        goto cmd_devmgr_END;
    }
    if (dth->ext->devmgr == NULL) {
        goto cmd_devmgr_END;
    }
    
    /// Purge the read pipe.  This is important to prevent any lingering data
    /// on the pipe from prepending the protocol response.
    ///@todo make sure this doesn't create dangling FILE pointers
    //FPURGE(fdopen(dth->ext->devmgr->fd_readfrom, "r"));
    
    /// In verbose mode, Print the devmgr input to stdout
    VDSRC_PRINTF("[out] %.*s\n", *inbytes, (const char*)src);
    
    fds[0].events   = (POLLIN | POLLNVAL | POLLHUP);
    fds[0].fd       = dth->ext->devmgr->fd_readfrom;
    
    write(dth->ext->devmgr->fd_writeto, src, *inbytes);
    
    /// wait one second (or configured timeout ms), and then timeout.
    rc = poll(fds, 1, cliopt_gettimeout());
    if (rc <= 0) {
        rc = -1;
        goto cmd_devmgr_END;
    }
    else if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        rc = -2;
        goto cmd_devmgr_END;
    }
    
    rc = 0;
    while (1) {
        curs    = (char*)&dst[rc];
        rbytes  = (int)read(dth->ext->devmgr->fd_readfrom, curs, dstmax-rc-1);

        if (rbytes < 0) {
            rc = -3;
            goto cmd_devmgr_END;
        }

        curs[rbytes]= 0;
        if (rbytes == 0) {
            break;
        }

        rc += rbytes;
        curs = strchr(curs, '\n');
        if (curs != NULL) {
            *curs = 0;
            rc = (int)((void*)curs - (void*)dst);
            break;
        }

        if (poll(fds, 1, 2) <= 0) {
            break;
        }
    }

    /// In verbose mode, Print the devmgr input to stdout
    VDSRC_PRINTF("[in] %.*s\n", rc, (const char*)dst);
    
    cmd_devmgr_END:
    return rc;
}








