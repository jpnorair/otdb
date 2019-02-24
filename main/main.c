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
#include "cmds.h"
#include "cmdsearch.h"
#include "cmdhistory.h"
#include "cliopt.h"
#include "debug.h"
#include "popen2.h"

// HBuilder Package Libraries
#include <argtable3.h>
#include <cJSON.h>
#include <cmdtab.h>
#include <otfs.h>
#include <talloc.h>

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

static void sub_tfree(void* ctx) {
    talloc_free(ctx);
}

static void sub_json_loadargs(  cJSON* json, 
                                int* verbose_val,
                                int* debug_val,
                                int* intf_val, 
                                char** socket, 
                                char** initfile,
                                char** devmgr,
                                char** xpath);

static int otdb_main(   INTF_Type intf_val, 
                        const char* socket, 
                        const char* initfile,
                        const char* devmgr, 
                        const char* xpath,
                        cJSON* params); 






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
    cli.exitcode = EXIT_SUCCESS;
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

static int sub_copy_stringarg(char** dststring, int argcount, const char* argstring) {
    if (argcount != 0) {
        size_t sz;
        if (*dststring != NULL) {
            free(*dststring);
        }
        sz = strlen(argstring) + 1;
        *dststring = malloc(sz);
        if (*dststring == NULL) {
            return -1;
        }
        memcpy(*dststring, argstring, sz);
        return argcount;
    }
    return 0;
}




int main(int argc, char* argv[]) {
    struct arg_file *config  = arg_file0("c", "config", "<file.json>",  "JSON based configuration file.");
    struct arg_lit  *verbose = arg_lit0("v","verbose",                  "use verbose mode");
    struct arg_lit  *debug   = arg_lit0("d","debug",                    "Set debug mode on: requires compiling for debug");
    struct arg_str  *intf    = arg_str0("i","intf", "interactive|pipe|socket", "Interface select.  Default: interactive");
    struct arg_file *socket  = arg_file0("S","socket","path/addr",      "Socket path/address to use for otdb daemon");
    struct arg_file *initfile= arg_file0("I","init","path",             "Path to initialization routine to run at startup");
    struct arg_str  *devmgr  = arg_str0("D", "devmgr", "cmd string",    "Command string to invoke device manager app");
    struct arg_file *xpath   = arg_file0("x", "xpath", "<filepath>",    "Path to directory of external data processor programs");
    struct arg_lit  *help    = arg_lit0(NULL,"help",                    "print this help and exit");
    struct arg_lit  *version = arg_lit0(NULL,"version",                 "print version information and exit");
    struct arg_end  *end     = arg_end(10);
    
    void* argtable[] = { config, verbose, debug, intf, socket, initfile, devmgr, xpath, help, version, end };
    const char* progname = OTDB_PARAM(NAME);
    int nerrors;
    bool bailout        = true;
    int exitcode        = 0;
    int test;
    
    char* xpath_val     = NULL;
    char* socket_val    = NULL;
    char* initfile_val  = NULL;
    char* devmgr_val    = NULL;
    cJSON* json         = NULL;
    char* buffer        = NULL;
    
    INTF_Type intf_val  = INTF_interactive;
    bool verbose_val    = false;
    bool debug_val      = false;
    
    /// Initialize allocators in argtable lib to defaults
    arg_set_allocators(NULL, NULL);
    
    /// Initialize allocators in CJSON lib to defaults
    cJSON_InitHooks(NULL);
    
    if (arg_nullcheck(argtable) != 0) {
        /// NULL entries were detected, some allocations must have failed 
        fprintf(stderr, "%s: insufficient memory\n", progname);
        exitcode = 1;
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
        goto main_FINISH;
    }

    /// special case: '--version' takes precedence error reporting 
    if (version->count > 0) {
        ///@todo change to new info format
        printf("%s -- %s\n", OTDB_PARAM_VERSION, OTDB_PARAM_DATE);
        printf("Commit-ID: %s\n", OTDB_PARAM_GITHEAD);
        printf("Designed by JP Norair (jpnorair@indigresso.com)\n");
        
        exitcode = 0;
        goto main_FINISH;
    }

    /// If the parser returned any errors then display them and exit
    /// - Display the error details contained in the arg_end struct.
    if (nerrors > 0) {
        arg_print_errors(stdout,end,progname);
        printf("Try '%s --help' for more information.\n", progname);
        exitcode = 1;
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
            goto main_FINISH;
        }

        fseek(fp, 0L, SEEK_END);
        lSize = ftell(fp);
        rewind(fp);

        buffer = calloc(1, lSize+1);
        if (buffer == NULL) {
            exitcode = (int)'m';
            goto main_FINISH;
        }

        if(fread(buffer, lSize, 1, fp) == 1) {
            json = cJSON_Parse(buffer);
            fclose(fp);
            free(buffer);
            buffer = NULL;
        }
        else {
            fclose(fp);
            fprintf(stderr, "read to %s fails\n", config->filename[0]);
            exitcode = (int)'r';
            goto main_FINISH;
        }

        /// At this point the file is closed and the json is parsed into the
        /// "json" variable.  
        if (json == NULL) {
            fprintf(stderr, "JSON parsing failed.  Exiting.\n");
            goto main_FINISH;
        }
        {   int tmp_intf, tmp_verbose, tmp_debug;
            sub_json_loadargs(json, &tmp_debug, &tmp_verbose, &tmp_intf, &socket_val, &initfile_val, &devmgr_val, &xpath_val);
            intf_val    = tmp_intf;
            verbose_val = (bool)tmp_verbose;
            debug_val   = (bool)tmp_debug;
        }
    }

    /// If no JSON file, then configuration should be through the arguments.
    /// If both exist, then the arguments will override JSON.
    if (intf->count != 0) {
        intf_val = sub_intf_cmp(intf->sval[0]);
    }
    cliopts.intf = intf_val;
    
    test = sub_copy_stringarg(&socket_val, socket->count, socket->filename[0]);
    if (test < 0)       goto main_FINISH;
    else if (test > 0)  intf_val = INTF_socket;

    test = sub_copy_stringarg(&initfile_val, initfile->count, initfile->filename[0]);
    if (test < 0)       goto main_FINISH;
    
    test = sub_copy_stringarg(&devmgr_val, devmgr->count, devmgr->sval[0]);
    if (test < 0)       goto main_FINISH;
    
    test = sub_copy_stringarg(&xpath_val, xpath->count, xpath->filename[0]);
    if (test < 0)       goto main_FINISH;

    if (verbose->count != 0) {
        verbose_val = true;
    }
    cliopts.verbose_on = verbose_val;
    
    if (debug->count != 0) {
        debug_val = true;
    }
    cliopts.debug_on = debug_val;
    
    /// Client Options.  These are read-only from internal modules
    cliopts.format = FORMAT_Dynamic;
    cliopt_init(&cliopts);
    
    /// All configuration is done.
    /// Send all configuration data to program main function.
    bailout = false;
    
    main_FINISH:
    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
    
    if (bailout == false) {
        exitcode = otdb_main(   intf_val, 
                                (const char*)socket_val, 
                                (const char*)initfile_val,
                                (const char*)devmgr_val, 
                                (const char*)xpath_val, 
                                json    );
    }

    cJSON_Delete(json);
    
    free(buffer);
    free(socket_val);
    free(initfile_val);
    free(devmgr_val);
    free(xpath_val);

    return exitcode;
}




/// What this should do is start two threads, one for the character I/O on
/// the dterm side, and one for the serial I/O.
int otdb_main(  INTF_Type intf_val,
                const char* socket,
                const char* initfile,
                const char* devmgr,
                const char* xpath,
                cJSON* params   ) { 
                   
    // Devmgr process
    childproc_t devmgr_proc;
    cmdtab_t main_cmdtab;
    
    // Application data hooked into dterm (for now)
    dterm_ext_t appdata = {
        .cmdtab = NULL,
        .devmgr = NULL,
        .db = NULL,
        .tmpl = NULL
    };
    
    // DTerm Datastructs
    dterm_handle_t dterm_handle;
    
    // Child Threads (1)
    void*       (*dterm_fn)(void* args);
    pthread_t   thr_dterm;
    
    DEBUG_PRINTF("otdb_main()\n  intf_val=%i\n  socket=%s\n  xpath=%s\n", intf_val, socket, xpath);
    
    /// Initialize Thread Mutexes & Conds.  This is finnicky.
    assert( pthread_mutex_init(&cli.kill_mutex, NULL) == 0 );
    pthread_cond_init(&cli.kill_cond, NULL);
    
    /// Initialize command table
    DEBUG_PRINTF("Initializing command table...\n");
    if (cmdtab_init(&main_cmdtab) == 0) {
        appdata.cmdtab = &main_cmdtab;
    }
    else {
        fprintf(stderr, "Err: command table cannot be initialized.\n");
        cli.exitcode = -2;
        goto otdb_main_TERM3;
    }
    DEBUG_PRINTF("--> done\n");
    
    /// Start the devmgr childprocess, if one is specified.
    /// If it works, the devmgr command should be added using the name of the
    /// program used for devmgr.
    DEBUG_PRINTF("Initializing devmgr (%s) ...\n", devmgr);
    if (devmgr != NULL) {
        if (popen2_s(&devmgr_proc, devmgr) == 0) {
            const char* procname = "devmgr";
            ///@todo extract command name from call string.
            //char procname[32];
            //cmd_getname(procname, devmgr, 32);

            if (cmdtab_add(&main_cmdtab, procname, (void*)&cmd_devmgr, NULL) != 0) {
                fprintf(stderr, "Err: command %s could not be added to command table.\n", procname);
                cli.exitcode = -2;
                goto otdb_main_TERM2;
            }
            
            appdata.devmgr = &devmgr_proc;
        }
        else {
            fprintf(stderr, "Err: \"%s\" could not be started.\n", devmgr);
            cli.exitcode = -2;
            goto otdb_main_TERM2;
        }
    }
    DEBUG_PRINTF("--> done\n");
    
    /// Initialize command search table.  
    ///@todo in the future, let's pull this from an initialization file or
    ///      something dynamic as such.
    DEBUG_PRINTF("Initializing commands ...\n");
    cmd_init(&main_cmdtab, xpath);
    DEBUG_PRINTF("--> done\n");
   
    /// Initialize DTerm data objects
    /// Non intrinsic dterm elements (cmdtab, devmgr, ext, tmpl) get attached
    /// following initialization
    DEBUG_PRINTF("Initializing DTerm ...\n");
    if (dterm_init(&dterm_handle, &appdata, intf_val) != 0) {
        cli.exitcode = -2;
        goto otdb_main_TERM2;
    }
    DEBUG_PRINTF("--> done\n");

    /// Open DTerm interface & Setup DTerm threads
    /// If sockets are not used, by design socket_path will be NULL.
    DEBUG_PRINTF("Opening DTerm on %s ...\n", socket);
    dterm_fn = dterm_open(&dterm_handle, socket);
    if (dterm_fn == NULL) {
        cli.exitcode = -2;
        goto otdb_main_TERM1;
    }
    DEBUG_PRINTF("--> done\n");
    
    DEBUG_PRINTF("Finished otdb startup\n");
    
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
    pthread_create(&thr_dterm, NULL, dterm_fn, (void*)&dterm_handle);
    DEBUG_PRINTF("Finished creating threads\n");
   
    /// Threads are now running.  
    /// If there is an archive supplied as argument, open it.
//    if (archive != NULL) {
//        uint8_t dstbuf[80];
//        int     srcsize;
//        int     cmd_rc;
//        srcsize = (int)strlen(archive);
//        cmd_rc  = cmd_open(&dterm_handle, dstbuf, &srcsize, (uint8_t*)archive, sizeof(dstbuf));
//        if (cmd_rc != 0) {
//            fprintf(stderr, "Err: open %d: Archive \"%s\" could not be opened.\n", cmd_rc, archive);
//        }
//        else {
//            VERBOSE_PRINTF("Archive \"%s\" opened.\n", archive);
//        }
//    }
    
    if (initfile != NULL) {
        if (dterm_cmdfile(&dterm_handle, initfile) < 0) {
            fprintf(stderr, ERRMARK"Could not run initialization file.\n");
        }
    }
    
    /// The rest of the main() code, below, is blocked by pthread_cond_wait() 
    /// until the kill_cond is sent by one of the child threads.  This will 
    /// cause the program to quit.
    pthread_mutex_lock(&cli.kill_mutex);
    pthread_cond_wait(&cli.kill_cond, &cli.kill_mutex);
    
    ///@todo clump this with dterm_deinit()
    DEBUG_PRINTF("Cancelling Theads\n");
    pthread_detach(thr_dterm);
    pthread_cancel(thr_dterm);
   
    otdb_main_TERM1:
    ///@todo OTFS freeing procedure might be best to do internally... hard to say
    DEBUG_PRINTF("Freeing OTFS\n");
    if (dterm_handle.ext->db != NULL) {
        otfs_deinit(dterm_handle.ext->db, &sub_tfree);
    }
    
    DEBUG_PRINTF("Freeing dterm\n");
    dterm_deinit(&dterm_handle);
 
 
    otdb_main_TERM2:
    DEBUG_PRINTF("Freeing cmdtab\n");
    cmdtab_free(&main_cmdtab);
    
    
    otdb_main_TERM3:
    DEBUG_PRINTF("Destroying threading objects\n");
    pthread_mutex_unlock(&cli.kill_mutex);
    pthread_mutex_destroy(&cli.kill_mutex);
    pthread_cond_destroy(&cli.kill_cond);
    
    // cli.exitcode is set to 0, unless sigint is raised.
    DEBUG_PRINTF("Exiting cleanly and flushing output buffers\n");
    fflush(stdout);
    fflush(stderr);
    
    VERBOSE_PRINTF("otdb exiting (%i)\n", cli.exitcode);
    return cli.exitcode;
}



void sub_json_loadargs(cJSON* json, int* debug_val, int* verbose_val, int* intf_val, char** socket, char** initfile, char** devmgr, char** xpath) {

#   define GET_STRINGENUM_ARG(DST, FUNC, NAME) do { \
        arg = cJSON_GetObjectItem(json, NAME);  \
        if (arg != NULL) {  \
            if (cJSON_IsString(arg) != 0) {    \
                *DST = FUNC(arg->valuestring); \
            }   \
        }   \
    } while(0)

#   define GET_STRING_ARG(DST, NAME) do { \
        arg = cJSON_GetObjectItem(json, NAME);  \
        if (arg != NULL) {  \
            if (cJSON_IsString(arg) != 0) {    \
                size_t sz = strlen(arg->valuestring)+1; \
                DST = malloc(sz);   \
                if (DST != NULL) memcpy(DST, arg->valuestring, sz); \
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
    GET_STRING_ARG(*socket, "socket");
    GET_STRING_ARG(*initfile, "init");
    GET_STRING_ARG(*devmgr, "devmgr");
    GET_STRING_ARG(*xpath, "xpath");
    
    /// 2. Systematically get all of the individual arguments
    GET_BOOL_ARG(debug_val, "debug");
    GET_BOOL_ARG(verbose_val, "verbose");
}





 
