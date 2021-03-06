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
#include "cmds.h"
#include "cmdhistory.h"
#include "cmdsearch.h"
#include "dterm.h"
#include "debug.h"

// Local Libraries/Headers
#include <bintex.h>
#include <clithread.h>
//#include <m2def.h>
//#include <otfs.h>

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
#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <ctype.h>


#if 0 //OTDB_FEATURE_DEBUG
#   define PRINTLINE()     fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#   define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#   define PRINTLINE()     do { } while(0)
#   define DEBUGPRINT(...) do { } while(0)
#endif



// Dterm variables
static const char prompt_root[]     = _E_MAG PROMPT _E_NRM;
static const char* prompt_str[]     = {
    prompt_root
};



// writes size bytes to command buffer
// retunrns number of bytes written
int dterm_putcmd(dterm_intf_t *dt, char *s, int size);


// removes count characters from linebuf
int dterm_remc(dterm_intf_t *dt, int count);


// clears current line, resets command buffer
// return ignored
void dterm_remln(dterm_intf_t *dt, dterm_fd_t* fd);


int dterm_put(dterm_fd_t* fd, char *s, int size);
int dterm_puts(dterm_fd_t* fd, char *s);
int dterm_putc(dterm_fd_t* fd, char c);
int dterm_putsc(dterm_intf_t *dt, char *s);
void dterm_reset(dterm_intf_t *dt);



// DTerm threads called in main.  
// Only one should be started.
// Piper is for usage with stdin/stdout pipes, via another process.
// Prompter is for usage with user console I/O.
void* dterm_piper(void* args);
void* dterm_prompter(void* args);
void* dterm_socketer(void* args);






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


static void iso_free(void* ctx) {
    talloc_free(ctx);
}

static TALLOC_CTX* iso_ctx;
static void* iso_malloc(size_t size) {
    return talloc_size(iso_ctx, size);
}

static void cjson_iso_allocators(void) {
    cJSON_Hooks hooks;
    hooks.free_fn   = &iso_free;
    hooks.malloc_fn = &iso_malloc;
    cJSON_InitHooks(&hooks);
}

static void cjson_std_allocators(void) {
    cJSON_InitHooks(NULL);
}







/** DTerm Control Functions <BR>
  * ========================================================================<BR>
  */

int dterm_init(dterm_handle_t* dth, dterm_ext_t* ext_data, INTF_Type intf) {
    int rc = 0;

    ///@todo ext data should be handled as its own module, but we can accept
    /// that it must be non-null.
    if ((dth == NULL) || (ext_data == NULL)) {
        return -1;
    }
    
    dth->ext    = ext_data;
    dth->ch     = NULL;
    dth->intf   = malloc(sizeof(dterm_intf_t));
    if (dth->intf == NULL) {
        rc = -2;
        goto dterm_init_TERM;
    }
    
    dth->intf->type = intf;
    if (intf == INTF_interactive) {
        dth->ch = ch_init();
        if (dth->ch == NULL) {
            rc = -3;
            goto dterm_init_TERM;
        }
    }
    
    if (clithread_init(&dth->clithread) != 0) {
        rc = -4;
        goto dterm_init_TERM;
    }
    
    talloc_disable_null_tracking();
    dth->pctx = talloc_new(NULL);
    dth->tctx = dth->pctx;
    if (dth->pctx == NULL) {
        rc = -5;
        goto dterm_init_TERM;
    }
    
    dth->iso_mutex = malloc(sizeof(pthread_mutex_t));
    if (dth->iso_mutex == NULL) {
        rc = -6;
        goto dterm_init_TERM;
    }
    
    if (pthread_mutex_init(dth->iso_mutex, NULL) != 0 ) {
        rc = -7;
        goto dterm_init_TERM;
    }
    
    /// If sockets are being used, SIGPIPE can cause trouble that we don't
    /// want, and it is safe to ignore.
    if (dth->intf->type == INTF_socket) {
        signal(SIGPIPE, SIG_IGN);
    }
    
    return 0;
    
    dterm_init_TERM:
    clithread_deinit(dth->clithread);
    
    if (dth->intf != NULL) {
        free(dth->intf);
    }
    if (dth->ch != NULL) {
        free(dth->ch);
    }
    
    return rc;
}


void dterm_deinit(dterm_handle_t* dth) {
    if (dth->intf != NULL) {
        dterm_close(dth);
        free(dth->intf);
    } 
    if (dth->ch != NULL) {
        ch_free(dth->ch);
    }
    
    // Kill devmgr process. popen2_kill() does a NULL check internally
    ///@todo the ext data should be handled as its own module
    if (dth->ext != NULL) {
        popen2_kill(dth->ext->devmgr);
        
        if (dth->ext->db != NULL) {
            ///@note ext gets free'd externally
            //dth->ext->db = NULL;
        }

        // Cannot use cJSON_Delete() here because the tmpl is stored as a
        // contiguous block.
        talloc_free(dth->ext->tmpl);
    }
    // -----------------------------------------------------------------------

    clithread_deinit(dth->clithread);
    
    if (dth->iso_mutex != NULL) {
        pthread_mutex_unlock(dth->iso_mutex);
        pthread_mutex_destroy(dth->iso_mutex);
        free(dth->iso_mutex);
    }
}




dterm_thread_t dterm_open(dterm_handle_t* dth, const char* path) {
    dterm_thread_t dt_thread = NULL;
    int retcode;
    
    if (dth == NULL)        return NULL;
    if (dth->intf == NULL)  return NULL;
    
    if (dth->intf->type == INTF_interactive) {
        /// Need to modify the stdout/stdin attributes in order to work with 
        /// the interactive terminal.  The "oldter" setting saves the original
        /// settings.
        dth->fd.in  = STDIN_FILENO;
        dth->fd.out = STDOUT_FILENO;

        retcode = tcgetattr(dth->fd.in, &(dth->intf->oldter));
        if (retcode < 0) {
            perror(NULL);
            fprintf(stderr, "Unable to access active termios settings for fd = %d\n", dth->fd.in);
            goto dterm_open_END;
        }
        
        retcode = tcgetattr(dth->fd.in, &(dth->intf->curter));
        if (retcode < 0) {
            perror(NULL);
            fprintf(stderr, "Unable to access application termios settings for fd = %d\n", dth->fd.in);
            goto dterm_open_END;
        }
        
        dth->intf->curter.c_lflag      &= ~(ICANON | ECHO);
        dth->intf->curter.c_cc[VMIN]    = 1;
        dth->intf->curter.c_cc[VTIME]   = 0;
        retcode                         = tcsetattr(dth->fd.in, TCSAFLUSH, &(dth->intf->curter));
        if (retcode == 0) {
            dt_thread = &dterm_prompter;
        }
    }
    
    else if (dth->intf->type == INTF_pipe) {
        /// Uses canonical stdin/stdout pipes, no manipulation necessary
        dth->fd.in  = STDIN_FILENO;
        dth->fd.out = STDOUT_FILENO;
        retcode     = 0;
        dt_thread   = &dterm_piper;
    }
    
    else if ((dth->intf->type == INTF_socket) && (path != NULL)) {
        /// Socket mode opens a listening socket
        ///@todo have listening queue size be dynamic
        struct sockaddr_un addr;
        
        dth->fd.in = socket(AF_UNIX, SOCK_STREAM, 0);
        if (dth->fd.in < 0) {
            perror("Unable to create a server socket\n");
            goto dterm_open_END;
        }
        VERBOSE_PRINTF("Socket created on fd=%i\n", dth->fd.in);
        
        ///@todo make sure this unlinking stage is OK.
        ///      unsure how to unbind the socket when server is finished.
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1); 
        unlink(path);
        
        VERBOSE_PRINTF("Binding...\n");
        if (bind(dth->fd.in, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("Unable to bind server socket");
            goto dterm_open_END;
        }
        VERBOSE_PRINTF("Binding Socket fd=%i to %s\n", dth->fd.in, path);
        
        if (listen(dth->fd.in, 5) == -1) {
            perror("Unable to enter listen on server socket");
            goto dterm_open_END;
        }
        VERBOSE_PRINTF("Listening on Socket fd=%i\n", dth->fd.in);
        
        retcode     = 0;
        dt_thread   = &dterm_socketer;
    }
    
    else {
        retcode = -1;
    }
    
    dterm_open_END:
    dterm_reset(dth->intf);
    return dt_thread;
}



int dterm_close(dterm_handle_t* dth) {
    int retcode;
    
    if (dth == NULL)        return -1;
    if (dth->intf == NULL)  return -1;
    
    if (dth->intf->type == INTF_interactive) {
        retcode = tcsetattr(dth->fd.in, TCSAFLUSH, &(dth->intf->oldter));
    }
    else {
        retcode = 0;
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


static int sub_proc_lineinput(dterm_handle_t* dth, int* cmdrc, char* loadbuf, int linelen, const char* termstring) {
    uint8_t     protocol_buf[1024];
    char        cmdname[32];
    int         cmdlen;
    cJSON*      cmdobj;
    uint8_t*    cursor  = protocol_buf;
    int         bufmax  = sizeof(protocol_buf);
    int         bytesout = 0;
    const cmdtab_item_t* cmdptr;
    
    DEBUG_PRINTF("raw input (%i bytes) %.*s\n", linelen, linelen, loadbuf);

    // Isolation memory context
    iso_ctx = dth->tctx;

    // Set allocators for cJSON, argtable
    cjson_iso_allocators();
    arg_set_allocators(&iso_malloc, &iso_free);

    ///@todo set context for other data systems

    /// The input can be JSON of the form:
    /// { "type":"${cmd_type}", data:"${cmd_data}" }
    /// where we only truly care about the data object, which must be a string.
    cmdobj = cJSON_Parse(loadbuf);
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
    cmdptr  = cmd_search(dth->ext->cmdtab, cmdname);
    if (cmdptr == NULL) {
        ///@todo build a nicer way to show where the error is,
        ///      possibly by using pi or ci (sign reversing)
        if (linelen > 0) {
            bytesout = snprintf((char*)protocol_buf, sizeof(protocol_buf)-1,
                        "{\"cmd\":\"%s\", \"err\":1, \"desc\":\"command not found\"}%s",
                        cmdname, termstring);
            //dterm_puts(&dth->fd, (char*)protocol_buf);
        }
    }
    else {
        int bytesin = linelen - cmdlen;

        ///@todo segmentation fault within cmd_run() for command:
        /// open -j /opt/otdb/examples/csip
        /// Could this be due to permissions problem?
        bytesout = cmd_run(cmdptr, dth, cursor, &bytesin, (uint8_t*)(loadbuf+cmdlen), bufmax);
        if (cmdrc != NULL) {
            *cmdrc = bytesout;
        }

        ///@todo spruce-up the command error reporting, maybe even with
        ///      a cursor showing where the first error was found.
        if (bytesout < 0) {
            bytesout = snprintf((char*)protocol_buf, sizeof(protocol_buf)-1, 
                        "{\"cmd\":\"%s\", \"err\":%d, \"desc\":\"command execution error\"}%s",
                        cmdname, bytesout, termstring);
            //dterm_puts(&dth->fd, (char*)protocol_buf);
        }

        // If there are bytes to send to interface, do that.
        // If bytesout == 0, there is no error, but also nothing to send.
        else if (bytesout > 0) {
            cursor += bytesout;
            bufmax -= bytesout;
            if (cJSON_IsObject(cmdobj)) {
                VCLIENT_PRINTF("JSON Response (%i bytes): %.*s\n", bytesout, bytesout, (char*)cursor);
                cursor += snprintf((char*)cursor, bufmax, "}%s", termstring);
            }
            else {
                cursor += snprintf((char*)cursor, bufmax, "%s", termstring);
            }
            bytesout = (int)(cursor - protocol_buf);
            
            DEBUG_PRINTF("raw output (%i bytes) %.*s\n", bytesout, bytesout, protocol_buf);
        }
    }
    
    /// If there are bytes to send back to the client, we can be extra safe
    /// by checking that the fd is still available (that client didn't die)
    /// If the file is down, we send an error to the caller so it can drop
    /// the client.
    if (bytesout > 0) {
        if ((fcntl(dth->fd.out, F_GETFD) != -1) || (errno != EBADF)) {
            write(dth->fd.out, (char*)protocol_buf, bytesout);
        }
        else {
            perror("could not write back to client");
            bytesout = -1;
        }
    }

    sub_proc_lineinput_FREE:
    cJSON_Delete(cmdobj);

    // Return cJSON and argtable to generic context allocators
    cjson_std_allocators();
    arg_set_allocators(NULL, NULL);

    return bytesout;
}







void* dterm_socket_clithread(void* args) {
/// Thread that:
/// <LI> Listens to stdin via read() pipe </LI>
/// <LI> Processes each LINE and takes action accordingly. </LI>

    dterm_handle_t* dth;
    dterm_handle_t dts;
    clithread_args_t* ct_args;
    char databuf[1024];

    ct_args = (clithread_args_t*)args;
    if (args == NULL)
        return NULL;
    if ((ct_args->app_handle == NULL) || (ct_args->tctx == NULL))
        return NULL;

    talloc_disable_null_tracking();

    // Thread-local memory elements
    dth = ((clithread_args_t*)args)->app_handle;
    memcpy(&dts, dth, sizeof(dterm_handle_t));
    dts.fd.in   = ((clithread_args_t*)args)->fd_in;
    dts.fd.out  = ((clithread_args_t*)args)->fd_out;
    dts.tctx    = ct_args->tctx;
    
    // Thread instantiation done: unblock the clithread creator
    clithread_sigup(ct_args->clithread_self);
    
    // Deferred cancellation: will wait until the blocking read() call is in
    // idle before killing the thread.
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    VERBOSE_PRINTF("Client Thread on socket:fd=%i has started\n", dts.fd.out);
    
    /// Get a packet from the Socket
    while (1) {
        int linelen;
        int loadlen;
        char* loadbuf = databuf;
        
        bzero(databuf, sizeof(databuf));
        
        VERBOSE_PRINTF("Waiting for read on socket:fd=%i\n", dts.fd.out);
        loadlen = (int)read(dts.fd.out, loadbuf, LINESIZE);
        if (loadlen > 0) {
            sub_str_sanitize(loadbuf, (size_t)loadlen);
            pthread_mutex_lock(dts.iso_mutex);
            dts.intf->state = prompt_off;
            
            do {
                // Burn whitespace ahead of command.
                while (isspace(*loadbuf)) { loadbuf++; loadlen--; }
                linelen = (int)sub_str_mark(loadbuf, (size_t)loadlen);

                // Process the line-input command
                // If there's a fatal error in the processing, we kill this thread
                if (sub_proc_lineinput(&dts, NULL, loadbuf, linelen, "\n") < 0) {
                    pthread_mutex_unlock(dts.iso_mutex);
                    goto dterm_socket_clithread_EXIT;
                }

                // +1 eats the terminator
                loadlen -= (linelen + 1);
                loadbuf += (linelen + 1);

            } while (loadlen > 0);

            pthread_mutex_unlock(dts.iso_mutex);

        }
        else {
            // After servicing the client socket, it is important to close it.
            close(dts.fd.out);
            break;
        }
    }

    dterm_socket_clithread_EXIT:

    VERBOSE_PRINTF("Client Thread on socket:fd=%i is exiting\n", dts.fd.out);
    
    /// End of thread: it *must* call clithread_exit() before exiting
    clithread_exit( ((clithread_args_t*)args)->clithread_self );
    return NULL;
}



void* dterm_socketer(void* args) {
/// Thread that:
/// <LI> Listens to stdin via read() pipe </LI>
/// <LI> Processes each LINE and takes action accordingly. </LI>
    dterm_handle_t* dth = (dterm_handle_t*)args;
    clithread_args_t clithread;
    
    // Socket operation has no interface prompt
    dth->intf->state        = prompt_off;
    clithread.app_handle    = dth;
    clithread.fd_in         = dth->fd.in;
    clithread.tctx          = NULL;
    
    /// Get a packet from the Socket
    while (1) {
        VERBOSE_PRINTF("Waiting for client accept on socket fd=%i\n", dth->fd.in);
        clithread.fd_out = accept(dth->fd.in, NULL, NULL);
        if (clithread.fd_out < 0) {
            perror("Server Socket accept() failed");
        }
        else {
            size_t poolsize = cliopt_getpoolsize();
            size_t est_obj  = 4; //(poolsize / 128) + 1;
            
            ///@todo with new clithread impl, might be fine to skip this and
            /// let clithread do this internally on the NULL context
            clithread.tctx = talloc_pooled_object(dth->pctx, void*, (unsigned int)est_obj, poolsize);
            
            ///@todo there are some problems with clithread that appear to be
            /// related to talloc's lack of support for concurrency.  For the
            /// time being, we're adding a guard.
            clithread.guard = dth->iso_mutex;
            
            clithread_add(dth->clithread, NULL, est_obj, poolsize, &dterm_socket_clithread, (void*)&clithread);
        }
    }
    
    return NULL;
}




void* dterm_piper(void* args) {
/// Thread that:
/// <LI> Listens to stdin via read() pipe </LI>
/// <LI> Processes each LINE and takes action accordingly. </LI>
    dterm_handle_t* dth     = (dterm_handle_t*)args;
    char*           loadbuf = dth->intf->linebuf;
    int             loadlen = 0;
    int             pipe_stat = 0;
    
    talloc_disable_null_tracking();
    
    // Initial state = off
    dth->intf->state = prompt_off;
    
    /// Get each line from the pipe.
    while (pipe_stat >= 0) {
        int linelen;
        size_t est_objs;
        size_t poolsize;
        
        if (loadlen <= 0) {
            dterm_reset(dth->intf);
            loadlen = (int)read(dth->fd.in, loadbuf, 1024);
            sub_str_sanitize(loadbuf, (size_t)loadlen);
        }
        
        // Burn whitespace ahead of command.
        while (isspace(*loadbuf)) { loadbuf++; loadlen--; }
        linelen = (int)sub_str_mark(loadbuf, (size_t)loadlen);

        // Create temporary context as a memory pool
        poolsize    = cliopt_getpoolsize();
        est_objs    = 4; //(poolsize / 128) + 1;
        dth->tctx   = talloc_pooled_object(NULL, void*, est_objs, poolsize);

        // Process the line-input command
        pipe_stat   = sub_proc_lineinput(dth, NULL, loadbuf, linelen, "");
        
        // Free temporary memory pool context
        talloc_free(dth->tctx);
        
        // +1 eats the terminator
        loadlen -= (linelen + 1);
        loadbuf += (linelen + 1);
    }
    
    /// This code will run only if the pipe goes down
    ERR_PRINTF("dterm_piper() closing due to unexpected closure of client pipe\n");
    raise(SIGTERM);
    return NULL;
}


int dterm_cmdfile(dterm_handle_t* dth, const char* filename) {
    int     filebuf_sz;
    char*   filecursor;
    char*   filebuf     = NULL;
    int     rc          = 0;
    FILE*   fp          = NULL;
    dterm_fd_t local;
    dterm_fd_t saved;
    
    // Initial state = off
    dth->intf->state = prompt_off;
    
    // Open the file, Load the contents into filebuf
    fp = fopen(filename, "r");
    if (fp == NULL) {
        //perror(ERRMARK"cmdfile couldn't be opened");
        return -2;
    }
    
    fseek(fp, 0L, SEEK_END);
    filebuf_sz = (int)ftell(fp);
    rewind(fp);
    filebuf = talloc_zero_size(dth->pctx, filebuf_sz+1);
    if (filebuf == NULL) {
        rc = -1;
        goto dterm_cmdfile_END;
    }
    
    rc = !(fread(filebuf, filebuf_sz, 1, fp) == 1);
    if (rc != 0) {
        //perror(ERRMARK"cmdfile couldn't be read");
        rc = -3;
        goto dterm_cmdfile_END;
    }
    
    // File stream no longer required
    fclose(fp);
    fp = NULL;
    
    // Preprocess the command inputs strings
    sub_str_sanitize(filebuf, (size_t)filebuf_sz);
    
    // Reset the terminal to default state
    dterm_reset(dth->intf);
    
    pthread_mutex_lock(dth->iso_mutex);
    local.in    = STDIN_FILENO;
    local.out   = STDOUT_FILENO;
    saved       = dth->fd;
    dth->fd     = local;
    
    // Run the command on each line
    filecursor = filebuf;
    while (filebuf_sz > 0) {
        int linelen;
        int cmdrc;
        size_t est_objs;
        size_t poolsize;
        
        // Burn whitespace ahead of command.
        while (isspace(*filecursor)) { filecursor++; filebuf_sz--; }
        linelen = (int)sub_str_mark(filecursor, (size_t)filebuf_sz);

        // Create temporary context as a memory pool
        poolsize    = cliopt_getpoolsize();
        est_objs    = 4; //(poolsize / 128) + 1;
        dth->tctx   = talloc_pooled_object(NULL, void*, est_objs, poolsize);
        
        // Echo input line to dterm
        dprintf(dth->fd.out, _E_MAG"%s"_E_NRM"%s\n", prompt_root, filecursor);
        
        // Process the line-input command.  Exit on proc error
        if (sub_proc_lineinput(dth, &cmdrc, filecursor, linelen, "\n") < 0) {
            filebuf_sz = 0;
        }
        
        // Free temporary memory pool context
        talloc_free(dth->tctx);
        
        // Exit the command sequence on first detection of error.
        if (cmdrc < 0) {
            rc = -4;
            dprintf(dth->fd.out, _E_RED"ERR: "_E_NRM"Command Returned %i: stopping.\n\n", cmdrc);
            break;
        }
        
        // +1 eats the terminator
        filebuf_sz -= (linelen + 1);
        filecursor += (linelen + 1);
    }
    
    dth->fd = saved;
    pthread_mutex_unlock(dth->iso_mutex);

    dterm_cmdfile_END:
    if (fp != NULL) fclose(fp);
    talloc_free(filebuf);
    
    return rc;
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
    
    // Initialize command history
    ((dterm_handle_t*)args)->ch = ch_init();
    if (((dterm_handle_t*)args)->ch == NULL) {
        goto dterm_prompter_TERM;
    }
    
    // Initialize
    
    // Local pointer for command history is just for making code look nicer
    ch = ((dterm_handle_t*)args)->ch;
    
    talloc_disable_null_tracking();
    
    // Initial state = off
    dth->intf->state = prompt_off;
    
    /// Get each keystroke.
    /// A keystoke is reported either as a single character or as three.
    /// triple-char keystrokes are for special keys like arrows and control
    /// sequences.
    ///@note dterm_read() will keep the thread asleep, blocking it until data arrives
    while ((keychars = read(dth->fd.in, dth->intf->readbuf, READSIZE)) > 0) {
        
        // Default: IGNORE
        cmd = ct_ignore;
        
        // If dterm state is off, ignore anything except ESCAPE
        ///@todo mutex unlocking on dt->state
        
        if ((dth->intf->state == prompt_off) && (keychars == 1) && (dth->intf->readbuf[0] <= 0x1f)) {
            cmd = npcodes[dth->intf->readbuf[0]];
            
            // Only valid commands when prompt is OFF are prompt, sigint, sigquit
            // Using prompt (ESC) will open a prompt and ignore the escape
            // Using sigquit (Ctl+\) or sigint (Ctl+C) will kill the program
            // Using any other key will be ignored
            if ((cmd != ct_prompt) && (cmd != ct_sigquit) && (cmd != ct_sigint)) {
                continue;
            }
        }
        
        else if (dth->intf->state == prompt_on) {
            if (keychars == 1) {
                c = dth->intf->readbuf[0];
                if (c <= 0x1F)              cmd = npcodes[c];   // Non-printable characters except DELETE
                else if (c == ASCII_DEL)    cmd = ct_delete;    // Delete (0x7F)
                else                        cmd = ct_key;       // Printable characters
            }
            
            else if (keychars == 3) {
                if ((dth->intf->readbuf[0] == VT100_UPARR[0]) && (dth->intf->readbuf[1] == VT100_UPARR[1])) {
                    if (dth->intf->readbuf[2] == VT100_UPARR[2]) {
                        cmd = ct_histnext;
                    }
                    else if (dth->intf->readbuf[2] == VT100_DWARR[2]) {
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
        if (dth->intf->state == prompt_off) {
            pthread_mutex_lock(dth->iso_mutex);
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
                                    sigcode     = SIGTERM; 
                                    break;
                                    
                case ct_sigquit:    killstring  = str_ct_sigquit;
                                    sigcode     = SIGQUIT;
                                    break;
                                    
                default:            killstring  = str_unknown;
                                    sigcode     = SIGQUIT; 
                                    break;
            }
            
            dterm_reset(dth->intf);
            dterm_puts(&dth->fd, (char*)killstring);
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
                    dterm_putcmd(dth->intf, &c, 1);
                    //dterm_put(dt, &c, 1);
                    dterm_putc(&dth->fd, c);
                } break;
                                    
                // Prompt-Escape is pressed, 
                case ct_prompt: {    
                    if (dth->intf->state == prompt_on) {
                        dterm_remln(dth->intf, &dth->fd);
                        dth->intf->state = prompt_off;
                    }
                    else {
                        dterm_puts(&dth->fd, (char*)prompt_str[0]);
                        dth->intf->state = prompt_on;
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
                    size_t est_objs;
                    size_t poolsize;
                    
                    //dterm_put(dt, (char[]){ASCII_NEWLN}, 2);
                    dterm_putc(&dth->fd, '\n');
                    
                    if (!ch_contains(ch, dth->intf->linebuf)) {
                        ch_add(ch, dth->intf->linebuf);
                    }
                    
                    // Create temporary context as a memory pool
                    poolsize    = cliopt_getpoolsize();
                    est_objs    = 4; //(poolsize / 128) + 1;
                    dth->tctx   = talloc_pooled_object(NULL, void*, est_objs, poolsize);
                    
                    // Run command(s) from line input
                    bytesout = sub_proc_lineinput( dth, NULL,
                                        (char*)dth->intf->linebuf,
                                        (int)sub_str_mark((char*)dth->intf->linebuf, 1024),
                                        "\n"
                                    );
                    
                    // Free temporary memory pool context
                    talloc_free(dth->tctx);
                    
                    // If there's meaningful output, add a linebreak
                    //if (bytesout > 0) {
                    //    dterm_puts(&dth->fd, "\n");
                    //}

                    dterm_reset(dth->intf);
                    dth->intf->state = prompt_close;
                } break;
                
                // TAB presses cause the autofill operation (a common feature)
                // autofill will try to finish the command input
                case ct_autofill: {
                    cmdlen = cmd_getname((char*)cmdname, dth->intf->linebuf, sizeof(cmdname));
                    cmdptr = cmd_subsearch(dth->ext->cmdtab, (char*)cmdname);
                    if ((cmdptr != NULL) && (dth->intf->linebuf[cmdlen] == 0)) {
                        dterm_remln(dth->intf, &dth->fd);
                        dterm_puts(&dth->fd, (char*)prompt_str[0]);
                        dterm_putsc(dth->intf, (char*)cmdptr->name);
                        dterm_puts(&dth->fd, (char*)cmdptr->name);
                    }
                    else {
                        dterm_puts(&dth->fd, ASCII_BEL);
                    }
                } break;
                
                // DOWN-ARROW presses fill the prompt with the next command 
                // entry in the command history
                case ct_histnext: {
                    //cmdstr = ch_next(ch);
                    cmdstr = ch_prev(ch);
                    if (ch->count && cmdstr) {
                        dterm_remln(dth->intf, &dth->fd);
                        dterm_puts(&dth->fd, (char*)prompt_str[0]);
                        dterm_putsc(dth->intf, cmdstr);
                        dterm_puts(&dth->fd, cmdstr);
                    }
                } break;
                
                // UP-ARROW presses fill the prompt with the last command
                // entry in the command history
                case ct_histprev: {
                    //cmdstr = ch_prev(ch);
                    cmdstr = ch_next(ch);
                    if (ch->count && cmdstr) {
                        dterm_remln(dth->intf, &dth->fd);
                        dterm_puts(&dth->fd, (char*)prompt_str[0]);
                        dterm_putsc(dth->intf, cmdstr);
                        dterm_puts(&dth->fd, cmdstr);
                    }
                } break;
                
                // DELETE presses issue a forward-DELETE
                case ct_delete: { 
                    if (dth->intf->linelen > 0) {
                        dterm_remc(dth->intf, 1);
                        dterm_put(&dth->fd, VT100_CLEAR_CH, 4);
                    }
                } break;
                
                // Every other command is ignored here.
                default: {
                    dth->intf->state = prompt_close;
                } break;
            }
        }
        
        // Unlock Mutex
        if (dth->intf->state != prompt_on) {
            dth->intf->state = prompt_off;
            pthread_mutex_unlock(dth->iso_mutex);
        }
        
    }
    
    dterm_prompter_TERM:
    
    /// This code should never occur, given the while(1) loop.
    /// If it does (possibly a stack fuck-up), we print this "chaotic error."
    fprintf(stderr, "\n--> Chaotic error: dterm_prompter() thread broke loop.\n");
    raise(SIGTERM);
    return NULL;
}














/** Subroutines for reading & writing
  * ========================================================================<BR>
  */


int dterm_put(dterm_fd_t* fd, char *s, int size) {
    return (int)write(fd->out, s, size);
}

int dterm_puts(dterm_fd_t* fd, char *s) {
    char* end = s-1;
    while (*(++end) != 0);
        
    return (int)write(fd->out, s, end-s);
}

int dterm_putc(dterm_fd_t* fd, char c) {
    return (int)write(fd->out, &c, 1);
}


int dterm_putsc(dterm_intf_t *dt, char *s) {
    uint8_t* end = (uint8_t*)s - 1;
    while (*(++end) != 0);
    
    return dterm_putcmd(dt, s, (int)(end-(uint8_t*)s) );
}




int dterm_putcmd(dterm_intf_t *dt, char *s, int size) {
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




int dterm_remc(dterm_intf_t *dt, int count) {
    int cl = dt->linelen;
    while (count-- > 0) {
        *dt->cline-- = 0;
        dt->linelen--;
    }
    return cl - dt->linelen;
}



void dterm_remln(dterm_intf_t *dt, dterm_fd_t* fd) {
    dterm_put(fd, VT100_CLEAR_LN, 5);
    dterm_reset(dt);
}



void dterm_reset(dterm_intf_t *dt) {
    dt->cline = dt->linebuf;
    
    while (dt->cline < (dt->linebuf + LINESIZE)) {
        *dt->cline++ = 0;  
    }
    
    dt->cline    = dt->linebuf;
    dt->linelen  = 0;
}



