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

// Application Headers
#include "cliopt.h"
#include "clithread.h"
#include "cmds.h"
#include "cmdhistory.h"
#include "cmdsearch.h"
#include "dterm.h"
#include "debug.h"

// Local Libraries/Headers
#include <bintex.h>
#include <m2def.h>
#include <otfs.h>

// Standard C & POSIX Libraries
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>



#include <ctype.h>

// Dterm variables
static const char prompt_root[]     = PROMPT;
static const char* prompt_str[]     = {
    prompt_root
};



// switches terminal to punctual input mode
// returns 0 if success, -1 - fail
int dterm_setnoncan(dterm_t *dt);


// switches terminal to canonical input mode
// returns 0 if success, -1 - fail
int dterm_setcan(dterm_t *dt);


// reads command from stdin
// returns command type
cmdtype dterm_readcmd(dterm_t *dt);





int dterm_putlinec(dterm_t *dt, char c);


// writes size bytes to command buffer
// retunrns number of bytes written
int dterm_putcmd(dterm_t *dt, char *s, int size);


// removes count characters from linebuf
int dterm_remc(dterm_t *dt, int count);


// reads chunk of bytes from stdin
// retunrns non-negative number if success
int dterm_read(dterm_t *dt);


// clears current line, resets command buffer
// return ignored
void dterm_remln(dterm_t *dt);






// DTerm threads called in main.  
// Only one should be started.
// Piper is for usage with stdin/stdout pipes, via another process.
// Prompter is for usage with user console I/O.
void* dterm_piper(void* args);
void* dterm_prompter(void* args);
void* dterm_socketer(void* args);




/** DTerm Control Functions <BR>
  * ========================================================================<BR>
  */

int dterm_init(dterm_handle_t* dth, INTF_Type intf) {
    int rc = 0;

    if (dth == NULL) {
        return -1;
    }
    
    dth->tmpl   = NULL;
    dth->ch     = NULL;
    dth->devmgr = NULL;
    dth->ext    = NULL;

    dth->dt     = malloc(sizeof(dterm_t));
    if (dth->dt == NULL) {
        rc = -2;
        goto dterm_init_TERM;
    }
    
    dth->dt->intf = intf;
    if (intf == INTF_interactive) {
        dth->ch = ch_init();
        if (dth->ch == NULL) {
            rc = -3;
            goto dterm_init_TERM;
        }
    }
    
    dth->clithread = clithread_init();
    if (dth->clithread == NULL) {
        rc = -4;
        goto dterm_init_TERM;
    }
    
    if (pthread_mutex_init(&dth->dtwrite_mutex, NULL) != 0 ) {
        rc = -5;
        goto dterm_init_TERM;
    }
    
    return 0;
    
    dterm_init_TERM:
    clithread_deinit(dth->clithread);
    
    if (dth->dt != NULL) {
        free(dth->dt);
    }
    if (dth->ch != NULL) {
        free(dth->ch);
    }
    
    return rc;
}


void dterm_deinit(dterm_handle_t* dth) {
    if (dth->dt != NULL) {
        dterm_close(dth->dt);
        free(dth->dt);
    } 
    if (dth->ch != NULL) {
        ch_free(dth->ch);
    }
    
    // Kill devmgr process. popen2_kill_s() does a NULL check internally
    popen2_kill_s(dth->devmgr);
    
    if (dth->ext != NULL) {
        ///@note ext gets free'd externally
        dth->ext = NULL; 
    } 
    if (dth->tmpl != NULL) {
        cJSON_Delete(dth->tmpl);
    }
    
    clithread_deinit(dth->clithread);
    
    pthread_mutex_unlock(&dth->dtwrite_mutex); 
    pthread_mutex_destroy(&dth->dtwrite_mutex);
}




dterm_thread_t dterm_open(dterm_t* dt, const char* path) {
    dterm_thread_t dt_thread = NULL;
    int retcode;
    
    if (dt == NULL) {
        return NULL;
    }
    
    if (dt->intf == INTF_interactive) {
        /// Need to modify the stdout/stdin attributes in order to work with 
        /// the interactive terminal.  The "oldter" setting saves the original
        /// settings.
        dt->fd_in   = STDIN_FILENO;
        dt->fd_out  = STDOUT_FILENO;

        retcode = tcgetattr(dt->fd_in, &(dt->oldter));
        if (retcode < 0) {
            perror(NULL);
            fprintf(stderr, "Unable to access active termios settings for fd = %d\n", dt->fd_in);
            goto dterm_open_END;
        }
        
        retcode = tcgetattr(dt->fd_in, &(dt->curter));
        if (retcode < 0) {
            perror(NULL);
            fprintf(stderr, "Unable to access application termios settings for fd = %d\n", dt->fd_in);
            goto dterm_open_END;
        }
        
        dt->curter.c_lflag     &= ~(ICANON | ECHO);
        dt->curter.c_cc[VMIN]   = 1;
        dt->curter.c_cc[VTIME]  = 0;
        retcode                 = tcsetattr(dt->fd_in, TCSAFLUSH, &(dt->curter));
        
        if (retcode == 0) {
            dt_thread = &dterm_prompter;
        }
    }
    
    else if (dt->intf == INTF_pipe) {
        /// Uses canonical stdin/stdout pipes, no manipulation necessary
        dt->fd_in   = STDIN_FILENO;
        dt->fd_out  = STDOUT_FILENO;
        retcode     = 0;
        dt_thread   = &dterm_piper;
    }
    
    else if ((dt->intf == INTF_socket) && (path != NULL)) {
        /// Socket mode opens a listening socket
        ///@todo have listening queue size be dynamic
        struct sockaddr_un addr;
        
        dt->fd_in = socket(AF_UNIX, SOCK_STREAM, 0);
        if (dt->fd_in < 0) {
            perror("Unable to create a server socket\n");
            goto dterm_open_END;
        }
        VERBOSE_PRINTF("Socket created on fd=%i\n", dt->fd_in);
        
        ///@todo make sure this unlinking stage is OK.
        ///      unsure how to unbind the socket when server is finished.
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1); 
        unlink(path);
        
        VERBOSE_PRINTF("Binding...\n");
        if (bind(dt->fd_in, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("Unable to bind server socket");
            goto dterm_open_END;
        }
        VERBOSE_PRINTF("Binding Socket fd=%i to %s\n", dt->fd_in, path);
        
        if (listen(dt->fd_in, 5) == -1) {
            perror("Unable to enter listen on server socket");
            goto dterm_open_END;
        }
        VERBOSE_PRINTF("Listening on Socket fd=%i\n", dt->fd_in);
        
        retcode     = 0;
        dt_thread   = &dterm_socketer;
    }
    
    else {
        retcode = -1;
    }
    
    dterm_open_END:
    dterm_reset(dt);
    return dt_thread;
}



int dterm_close(dterm_t* dt) {
    int retcode = 0;
    
    if (dt->intf == INTF_interactive) {
        retcode = tcsetattr(dt->fd_in, TCSAFLUSH, &(dt->oldter));
    }
    
    return retcode;
}





/** DTerm Threads <BR>
  * ========================================================================<BR>
  * <LI> dterm_piper()      : For use with input pipe option </LI>
  * <LI> dterm_prompter()   : For use with console entry (default) </LI>
  *
  * Only one of the threads will run.  Piper is much simpler because it just
  * reads stdin pipe as an atomic line read.  Prompter requires character by
  * character input and analysis, and it enables shell-like features.
  */
static void sub_str_sanitize(char* str, size_t max) {
    while ((*str != 0) && (max != 0)) {
        if (*str == '\r') {
            *str = '\n';
        }
        str++;
        max--;
    }
}

static size_t sub_str_mark(char* str, size_t max) {
    char* s1 = str;
    while ((*str!=0) && (*str!='\n') && (max!=0)) {
        max--;
        str++;
    }
    if (*str=='\n') *str = 0;
    
    return (str - s1);
}



static int sub_proc_lineinput(dterm_handle_t* dth, char* loadbuf, int linelen) {
    uint8_t     protocol_buf[1024];
    char        cmdname[32];
    int         cmdlen;
    cJSON*      cmdobj;
    uint8_t*    cursor  = protocol_buf;
    int         bufmax  = sizeof(protocol_buf);
    int         bytesout = 0;
    const cmdtab_item_t* cmdptr;
    
    DEBUG_PRINTF("raw input (%i bytes) %.*s\n", linelen, linelen, loadbuf);
    
    /// The input can be JSON of the form:
    /// { "type":"${cmd_type}", data:"${cmd_data}" }
    /// where we only truly care about the data object, which must be a string.
    cmdobj  = cJSON_Parse(loadbuf);
    if (cJSON_IsObject(cmdobj)) {
        cJSON* dataobj;
        cJSON* typeobj;
        typeobj = cJSON_GetObjectItemCaseSensitive(cmdobj, "type");
        dataobj = cJSON_GetObjectItemCaseSensitive(cmdobj, "data");
        
        if (cJSON_IsString(typeobj) && cJSON_IsString(dataobj)) {
            int hdr_sz;
            VCLIENT_PRINTF("JSON Request (%i bytes): %.*s\n", linelen, linelen, loadbuf);
            loadbuf = dataobj->valuestring;
            hdr_sz  = snprintf((char*)cursor, bufmax-1, "{\"type\":\"%s\", \"data\":", typeobj->valuestring);
            cursor += hdr_sz;
            bufmax -= hdr_sz;
        }
        else {
            goto sub_proc_lineinput_FREE;
        }
    }
    
    // determine length until newline, or null.
    // then search/get command in list.
    cmdlen  = cmd_getname(cmdname, loadbuf, sizeof(cmdname));
    cmdptr  = cmd_search(dth->cmdtab, cmdname);
    
    // Test only
    //fprintf(stderr, "\nlinebuf=%s\nlinelen=%d\ncmdname=%s, len=%d, ptr=%016X\n", loadbuf, linelen, cmdname, cmdlen, cmdptr);
    //fflush(stderr);
    // Test only
    
    if (cmdptr == NULL) {
        ///@todo build a nicer way to show where the error is,
        ///      possibly by using pi or ci (sign reversing)
        if (linelen > 0) {
            //dterm_printf(dth->dt, "{\"cmd\":\"%s\", \"err\":1, \"desc\":\"command not found\"}", cmdname);
            bytesout = snprintf((char*)protocol_buf, sizeof(protocol_buf)-1, 
                        "{\"cmd\":\"%s\", \"err\":1, \"desc\":\"command not found\"}", 
                        cmdname);
            dterm_puts(dth->dt, (char*)protocol_buf);
        }
    }
    else {
        int bytesin = linelen;

        //fprintf(stderr, "bytesin=%d\nloadlen=%d\n", bytesin, (char*)loadbuf);
        //fflush(stderr);
        bytesout = cmd_run(cmdptr, dth, cursor, &bytesin, (uint8_t*)(loadbuf+cmdlen), bufmax);
        
        // Test only
        //fprintf(stderr, "\noutput\nloadbuf=%s\nloadlen=%d\n", loadbuf, loadlen);
        //fflush(stderr);
        // Test only
        
        ///@todo spruce-up the command error reporting, maybe even with
        ///      a cursor showing where the first error was found.
        if (bytesout < 0) {
            //dterm_printf(dth->dt, "{\"cmd\":\"%s\", \"err\":%d, \"desc\":\"command execution error\"}", cmdname, bytesout);
            bytesout = snprintf((char*)protocol_buf, sizeof(protocol_buf)-1, 
                        "{\"cmd\":\"%s\", \"err\":%d, \"desc\":\"command execution error\"}", 
                        cmdname, bytesout);
            dterm_puts(dth->dt, (char*)protocol_buf);
        }
        
        // If there are bytes to send to MPipe, do that.
        // If bytesout == 0, there is no error, but also nothing
        // to send to MPipe.
        else if (bytesout > 0) {
            // Test only
            //test_dumpbytes(protocol_buf, bytesout, "TX Packet Add");
            // Test only
            
            if (cJSON_IsObject(cmdobj)) {
                VCLIENT_PRINTF("JSON Response (%i bytes): %.*s\n", bytesout, bytesout, (char*)cursor);
                cursor += bytesout;
                bufmax -= bytesout;
                cursor  = (uint8_t*)stpncpy((char*)cursor, "}\f\0", bufmax);
                bytesout= (int)(cursor - protocol_buf);
            }
            
            DEBUG_PRINTF("raw output (%i bytes) %.*s\n", bytesout, bytesout, protocol_buf);
            
            write(dth->dt->fd_out, (char*)protocol_buf, bytesout);
        }
    }
    
    sub_proc_lineinput_FREE:
    cJSON_Delete(cmdobj);
    
    return bytesout;
}










void* dterm_socket_clithread(void* args) {
/// Thread that:
/// <LI> Listens to stdin via read() pipe </LI>
/// <LI> Processes each LINE and takes action accordingly. </LI>
    dterm_handle_t* dth     = ((clithread_args_t*)args)->ext;
    int             fd_in   = ((clithread_args_t*)args)->fd_in;
    int             fd_out  = ((clithread_args_t*)args)->fd_out;
    
    char databuf[1024];
    
    // Deferred cancellation: will wait until the blocking read() call is in
    // idle before killing the thread.
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    
    // Initial state = off
    dth->dt->state = prompt_off;
    
    /// Get a packet from the Socket
    while (1) {
        int linelen;
        int loadlen;
        char* loadbuf = databuf;
        
        VERBOSE_PRINTF("Waiting for read on socket fd=%i\n", dth->dt->fd_in);
        loadlen = (int)read(fd_out, loadbuf, LINESIZE);
        if (loadlen > 0) {
            sub_str_sanitize(loadbuf, (size_t)loadlen);
            
            pthread_mutex_lock(&dth->dtwrite_mutex);
            do {
                int dataout;
            
                // Burn whitespace ahead of command.
                while (isspace(*loadbuf)) { loadbuf++; loadlen--; }
                linelen = (int)sub_str_mark(loadbuf, (size_t)loadlen);
                
                // Process the line-input command
                dth->dt->fd_in  = fd_in;
                dth->dt->fd_out = fd_out;
                dataout = sub_proc_lineinput(dth, loadbuf, linelen);

                // +1 eats the terminator
                loadlen -= (linelen + 1);
                loadbuf += (linelen + 1);
            
            } while (loadlen > 0);
            pthread_mutex_unlock(&dth->dtwrite_mutex);
            
        }
        else {
            // After servicing the client socket, it is important to close it.
            close(fd_out);
            break;
        }
    }
    
    /// End of thread
    return NULL;
}



void* dterm_socketer(void* args) {
/// Thread that:
/// <LI> Listens to stdin via read() pipe </LI>
/// <LI> Processes each LINE and takes action accordingly. </LI>
    dterm_handle_t* dth = (dterm_handle_t*)args;
    clithread_args_t clithread;
    
    // Initial state = off
    dth->dt->state  = prompt_off;

    ///@todo make sure this fd_in works out
    clithread.ext   = dth;
    clithread.fd_in = dth->dt->fd_in;
    
    /// Get a packet from the Socket
    while (1) {
        VERBOSE_PRINTF("Waiting for client accept on socket fd=%i\n", dth->dt->fd_in);
        clithread.fd_out = accept(dth->dt->fd_in, NULL, NULL);
        if (clithread.fd_out < 0) {
            perror("Server Socket accept() failed");
        }
        else {
            clithread_add(dth->clithread, NULL, &dterm_socket_clithread, (void*)&clithread);
        }
    }
    
    /// This code should never occur, given the while(1) loop.
    /// If it does (possibly a stack fuck-up), we print this "chaotic error."
    fprintf(stderr, "\n--> Chaotic error: dterm_piper() thread broke loop.\n");
    raise(SIGINT);
    return NULL;
}




void* dterm_piper(void* args) {
/// Thread that:
/// <LI> Listens to stdin via read() pipe </LI>
/// <LI> Processes each LINE and takes action accordingly. </LI>
    dterm_handle_t* dth     = (dterm_handle_t*)args;
    int             loadlen = 0;
    char*           loadbuf = dth->dt->linebuf;
    
    // Initial state = off
    dth->dt->state = prompt_off;
    
    /// Get each line from the pipe.
    while (1) {
        int linelen;
        
        if (loadlen <= 0) {
            dterm_reset(dth->dt);
            loadlen = (int)read(dth->dt->fd_in, loadbuf, 1024);
            sub_str_sanitize(loadbuf, (size_t)loadlen);
        }
        
        // Burn whitespace ahead of command.
        while (isspace(*loadbuf)) { loadbuf++; loadlen--; }
        linelen = (int)sub_str_mark(loadbuf, (size_t)loadlen);

        // Process the line-input command
        sub_proc_lineinput(dth, loadbuf, linelen);
        
        // +1 eats the terminator
        loadlen -= (linelen + 1);
        loadbuf += (linelen + 1);
    }
    
    /// This code should never occur, given the while(1) loop.
    /// If it does (possibly a stack fuck-up), we print this "chaotic error."
    fprintf(stderr, "\n--> Chaotic error: dterm_piper() thread broke loop.\n");
    raise(SIGINT);
    return NULL;
}



void* dterm_prompter(void* args) {
/// Thread that:
/// <LI> Listens to dterm-input via read(). </LI>
/// <LI> Processes each keystroke and takes action accordingly. </LI>
/// <LI> Prints to the output while the prompt is active. </LI>
/// <LI> Sends signal (and the accompanied input) to dterm_parser() when a new
///          input is entered. </LI>
///
    //uint8_t protocol_buf[1024];
    
    static const cmdtype npcodes[32] = {
        ct_ignore,          // 00: NUL
        ct_ignore,          // 01: SOH
        ct_ignore,          // 02: STX
        ct_sigint,          // 03: ETX (Ctl+C)
        ct_ignore,          // 04: EOT
        ct_ignore,          // 05: ENQ
        ct_ignore,          // 06: ACK
        ct_ignore,          // 07: BEL
        ct_ignore,          // 08: BS (backspace)
        ct_autofill,        // 09: TAB
        ct_enter,           // 10: LF
        ct_ignore,          // 11: VT
        ct_ignore,          // 12: FF
        ct_ignore,          // 13: CR
        ct_ignore,          // 14: SO
        ct_ignore,          // 15: SI
        ct_ignore,          // 16: DLE
        ct_ignore,          // 17: DC1
        ct_ignore,          // 18: DC2
        ct_ignore,          // 19: DC3
        ct_ignore,          // 20: DC4
        ct_ignore,          // 21: NAK
        ct_ignore,          // 22: SYN
        ct_ignore,          // 23: ETB
        ct_ignore,          // 24: CAN
        ct_ignore,          // 25: EM
        ct_ignore,          // 26: SUB
        ct_prompt,          // 27: ESC (used to invoke prompt, ignored while prompt is up)
        ct_sigquit,         // 28: FS (Ctl+\)
        ct_ignore,          // 29: GS
        ct_ignore,          // 30: RS
        ct_ignore,          // 31: US
    };
    
    cmdtype             cmd;
    char                cmdname[32];
    cmdhist*            ch;
    char                c           = 0;
    ssize_t             keychars    = 0;
    dterm_handle_t*     dth         = (dterm_handle_t*)args;
    pthread_mutex_t*    write_mutex = &((dterm_handle_t*)args)->dtwrite_mutex;
    
    // Initialize command history
    ((dterm_handle_t*)args)->ch = ch_init();
    if (((dterm_handle_t*)args)->ch == NULL) {
        goto dterm_prompter_TERM;
    }
    
    // Initialize
    
    // Local pointer for command history is just for making code look nicer
    ch = ((dterm_handle_t*)args)->ch;
    
    // Initial state = off
    dth->dt->state = prompt_off;
    
    /// Get each keystroke.
    /// A keystoke is reported either as a single character or as three.
    /// triple-char keystrokes are for special keys like arrows and control
    /// sequences.
    ///@note dterm_read() will keep the thread asleep, blocking it until data arrives
    while ((keychars = read(dth->dt->fd_in, dth->dt->readbuf, READSIZE)) > 0) {
        
        // Default: IGNORE
        cmd = ct_ignore;
        
        // If dterm state is off, ignore anything except ESCAPE
        ///@todo mutex unlocking on dt->state
        
        if ((dth->dt->state == prompt_off) && (keychars == 1) && (dth->dt->readbuf[0] <= 0x1f)) {
            cmd = npcodes[dth->dt->readbuf[0]];
            
            // Only valid commands when prompt is OFF are prompt, sigint, sigquit
            // Using prompt (ESC) will open a prompt and ignore the escape
            // Using sigquit (Ctl+\) or sigint (Ctl+C) will kill the program
            // Using any other key will be ignored
            if ((cmd != ct_prompt) && (cmd != ct_sigquit) && (cmd != ct_sigint)) {
                continue;
            }
        }
        
        else if (dth->dt->state == prompt_on) {
            if (keychars == 1) {
                c = dth->dt->readbuf[0];   
                if (c <= 0x1F)              cmd = npcodes[c];   // Non-printable characters except DELETE
                else if (c == ASCII_DEL)    cmd = ct_delete;    // Delete (0x7F)
                else                        cmd = ct_key;       // Printable characters
            }
            
            else if (keychars == 3) {
                if ((dth->dt->readbuf[0] == VT100_UPARR[0]) && (dth->dt->readbuf[1] == VT100_UPARR[1])) {
                    if (dth->dt->readbuf[2] == VT100_UPARR[2]) {
                        cmd = ct_histnext;
                    }
                    else if (dth->dt->readbuf[2] == VT100_DWARR[2]) {
                        cmd = ct_histprev;
                    }
                }
            }
        }
        
        // Ignore the keystroke, the prompt is off and/or it is an invalid key
        else {
            continue;
        }
        
        // This mutex protects the terminal output from being written-to by
        // this thread and mpipe_parser() at the same time.
        if (dth->dt->state == prompt_off) {
            pthread_mutex_lock(write_mutex);
        }
        
        // These are error conditions
        if ((int)cmd < 0) {
            int sigcode;
            const char* killstring;
            static const char str_ct_error[]    = "--> terminal read error, sending SIGQUIT\n";
            static const char str_ct_sigint[]   = "^C\n";
            static const char str_ct_sigquit[]  = "^\\\n";
            static const char str_unknown[]     = "--> unknown error, sending SIGQUIT\n";
            
            switch (cmd) {
                case ct_error:      killstring  = str_ct_error;
                                    sigcode     = SIGQUIT;
                                    break;
                                    
                case ct_sigint:     killstring  = str_ct_sigint;
                                    sigcode     = SIGINT; 
                                    break;
                                    
                case ct_sigquit:    killstring  = str_ct_sigquit;
                                    sigcode     = SIGQUIT;
                                    break;
                                    
                default:            killstring  = str_unknown;
                                    sigcode     = SIGQUIT; 
                                    break;
            }
            
            dterm_reset(dth->dt);
            dterm_puts(dth->dt, (char*)killstring);
            raise(sigcode);
            return NULL;
        }
        
        // These are commands that cause input into the prompt.
        // Note that the mutex is only released after ENTER is used, which has
        // the effect of blocking printout of received messages while the 
        // prompt is up
        else {
            int cmdlen;
            char* cmdstr;
            const cmdtab_item_t* cmdptr;
            //cmdaction_t cmdfn;
            
            switch (cmd) {
                // A printable key is used
                case ct_key: {       
                    dterm_putcmd(dth->dt, &c, 1);
                    //dterm_put(dt, &c, 1);
                    dterm_putc(dth->dt, c);
                } break;
                                    
                // Prompt-Escape is pressed, 
                case ct_prompt: {    
                    if (dth->dt->state == prompt_on) {
                        dterm_remln(dth->dt);
                        dth->dt->state = prompt_off;
                    }
                    else {
                        dterm_puts(dth->dt, (char*)prompt_str[0]);
                        dth->dt->state = prompt_on;
                    }
                } break;
            
                // EOF currently has the same effect as ENTER/RETURN
                case ct_eof:        
                
                // Enter/Return is pressed
                // 1. Echo Newline (NOTE: not sure why 2 chars here)
                // 2. Add line-entry into the  history
                // 3. Search and try to execute cmd
                // 4. Reset prompt, change to OFF State, unlock mutex on dterm
                case ct_enter: {
                    int bytesout;
                    
                    //dterm_put(dt, (char[]){ASCII_NEWLN}, 2);
                    dterm_putc(dth->dt, '\n');
                    
                    if (!ch_contains(ch, dth->dt->linebuf)) {
                        ch_add(ch, dth->dt->linebuf);
                    }
                    
                    bytesout = sub_proc_lineinput( dth, 
                                        (char*)dth->dt->linebuf, 
                                        (int)sub_str_mark((char*)dth->dt->linebuf, 1024)
                                    );
                                    
                    // If there's meaningful output, add a linebreak
                    if (bytesout > 0) {
                        dterm_puts(dth->dt, "\n");
                    }

                    dterm_reset(dth->dt);
                    dth->dt->state = prompt_close;
                } break;
                
                // TAB presses cause the autofill operation (a common feature)
                // autofill will try to finish the command input
                case ct_autofill: {
                    cmdlen = cmd_getname((char*)cmdname, dth->dt->linebuf, sizeof(cmdname));
                    cmdptr = cmd_subsearch(dth->cmdtab, (char*)cmdname);
                    if ((cmdptr != NULL) && (dth->dt->linebuf[cmdlen] == 0)) {
                        dterm_remln(dth->dt);
                        dterm_puts(dth->dt, (char*)prompt_str[0]);
                        dterm_putsc(dth->dt, (char*)cmdptr->name);
                        dterm_puts(dth->dt, (char*)cmdptr->name);
                    }
                    else {
                        dterm_puts(dth->dt, ASCII_BEL);
                    }
                } break;
                
                // DOWN-ARROW presses fill the prompt with the next command 
                // entry in the command history
                case ct_histnext: {
                    //cmdstr = ch_next(ch);
                    cmdstr = ch_prev(ch);
                    if (ch->count && cmdstr) {
                        dterm_remln(dth->dt);
                        dterm_puts(dth->dt, (char*)prompt_str[0]);
                        dterm_putsc(dth->dt, cmdstr);
                        dterm_puts(dth->dt, cmdstr);
                    }
                } break;
                
                // UP-ARROW presses fill the prompt with the last command
                // entry in the command history
                case ct_histprev: {
                    //cmdstr = ch_prev(ch);
                    cmdstr = ch_next(ch);
                    if (ch->count && cmdstr) {
                        dterm_remln(dth->dt);
                        dterm_puts(dth->dt, (char*)prompt_str[0]);
                        dterm_putsc(dth->dt, cmdstr);
                        dterm_puts(dth->dt, cmdstr);
                    }
                } break;
                
                // DELETE presses issue a forward-DELETE
                case ct_delete: { 
                    if (dth->dt->linelen > 0) {
                        dterm_remc(dth->dt, 1);
                        dterm_put(dth->dt, VT100_CLEAR_CH, 4);
                    }
                } break;
                
                // Every other command is ignored here.
                default: {
                    dth->dt->state = prompt_close;
                } break;
            }
        }
        
        // Unlock Mutex
        if (dth->dt->state != prompt_on) {
            dth->dt->state = prompt_off;
            pthread_mutex_unlock(write_mutex);
        }
        
    }
    
    dterm_prompter_TERM:
    
    /// This code should never occur, given the while(1) loop.
    /// If it does (possibly a stack fuck-up), we print this "chaotic error."
    fprintf(stderr, "\n--> Chaotic error: dterm_prompter() thread broke loop.\n");
    raise(SIGINT);
    return NULL;
}










/** Subroutines for reading & writing
  * ========================================================================<BR>
  */


int dterm_read(dterm_t *dt) {
    return (int)read(dt->fd_in, dt->readbuf, READSIZE);
}



int dterm_scanf(dterm_t* dt, const char* format, ...) {
    int retval;
    size_t keychars;
    va_list args;
    
    /// Clear linebuf without actually clearing the line, which is probably a
    /// prompt or similar.  linebuf will hold the rest of the line only
    dterm_reset(dt);
    
    /// Read in a line.  All non-printable characters are ignored.
    while ((keychars = read(dt->fd_in, dt->readbuf, READSIZE)) > 0) {
        if (keychars == 1) {
            if (dt->readbuf[0] == ASCII_NEWLN) {
                dterm_putc(dt, ASCII_NEWLN);
                dterm_putlinec(dt, 0);
            }
            else if ((dt->linelen < LINESIZE) \
            && (dt->readbuf[0] > 0x1f) \
            && (dt->readbuf[0] < 0x7f)) {
                dterm_putc(dt, dt->readbuf[0]);
                dterm_putlinec(dt, dt->readbuf[0]);
            }
        }
    }
    
    /// Run the line through scanf, wrapping the variadic args
    va_start(args, format);
    retval = vsscanf(dt->linebuf, format, args);
    va_end(args);
    
    return retval;
}




int dterm_printf(dterm_t* dt, const char* format, ...) {
    FILE* fp;
    int retval;
    va_list args;
    
    fp = fdopen(dt->fd_out, "w");   //don't close this!  Merely fd --> fp conversion
    if (fp == NULL) {
        return -1;
    }
    
    va_start(args, format);
    retval = vfprintf(fp, format, args);
    va_end(args);
    
    return retval;
}




int dterm_put(dterm_t *dt, char *s, int size) {
    return (int)write(dt->fd_out, s, size);    
}

int dterm_puts(dterm_t *dt, char *s) {
    char* end = s-1;
    while (*(++end) != 0);
        
    return (int)write(dt->fd_out, s, end-s);
}

int dterm_putc(dterm_t *dt, char c) {        
    return (int)write(dt->fd_out, &c, 1);
}

int dterm_puts2(dterm_t *dt, char *s) {
    return (int)write(dt->fd_out, s, strlen(s));
}

int dterm_putsc(dterm_t *dt, char *s) {
    uint8_t* end = (uint8_t*)s - 1;
    while (*(++end) != 0);
    
    return dterm_putcmd(dt, s, end - (uint8_t*)s);
}



int dterm_putlinec(dterm_t *dt, char c) {
    int line_delta = 0;
    
    if (c == ASCII_BACKSPC) {
        line_delta = -1;
    }
    
    else if (c == ASCII_DEL) {
        size_t line_remnant;
        line_remnant = dt->linelen - 1 - (dt->cline - dt->linebuf);
        
        if (line_remnant > 0) {
            memcpy(dt->cline, dt->cline+1, line_remnant);
            line_delta = -1;
        }
    }
    
    else if (dt->linelen > (LINESIZE-1) ) {
        return 0;
    }
    
    else {
        *dt->cline++    = c;
        line_delta      = 1;
    }
    
    dt->linelen += line_delta;
    return line_delta;
}



int dterm_putcmd(dterm_t *dt, char *s, int size) {
    int i;
    
    if ((dt->linelen + size) > LINESIZE) {
        return 0;
    }
        
    dt->linelen += size;
    
    for (i=0; i<size; i++) {
        *dt->cline++ = *s++;
    }
        
    return size;
}




int dterm_remc(dterm_t *dt, int count) {
    int cl = dt->linelen;
    while (count-- > 0) {
        *dt->cline-- = 0;
        dt->linelen--;
    }
    return cl - dt->linelen;
}



void dterm_remln(dterm_t *dt) {
    dterm_put(dt, VT100_CLEAR_LN, 5);
    dterm_reset(dt);
}



void dterm_reset(dterm_t *dt) {
    dt->cline = dt->linebuf;
    
    while (dt->cline < (dt->linebuf + LINESIZE)) {
        *dt->cline++ = 0;  
    }
    
    dt->cline    = dt->linebuf;
    dt->linelen  = 0;
}



