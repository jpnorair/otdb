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

// Cmdtab library header
#include <cmdtab.h>

// Local Headers
#include "cmds.h"
#include "cmdsearch.h"

#include <bintex.h>

#include <string.h>
#include <stdio.h>
#include <ctype.h>





/// Binary Search Table for Commands

// sorted list of supported commands

typedef struct {
    const char      name[8]; 
    cmdaction_t     action; 
} cmd_t;

static const cmd_t otdb_commands[] = {
    { "cmdls",      &cmd_cmdlist },
    { "del",        &cmd_del },
    { "dev-del",    &cmd_devdel },
    { "dev-new",    &cmd_devnew },
    { "dev-set",    &cmd_devset },
    { "open",       &cmd_open },
    { "new",        &cmd_new },
    { "quit",       &cmd_quit },
    { "r",          &cmd_read },
    { "r*",         &cmd_readall },
    { "restore",    &cmd_restore },
    { "rh",         &cmd_readhdr },
    { "rp",         &cmd_readperms },
    { "w",          &cmd_write },
    { "wp",         &cmd_writeperms },
    { "save",       &cmd_save },
    { "z",          &cmd_restore },
};



///@todo Make this thread safe by adding a mutex here.
///      It's not technically required yet becaus only one thread in otdb uses
///      cmdsearch, but we should put it in soon, just in case.
static cmdtab_t cmdtab_default = {
    .cmd    = NULL,
    .size   = 0,
    .alloc  = 0
};

cmdtab_t* otdb_cmdtab;


typedef enum {
    EXTCMD_null = 0,
    EXTCMD_path,
    EXTCMD_MAX
} otdb_extcmd_t;



/** Argtable objects
  * -------------------------------------------------------------------------
  */




int cmd_init(cmdtab_t* init_table, const char* xpath) {
  
    otdb_cmdtab = (init_table == NULL) ? &cmdtab_default : init_table;

    /// cmdtab_add prioritizes subsequent command adds, so the highest priority
    /// commands should be added last.
    
    /// First, add commands that are available from the external command path.
    if (xpath != NULL) {
        size_t xpath_len = strlen(xpath);
        char buffer[256];
        char* cmd;
        FILE* stream;
        int test;
        
        if (xpath_len > 0) { 
            ///@todo make this find call work properly on mac and linux.
#           if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
            snprintf(buffer, 256, "find %s -perm +111 -type f", xpath);
            stream = popen(buffer, "r");
#           elif defined(__linux__)
            snprintf(buffer, 256, "find %s -perm /u=x,g=x,o=x -type f", xpath);
            stream = popen(buffer, "r");
#           else
            stream = NULL;
#           endif
            
            if (stream != NULL) {
                do {
                    test = fscanf(stream, "%s", buffer);
                    if (test == 1) {
                        cmd = &buffer[xpath_len];
                        if (strlen(cmd) >= 2) {
                            cmdtab_add(otdb_cmdtab, buffer, (void*)xpath, (void*)EXTCMD_path);
                        }
                    }
                } while (test != EOF);
                
                pclose(stream);
            }
        }
    }

    /// Add Otter commands to the cmdtab.
    for (int i=0; i<(sizeof(otdb_commands)/sizeof(cmd_t)); i++) {
        int rc;
 
        rc = cmdtab_add(otdb_cmdtab, otdb_commands[i].name, (void*)otdb_commands[i].action, (void*)EXTCMD_null);
        if (rc != 0) {
            fprintf(stderr, "ERROR: cmdtab_add() from %s line %d returned %d.\n", __FUNCTION__, __LINE__, rc);
            return -1;
        }
        
        ///@note No commands currently require initialization
        //otdb_commands[i].action(NULL, NULL, NULL, NULL, 0);
    }
  
    /// Initialize the arguments used by commands
    cmd_init_args();

    return 0;
}



int cmd_run(const cmdtab_item_t* cmd, dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    int output;

    if (cmd == NULL) {
        return -1;
    }
    
    // handling of different command types
    switch ((otdb_extcmd_t)cmd->extcmd) {
        case EXTCMD_null:
            //fprintf(stderr, "EXTCMD_null: inbytes=%d, src=%s\n", *inbytes, (char*)src);
            output = ((cmdaction_t)cmd->action)(dth, dst, inbytes, src, dstmax);
            break;

        case EXTCMD_path: {
            char xpath[512];
            char* cursor;
            FILE* stream;
            int rsize;
            //fprintf(stderr, "EXTCMD_path: inbytes=%d, src=%s\n", *inbytes, (char*)src);
            
            cursor          = xpath;
            cursor          = stpncpy(cursor, (const char*)(cmd->action), (&xpath[512] - cursor));
            cursor          = stpncpy(cursor, (const char*)(cmd->name), (&xpath[512] - cursor));
            cursor          = stpncpy(cursor, " ", (&xpath[512] - cursor));
            rsize           = (int)(&xpath[512] - cursor);
            rsize           = (*inbytes < rsize) ? *inbytes : (rsize-1);
            cursor[rsize]   = 0;
            memcpy(cursor, src, rsize);

            stream = popen(xpath, "r");
            if (stream == NULL) {
                output = -1;    ///@todo make sure this is correct error code
            }
            else {
                output = bintex_fs(stream, dst, (int)dstmax);
                pclose(stream);
            }
        } break;

        default:
            //fprintf(stderr, "No Command Extension found: inbytes=%d, src=%s\n", *inbytes, (char*)src);
            output = -2;
            break;
    }

    return output;
}



int cmd_getname(char* cmdname, const char* cmdline, size_t max_cmdname) {
	size_t diff = max_cmdname;
    
    // Copy command into cmdname, stopping when whitespace is detected, or
    // the command line (string) is ended.
    while ((diff != 0) && (*cmdline != 0) && !isspace(*cmdline)) {
        diff--;
        *cmdname++ = *cmdline++;
    }
    
    // Add command string terminator & determine command string length (diff)
    *cmdname    = 0;
    diff        = max_cmdname - diff;
    return (int)diff;    
}




const cmdtab_item_t* cmd_search(char *cmdname) {
    return cmdtab_search(otdb_cmdtab, cmdname);
}



const cmdtab_item_t* cmd_subsearch(char *namepart) {
    return cmdtab_subsearch(otdb_cmdtab, namepart);
}





