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

#ifndef dterm_h
#define dterm_h

// Configuration Header
#include "otdb_cfg.h"
#include "cliopt.h"
#include "clithread.h"
#include "cmdhistory.h"
#include "popen2.h"

// HB Libraries
#include <cJSON.h>
#include <cmdtab.h>

// Standard C & POSIX Libraries
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <termios.h>
#include <unistd.h>

//#include <stdlib.h>



#define LINESIZE            1024
#define CMDSIZE             LINESIZE        //CMDSIZE is deprecated
#define READSIZE            3

#define APP_NAME            (OTDB_PARAM(NAME) " " OTDB_PARAM(VERSION))
#define PROMPT_ROOT         OTDB_PARAM(NAME)"# "
#define PROMPT              PROMPT_ROOT
#define INV                 PROMPT

#define ASCII_TAB           '\t'
#define ASCII_NEWLN         '\n'
#define ASCII_BACKSPC       '\b'
#define ASCII_DEL           '\x7F'
#define ASCII_BEL           "\a"
#define ASCII_CTLC          '\x03'
#define ASCII_ESC           '\x1B'
#define ASCII_CTLBSLASH     '\x1C'

// for reading from stdout
#define VT100_UPARR         "\x1B[A"
#define VT100_DWARR         "\x1B[B"
#define VT100_RTARR         "\x1B[C"
#define VT100_LFARR         "\x1B[D"

// for writing to stdin
#define VT100_CLEAR_CH      "\b\033[K"
#define VT100_CLEAR_LN      "\033[2K\r"



// describes dterm possible states
typedef enum {
    prompt_off   = 0,
    prompt_on    = 1,
    prompt_close = 2
} prompt_state;


// defines state of dash terminal
typedef struct {
    // old and current terminal settings
    INTF_Type intf;
    
    struct termios oldter;
    struct termios curter;
    
    volatile prompt_state state; // state of the terminal prompt

    int linelen;                 // line length
    char *cline;                 // pointer to current position in linebuf
    char linebuf[LINESIZE];      // command read buffer
    char readbuf[READSIZE];     // character read buffer
} dterm_t;


typedef struct {
    // fd_in, fd_out are used by controlling interface.
    // Usage will differ in case of interactive, pipe, socket
    int in;
    int out;
} dterm_fd_t;


typedef struct {
    // Internally initialized in dterm_init()
    // Used only by dterm controlling thread.
    dterm_t*            dt;
    cmdhist*            ch;
    clithread_handle_t  clithread;
    
    // Internally initialized in dterm_init()
    // Used by client threads to prevent more than one command from running in
    // OTDB engine at any given time.
    pthread_mutex_t*    dtwrite_mutex;
    
    // Client Thread I/O parameters.
    // Should be altered per client thread in cloned dterm_handle_t
    dterm_fd_t          fd;
    
    // Externally initialized in otdb_main()
    // Safe for client threads to duplicate/reference because of mutex protection
    cmdtab_t*           cmdtab;
    childproc_t*        devmgr;
    void*               ext;
    cJSON*              tmpl;
} dterm_handle_t;


typedef void* (*dterm_thread_t)(void*);


// describes supported command types
typedef enum {
    ct_sigint       = -3,   // Control-C
    ct_sigquit      = -2,   // Control-\ (backslash)
    ct_error        = -1,   // error reading stdin
    ct_key          = 0,    // stdin eof
    ct_prompt       = 1,
    ct_eof          = 2,
    ct_enter        = 3,    // prompt entered
    ct_autofill     = 4,    // autocomplete query by tab key
    ct_histnext     = 5,    // get next command from history
    ct_histprev     = 6,    // get previous command from history
    ct_delete       = 7,
    ct_ignore       = 8
} cmdtype;




int dterm_init(dterm_handle_t* dth, INTF_Type intf);
void dterm_deinit(dterm_handle_t* dth);


dterm_thread_t dterm_open(dterm_handle_t* dth, const char* path);
int dterm_close(dterm_handle_t* dth);



///@todo refactor these read/write functions

int dterm_put(dterm_fd_t* fd, char *s, int size);
int dterm_puts(dterm_fd_t* fd, char *s);
int dterm_putc(dterm_fd_t* fd, char c);
int dterm_puts2(dterm_fd_t* fd, char *s);

int dterm_putsc(dterm_t *dt, char *s);
int dterm_putcmd(dterm_t *dt, char *s, int size);


// resets command buffer
void dterm_reset(dterm_t *dt);




#endif
