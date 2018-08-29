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

// HB Headers/Libraries
#include <bintex.h>
#include <cmdtab.h>

// Standard C & POSIX Libraries
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>



/// Variables used across shell commands
#define HOME_PATH_MAX           256
char home_path[HOME_PATH_MAX]   = "~/";
int home_path_len               = 2;





uint8_t* sub_markstring(uint8_t** psrc, int* search_limit, int string_limit) {
    size_t      code_len;
    size_t      code_max;
    uint8_t*    cursor;
    uint8_t*    front;
    
    /// 1. Set search limit on the string to mark within the source string
    code_max    = (*search_limit < string_limit) ? *search_limit : string_limit; 
    front       = *psrc;
    
    /// 2. Go past whitespace in the front of the source string if there is any.
    ///    This updates the position of the source string itself, so the caller
    ///    must save the position of the source string if it wishes to go back.
    while (isspace(**psrc)) { 
        (*psrc)++; 
    }
    
    /// 3. Put a Null Terminator where whitespace is found after the marked
    ///    string.
    for (code_len=0, cursor=*psrc; (code_len < code_max); code_len++, cursor++) {
        if (isspace(*cursor)) {
            *cursor = 0;
            cursor++;
            break;
        }
    }
    
    /// 4. Go past any whitespace after the cursor position, and update cursor.
    while (isspace(*cursor)) { 
        cursor++; 
    }
    
    /// 5. reduce the message limit counter given the bytes we've gone past.
    *search_limit -= (cursor - front);
    
    return cursor;
}



uint8_t* goto_eol(uint8_t* src) {
    uint8_t* end = src;
    
    while ((*end != 0) && (*end != '\n')) {
        end++;
    }
    
    return end;
}


#define INPUT_SANITIZE() do { \
    if ((src == NULL) || (dst == NULL)) {   \
        *inbytes = 0;                                       \
        return -1;                                          \
    }                                                       \
    {   uint8_t* eol    = goto_eol(src);                    \
        *inbytes        = (int)(eol-src);                   \
        *eol   = 0;                                         \
    }                                                       \
} while(0)

#define INPUT_SANITIZE_FLAG_EOS(IS_EOS) do { \
    if ((src == NULL) || (dst == NULL)) {   \
        *inbytes = 0;                                       \
        return -1;                                          \
    }                                                       \
    {   uint8_t* eol    = goto_eol(src);                    \
        *inbytes        = (int)(eol-src);                   \
        IS_EOS          = (bool)(*eol == 0);                \
        *eol   = 0;                                         \
    }                                                       \
} while(0)







int cmd_quit(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    
    /// dt == NULL is the initialization case.
    /// There may not be an initialization for all command groups.
    if (dt == NULL) {
        return 0;
    }
    
    INPUT_SANITIZE();
    
    raise(SIGINT);
    return 0;
}


extern cmdtab_t* otdb_cmdtab;
int cmd_cmdlist(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int bytes_out;
    char cmdprint[1024];

    /// dt == NULL is the initialization case.
    /// There may not be an initialization for all command groups.
    if (dt == NULL) {
        return 0;
    }
    
    INPUT_SANITIZE();
    
    bytes_out = cmdtab_list(otdb_cmdtab, cmdprint, 1024);
    dterm_puts(dt, "Commands available:\n");
    dterm_puts(dt, cmdprint);
    
    return 0;
}


int cmd_del(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}

int cmd_new(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}

int cmd_read(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}

int cmd_readall(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}

int cmd_restore(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}

int cmd_readhdr(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}

int cmd_readperms(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}

int cmd_write(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}

int cmd_writeperms(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}




int cmd_open(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}

int cmd_save(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}



int cmd_setid(dterm_t* dt, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    return 0;
}






