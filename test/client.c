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
  * @date       10 Sept 2018
  * @brief      OTDB Client main() function for testing purposes
  * @defgroup   OTDB_cli
  * @ingroup    OTDB_cli
  * 
  * OTDB Client is a test program to use with OTDB for the sole purpose of 
  * testing the client functions.
  *
  * The client functions are part of libotdb, and they should be linked into
  * applications that use OTDB.
  *
  ******************************************************************************
  */

// Top Level Configuration Header
#include <otdb_cfg.h>

// HBuilder Package Libraries
#include <argtable3.h>


// Standard C & POSIX Libraries
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <assert.h>



// Command Line Interface (CLI) data features.
typedef struct {
    int exitcode;
    bool run;
} cli_struct;

cli_struct cli;



/** Test Commands <BR>
  * ========================================================================<BR>
  */
  
#define TEST_CMDS   10
const char* cmd1    = "test command 1";
const char* cmd2    = "test command 2";
const char* cmd3    = "test command 3";
const char* cmd4    = "test command 4";
const char* cmd5    = "test command 5"; 
const char* cmd6    = "test command 6";
const char* cmd7    = "test command 7";
const char* cmd8    = "test command 8";
const char* cmd9    = "test command 9";
const char* cmd10   = "test command 10";  

const char* otdbcmd[TEST_CMDS] = \
    { cmd1, cmd2, cmd3, cmd4, cmd5, cmd6, cmd7, cmd8, cmd9, cmd10 };



/** Local Functions <BR>
  * ========================================================================<BR>
  */
  
static int client_main(const char* otdbpath, const char* otdbsocket); 






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
    cli.exitcode    = EXIT_FAILURE;
    cli.run         = false;
}

static void sigquit_handler(int sigcode) {
    cli.exitcode    = EXIT_SUCCESS;
    cli.run         = false;
}






// notes:
// 0. ch_inc/ch_dec (w/ ?:) and history pointers can be replaced by integers and modulo
// 1. command history is glitching when command takes whole history buf (not c string)
// 2. delete and remove line do not work for multiline command


int main(int argc, char* argv[]) {
    struct arg_file *otdb    = arg_file1(NULL, NULL, "otdb-path",       "Path to OTDB server program");
    struct arg_file *socket  = arg_file0(NULL, NULL, "socket",          "Path/Address to OTDB server socket");
    struct arg_lit  *help    = arg_lit0(NULL,"help",                    "print this help and exit");
    struct arg_lit  *version = arg_lit0(NULL,"version",                 "print version information and exit");
    struct arg_end  *end     = arg_end(10);
    
    void* argtable[]        = { otdb, socket, help, version, end };
    const char* progname    = "otdbcli";
    int exitcode            = 0;
    char* otdbsocket        = NULL;
    char* otdbpath          = NULL;
    bool bailout            = true;
    int nerrors;
    
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
        printf("%s -- %s\n", OTDB_PARAM_VERSION, OTDB_PARAM_DATE);
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

    /// special case: with no command line options induces brief help 
    if (argc==1) {
        printf("Try '%s --help' for more information.\n",progname);
        exitcode = 0;
        goto main_FINISH;
    }
    
    /// Get OTDB socket path/address.  This is mandatory.
    if (otdb->count != 0) {
        size_t sz   = strlen(otdb->filename[0] + 1);
        otdbpath    = calloc(sz, sizeof(char));
        if (otdbpath == NULL) {
            goto main_FINISH;
        }
        strcpy(otdbpath, otdb->filename[0]);
    }
    if (socket->count != 0) {
        size_t sz   = strlen(socket->filename[0] + 1);
        otdbsocket  = calloc(sz, sizeof(char));
        if (otdbsocket == NULL) {
            goto main_FINISH;
        }
        strcpy(otdbsocket, socket->filename[0]);
    }
    
    /// All configuration is done.
    /// Send all configuration data to program main function.
    bailout = false;
    
    main_FINISH:
    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
    
    if (bailout == false) {
        exitcode = otdb_main((const char*)otdbpath, (const char*)otdbsocket);
    }
    if (otdbpath != NULL) {
        free(otdbpath);
    }
    if (otdbsocket != NULL) {
        free(otdbsocket);
    }

    return exitcode;
}





int client_main(const char* otdbpath, const char* otdbsocket) {    
/// 1. Open a socket on the supplied socket path.
/// 2. Send a sequence of requests to OTDB
/// 3. Loop until Ctrl+C
    int     otdb_stdout[2];
    int     otdb_stdout_fd;
    pid_t   otdbpid;
    char*   otdbcall;
    void*   otdb;
    
    struct sockaddr_un sockaddr;
    char sockbuf[100];
    int sockfd
    int rc;
    
    /// Initialize the signal handlers for this process.
    /// These are activated by Ctl+C (SIGINT) and Ctl+\ (SIGQUIT) as is
    /// typical in POSIX apps.  When activated, the threads are halted and
    /// Otter is shutdown.
    cli.run         = true;
    cli.exitcode    = EXIT_SUCCESS;
    sub_assign_signal(SIGINT, &sigint_handler);
    sub_assign_signal(SIGQUIT, &sigquit_handler);
    
    /// Start OTDB as a child process
    /// wait 1 second for otdb to start up completely (this is overkill)
    ///@todo spawn a local thread for relaying reads from the OTDB stdout
    {   const char* otdbargs   = "-i socket -S ";
        const char* def_socket = "./otdbsocket";
        
        char* cursor;
        size_t call_len;
        
        if (otdbsocket == NULL) {
            otdbsocket = def_socket;
        }
        
        call_len    = strlen(otdbpath) + 1 + strlen(otdbargs) + strlen(otdbsocket) + 1;
        otdbcall    = calloc(call_len, sizeof(char));
        cursor      = stpcpy(otdbcall, otdbpath);
        *cursor++   = ' ';
        cursor      = stpcpy(cursor, otdbargs);
        cursor      = stpcpy(cursor, otdbsocket);
        *cursor++   = 0;
        
        fprintf(stdout, "Opening OTDB on: %s\n", otdbcall);
        
        if (pipe(otdb_stdout) != 0) {
            fprintf(stderr, "Failed to open stdout pipe for OTDB.\n");
            goto client_main_END;
        }
        
        otdbpid = fork();
        
        if (otdbpid < 0) {
            fprintf(stderr, "Failed to fork OTDB.\n");
            goto client_main_END;
        }
    
        /// Child Process per fork()
        if (otdbpid == 0) { 
            close(otdb_stdout[0]);
            dup2(otdb_stdout[1], 1);
            execl("/bin/sh", "sh", "-c", otdbcall, NULL);
            perror("execl");
            exit(2);
        }
        
        otdb_stdout_fd = otdb_stdout[0];

        // Just waiting for OTDB to start-up
        sleep(1);
    }
    
    /// Initialize OTDB Client (socket)
    otdb = otdb_init(AF_UNIX, otdbpath);
    if (otdb == NULL) {
        ///@todo error message
        goto client_main_TERM1;
    }
    
    // Set device ID to a certain value known ahead of time.
    rc = otdb_setdevice(otdb, 2);
    
    // Do a bunch of commands, one at a time
    while (cli.run == true) {
        if (i >= TEST_CMDS) {
            i = 0;
        }
        
        // Run different commands based on value of i
        switch (i) {
        case 0: rc = otdb_readall(otdb, otdb_filehdr_t* output_hdr, otdb_filedata_t* output_data, 1, BLOCK_isf, 0, 0, -1);
                break;
                
        case 1: {
                uint8_t testdata[2] = { 0xAA, 0xBB };
                rc = otdb_writedata(otdb, 1, BLOCK_isf, 0, 2, 2, testdata);
            } break;
                
        case 2: rc = otdb_read(otdb, otdb_filedata_t* output_data, 1, BLOCK_isf, 0, 2, -1);
                break;
                
        case 3: rc = otdb_restore(otdb, 1, BLOCK_isf, 0);
                break;
                
        case 4: rc = otdb_read(otdb, otdb_filedata_t* output_data, 1, BLOCK_isf, 0, 2, -1);
                break;
                
        case 5: //rc =
                break;
                
        case 6: //rc = 
                break;
                
        case 7:
        case 8:
        case 9:
        
        default: goto client_main_TERM2;
        }
        
        sleep(1);
        i++;
    }

    client_main_TERM2:
    otdb_deinit(otdb);
    
    /// Kill OTDB
    client_main_TERM1:
    kill(otdbpid, SIGQUIT);
    sleep(1);

    /// Free memory elements
    client_main_END:
    free(otdbcall);
    
    // cli.exitcode is set to 0, unless sigint is raised.
    printf("Exiting cleanly and flushing output buffers\n");
    fflush(stdout);
    fflush(stderr);
    
    return cli.exitcode;
}






 
