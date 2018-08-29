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
/**
  * @file       main.c
  * @author     JP Norair
  * @version    R100
  * @date       25 May 2018
  * @brief      OTDB main() function and global data declarations
  * @defgroup   OTDB
  * @ingroup    OTDB
  * 
  * OTDB (OpenTag DataBase) is a threaded, POSIX-C app that provides a cache
  * for OpenTag device filesystems.  In other words, it allows the device 
  * filesystems to be mirrored and synchronized on a gateway device (typically
  * a computer of some sort, running some sort of Linux).
  *
  * See http://wiki.indigresso.com for more information and documentation.
  * 
  ******************************************************************************
  */

// Top Level Configuration Header
#include "otdb_cfg.h"

// Application Headers
#include "pktlist.h"
#include "ppio.h"
#include "cmdsearch.h"
#include "cmdhistory.h"
#include "cliopt.h"
#include "debug.h"

// HBuilder Package Libraries
#include <argtable3.h>
#include <cJSON.h>
#include <cmdtab.h>     // Maybe

// Standard C & POSIX Libraries
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>



// Client Data type
///@todo some of this should get merged into MPipe data type

static cliopt_t cliopts;

typedef struct {
    //FILE*           out;
    //FILE*           in;
    //cliopt_struct   opt;
    //bool            block_in;
    
    int             exitcode;
    pthread_mutex_t kill_mutex;
    pthread_cond_t  kill_cond;
    
    char*           call_path;
    
} cli_struct;

cli_struct cli;





/** Local Functions <BR>
  * ========================================================================<BR>
  * otdb_main() is called by main().  The job of main() is to parse arguments
  * and then send them to otdb_main(), which deals with program setup and
  * management.
  */
  
static void sub_json_loadargs(cJSON* json, bool* verbose_val, bool* debug_val, int* intf_val, char* xpath);

static int otdb_main(INTF_Type intf_val, const char* xpath, cJSON* params); 






/** signal handlers <BR>
  * ========================================================================<BR>
  */

static void sub_assign_signal(int sigcode, void (*sighandler)(int)) {
    if (signal(sigcode, sighandler) != 0) {
        fprintf(stderr, "--> Error assigning signal (%d), exiting\n", sigcode);
        exit(EXIT_FAILURE);
    }
}

static void sigint_handler(int sigcode) {
    cli.exitcode = EXIT_FAILURE;
    pthread_cond_signal(&cli.kill_cond);
}

static void sigquit_handler(int sigcode) {
    cli.exitcode = EXIT_SUCCESS;
    pthread_cond_signal(&cli.kill_cond);
}






// NOTE Change to transparent mutex usage or not?
/*
void _cli_block(bool set) {
    pthread_mutex_lock(&cli.block_mutex);
    cli.block_in = set;
    pthread_mutex_unlock(&cli.block_mutex);
}

bool _cli_is_blocked(void) {
    volatile bool retval;
    pthread_mutex_lock(&cli.block_mutex);
    retval = cli.block_in;
    pthread_mutex_unlock(&cli.block_mutex);
    
    return retval;
}
*/




// notes:
// 0. ch_inc/ch_dec (w/ ?:) and history pointers can be replaced by integers and modulo
// 1. command history is glitching when command takes whole history buf (not c string)
// 2. delete and remove line do not work for multiline command

static dterm_t* _dtputs_dterm;

int _dtputs(char* str) {
    return dterm_puts(_dtputs_dterm, str);
}


static INTF_Type sub_intf_cmp(const char* s1) {
    INTF_Type selected_intf;

    if (strcmp(s1, "pipe") == 0) {
        selected_intf = INTF_pipe;
    }
    else if (strcmp(s1, "socket") == 0) {
        selected_intf = INTF_socket;
    }
    else {
        selected_intf = INTF_interactive;
    }
    
    return selected_intf;
}



int main(int argc, char* argv[]) {
    struct arg_file *config  = arg_file0("C", "config", "<file.json>",  "JSON based configuration file.");
    struct arg_lit  *verbose = arg_lit0("v","verbose",                  "use verbose mode");
    struct arg_lit  *debug   = arg_lit0("d","debug",                    "Set debug mode on: requires compiling for debug");
    struct arg_str  *intf    = arg_str0("i","intf", "interactive|pipe|socket", "Interface select.  Default: interactive");
    struct arg_file *xpath   = arg_file0("x", "xpath", "<filepath>",     "Path to directory of external data processor programs");
    struct arg_lit  *help    = arg_lit0(NULL,"help",                    "print this help and exit");
    struct arg_lit  *version = arg_lit0(NULL,"version",                 "print version information and exit");
    struct arg_end  *end     = arg_end(10);
    
    void* argtable[]        = { config, verbose, debug, intf, xpath, help, version, end };
    const char* progname    = OTDB_PARAM(NAME);
    int exitcode            = 0;
    bool verbose_val        = false;
    bool debug_val          = false;
    cJSON* json             = NULL;
    char* buffer            = NULL;
    INTF_Type intf_val      = INTF_interactive;
    char xpath_val[256]     = "";
    
    int nerrors;
    bool bailout = false;

    if (arg_nullcheck(argtable) != 0) {
        /// NULL entries were detected, some allocations must have failed 
        fprintf(stderr, "%s: insufficient memory\n", progname);
        exitcode = 1;
        bailout = true;
        goto main_FINISH;
    }

    /* Parse the command line as defined by argtable[] */
    nerrors = arg_parse(argc, argv, argtable);

    /// special case: '--help' takes precedence over error reporting
    if (help->count > 0) {
        printf("Usage: %s", progname);
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        exitcode = 0;
        bailout = true;
        goto main_FINISH;
    }

    /// special case: '--version' takes precedence error reporting 
    if (version->count > 0) {
        ///@todo change to new info format
        printf("%s -- %s\n", OTDB_PARAM_VERSION, OTDB_PARAM_DATE);
        printf("Designed by JP Norair (jpnorair@indigresso.com)\n");
        exitcode = 0;
        bailout = true;
        goto main_FINISH;
    }

    /// If the parser returned any errors then display them and exit
    /// - Display the error details contained in the arg_end struct.
    if (nerrors > 0) {
        arg_print_errors(stdout,end,progname);
        printf("Try '%s --help' for more information.\n", progname);
        exitcode = 1;
        bailout = true;
        goto main_FINISH;
    }

    /// special case: with no command line options induces brief help 
    if (argc==1) {
        printf("Try '%s --help' for more information.\n",progname);
        exitcode = 0;
        bailout = true;
        goto main_FINISH;
    }

    /// Do some final checking of input values
    ///
    /// Get JSON config input.  Priority is direct input vs. file input
    /// 1. There is an "arguments" object that works the same as the command 
    ///    line arguments.  
    /// 2. Any other objects may be used in custom ways by the app itself.
    ///    In particular, they can be used for loading custom keys & certs.
    if (config->count > 0) {
        FILE* fp;
        long lSize;
        fp = fopen(config->filename[0], "r");
        if (fp == NULL) {
            exitcode = (int)'f';
            bailout = true;
            goto main_FINISH;
        }

        fseek(fp, 0L, SEEK_END);
        lSize = ftell(fp);
        rewind(fp);

        buffer = calloc(1, lSize+1);
        if (buffer == NULL) {
            exitcode = (int)'m';
            bailout = true;
            goto main_FINISH;
        }

        if(fread(buffer, lSize, 1, fp) == 1) {
            json = cJSON_Parse(buffer);
            fclose(fp);
        }
        else {
            fclose(fp);
            fprintf(stderr, "read to %s fails\n", config->filename[0]);
            exitcode = (int)'r';
            bailout = true;
            goto main_FINISH;
        }

        /// At this point the file is closed and the json is parsed into the
        /// "json" variable.  
        if (json == NULL) {
            fprintf(stderr, "JSON parsing failed.  Exiting.\n");
            bailout = true;
            goto main_FINISH;
        }
        {   int tmp_intf;
            sub_json_loadargs(json, &verbose_val, &debug_val, &tmp_intf, xpath_val);
            intf_val = tmp_intf;
        }
    }
    
    /// If no JSON file, then configuration should be through the arguments.
    /// If both exist, then the arguments will override JSON.
    if (intf->count != 0) {
        intf_val = sub_intf_cmp(intf->sval[0]);
    }
    if (xpath->count != 0) {
        strncpy(xpath_val, xpath->filename[0], 256);
    }
    if (verbose->count != 0) {
        verbose_val = true;
    }
    
    /// Client Options.  These are read-only from internal modules
    cliopts.format      = FORMAT_Dynamic;
    cliopts.intf        = intf_val;
    if (debug->count != 0) {
        cliopts.debug_on    = true;
        cliopts.verbose_on  = true;
    }
    else {
        cliopts.debug_on    = false;
        cliopts.verbose_on  = verbose_val;
    }
    cliopt_init(&cliopts);
    
    
    /// All configuration is done.
    /// Send all configuration data to program main function.
    main_FINISH:
    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
    
    if (bailout == false) {
        exitcode = otdb_main(intf_val, (const char*)xpath_val, json);
    }
    
    if (json != NULL) {
        cJSON_Delete(json);
    }
    if (buffer != NULL) {
        free(buffer);
    }

    return exitcode;
}




/// What this should do is start two threads, one for the character I/O on
/// the dterm side, and one for the serial I/O.
int otdb_main(  INTF_Type intf_val,
                const char* xpath,
                cJSON* params   ) {    
    
    // DTerm Datastructs
    dterm_arg_t dterm_args;
    dterm_t     dterm;
    cmdhist     cmd_history;
    
    // Child Threads (1)
    void*       (*dterm_fn)(void* args);
    pthread_t   thr_dterm;
    
    // Mutexes used with Child Threads
    pthread_mutex_t dtwrite_mutex;          // For control of writeout to dterm
    pthread_mutex_t rlist_mutex;
    pthread_mutex_t tlist_mutex;
    
    // Cond/Mutex used with Child Threads
    pthread_cond_t  tlist_cond;
    pthread_mutex_t tlist_cond_mutex;
    pthread_cond_t  pktrx_cond;
    pthread_mutex_t pktrx_mutex;
    
    /// Initialize command search table.  
    ///@todo in the future, let's pull this from an initialization file or
    ///      something dynamic as such.
    cmd_init(NULL, xpath);
    
    
    /// Initialize Thread Mutexes & Conds.  This is finnicky and it must be
    /// done before assignment into the argument containers, possibly due to 
    /// C-compiler foolishly optimizing.
    assert( pthread_mutex_init(&dtwrite_mutex, NULL) == 0 );
    assert( pthread_mutex_init(&rlist_mutex, NULL) == 0 );
    assert( pthread_mutex_init(&tlist_mutex, NULL) == 0 );
    assert( pthread_mutex_init(&cli.kill_mutex, NULL) == 0 );
    pthread_cond_init(&cli.kill_cond, NULL);
    assert( pthread_mutex_init(&tlist_cond_mutex, NULL) == 0 );
    pthread_cond_init(&tlist_cond, NULL);
    assert( pthread_mutex_init(&pktrx_mutex, NULL) == 0 );
    pthread_cond_init(&pktrx_cond, NULL);

    
    /// Open DTerm interface & Setup DTerm threads
    /// The dterm thread will deal with all other aspects, such as command
    /// entry and history initialization.
    ///@todo "STDIN_FILENO" and "STDOUT_FILENO" could be made dynamic
    _dtputs_dterm               = &dterm;
    dterm.fd_in                 = STDIN_FILENO;
    dterm.fd_out                = STDOUT_FILENO;
    dterm_args.ch               = ch_init(&cmd_history);
    dterm_args.dt               = &dterm;
    dterm_args.dtwrite_mutex    = &dtwrite_mutex;
    dterm_args.kill_mutex       = &cli.kill_mutex;
    dterm_args.kill_cond        = &cli.kill_cond;
    
    ///@todo implement socket variant
    switch(intf_val) {
        case INTF_pipe:     dterm_fn = &dterm_piper;    break;
        //case INTF_socket:   dterm_fn = &dterm_socket;   break;    
        default:            dterm_fn = &dterm_prompter; break;
    }
    if (dterm_open(&dterm, intf_val) < 0) {
        cli.exitcode = -2;
        goto otdb_main_TERM2;
    }
    
    
    /// Initialize the signal handlers for this process.
    /// These are activated by Ctl+C (SIGINT) and Ctl+\ (SIGQUIT) as is
    /// typical in POSIX apps.  When activated, the threads are halted and
    /// Otter is shutdown.
    cli.exitcode = EXIT_SUCCESS;
    sub_assign_signal(SIGINT, &sigint_handler);
    sub_assign_signal(SIGQUIT, &sigquit_handler);
    
    
    /// Invoke the child threads below.  All of the child threads run
    /// indefinitely until an error occurs or until the user quits.  Quit can 
    /// be via Ctl+C or Ctl+\, or potentially also through a dterm command.  
    /// Each thread must be be implemented to raise SIGQUIT or SIGINT on exit
    /// i.e. raise(SIGINT).
    pthread_create(&thr_dterm, NULL, dterm_fn, (void*)&dterm_args);
    DEBUG_PRINTF("Finished creating theads\n");
    
    /// Threads are now running.  The rest of the main() code, below, is
    /// blocked by pthread_cond_wait() until the kill_cond is sent by one of 
    /// the child threads.  This will cause the program to quit.
    pthread_mutex_lock(&cli.kill_mutex);
    pthread_cond_wait(&cli.kill_cond, &cli.kill_mutex);
    
    DEBUG_PRINTF("Cancelling Theads\n");
    pthread_cancel(thr_dterm);
    
    otdb_main_TERM:
    DEBUG_PRINTF("Destroying thread resources\n");
    pthread_mutex_unlock(&dtwrite_mutex);
    pthread_mutex_destroy(&dtwrite_mutex);
    DEBUG_PRINTF("-- dtwrite_mutex destroyed\n");
    pthread_mutex_unlock(&rlist_mutex);
    pthread_mutex_destroy(&rlist_mutex);
    DEBUG_PRINTF("-- rlist_mutex destroyed\n");
    pthread_mutex_unlock(&tlist_mutex);
    pthread_mutex_destroy(&tlist_mutex);
    DEBUG_PRINTF("-- tlist_mutex destroyed\n");
    pthread_mutex_unlock(&tlist_cond_mutex);
    pthread_mutex_destroy(&tlist_cond_mutex);
    pthread_cond_destroy(&tlist_cond);
    DEBUG_PRINTF("-- tlist_mutex & tlist_cond destroyed\n");
    pthread_mutex_unlock(&pktrx_mutex);
    pthread_mutex_destroy(&pktrx_mutex);
    pthread_cond_destroy(&pktrx_cond);
    
    
    /// Close the drivers/files and free all allocated data objects (primarily 
    /// in mpipe).
    if (intf_val == INTF_interactive) {
        DEBUG_PRINTF("Closing DTerm\n");
        dterm_close(&dterm);
    }
    
    otdb_main_TERM2:
    DEBUG_PRINTF("Freeing DTerm and Command History\n");
    dterm_free(&dterm);
    ch_free(&cmd_history);
    
    // cli.exitcode is set to 0, unless sigint is raised.
    DEBUG_PRINTF("Exiting cleanly and flushing output buffers\n");
    fflush(stdout);
    fflush(stderr);
    
    ///@todo there is a SIGILL that happens on pthread_cond_destroy(), but only
    ///      after packets have been TX'ed.
    ///      - Happens on two different systems
    ///      - May need to use valgrind to figure out what is happening
    ///      - after fixed, can move this code block upwards.
    DEBUG_PRINTF("-- rlist_mutex & rlist_cond destroyed\n");
    pthread_mutex_unlock(&cli.kill_mutex);
    DEBUG_PRINTF("-- pthread_mutex_unlock(&cli.kill_mutex)\n");
    pthread_mutex_destroy(&cli.kill_mutex);
    DEBUG_PRINTF("-- pthread_mutex_destroy(&cli.kill_mutex)\n");
    pthread_cond_destroy(&cli.kill_cond);
    DEBUG_PRINTF("-- cli.kill_mutex & cli.kill_cond destroyed\n");
    
    return cli.exitcode;
}



void sub_json_loadargs(cJSON* json, bool* debug_val, bool* verbose_val, int* intf_val, char* xpath) {

#   define GET_STRINGENUM_ARG(DST, FUNC, NAME) do { \
        arg = cJSON_GetObjectItem(json, NAME);  \
        if (arg != NULL) {  \
            if (cJSON_IsString(arg) != 0) {    \
                *DST = FUNC(arg->valuestring); \
            }   \
        }   \
    } while(0)
                       
#   define GET_STRING_ARG(DST, LIMIT, NAME) do { \
        arg = cJSON_GetObjectItem(json, NAME);  \
        if (arg != NULL) {  \
            if (cJSON_IsString(arg) != 0) {    \
                strncpy(DST, arg->valuestring, LIMIT);   \
            }   \
        }   \
    } while(0)
    
#   define GET_CHAR_ARG(DST, NAME) do { \
        arg = cJSON_GetObjectItem(json, NAME);  \
        if (arg != NULL) {  \
            if (cJSON_IsString(arg) != 0) {    \
                *DST = arg->valuestring[0]; \
            }   \
        }   \
    } while(0)
    
#   define GET_INT_ARG(DST, NAME) do { \
        arg = cJSON_GetObjectItem(json, NAME);  \
        if (arg != NULL) {  \
            if (cJSON_IsNumber(arg) != 0) {    \
                *DST = (int)arg->valueint;   \
            }   \
        }   \
    } while(0)
    
#   define GET_BOOL_ARG(DST, NAME) do { \
        arg = cJSON_GetObjectItem(json, NAME);  \
        if (arg != NULL) {  \
            if (cJSON_IsNumber(arg) != 0) {    \
                *DST = (arg->valueint != 0);   \
            }   \
        }   \
    } while(0)
    
    cJSON* arg;
    
    ///1. Get "arguments" object, if it exists
    json = cJSON_GetObjectItem(json, "arguments");
    if (json == NULL) {
        return;
    }
    
    GET_STRINGENUM_ARG(intf_val, sub_intf_cmp, "intf");
    GET_STRING_ARG(xpath, 256, "xpath");
    
    /// 2. Systematically get all of the individual arguments
    GET_BOOL_ARG(debug_val, "debug");
    GET_BOOL_ARG(verbose_val, "verbose");
}





 
