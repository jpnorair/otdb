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
    int             exitcode;
    pthread_mutex_t kill_mutex;
    pthread_cond_t  kill_cond;
    char*           call_path;
} cli_struct;

cli_struct cli;





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
        ///@todo change to new info format
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
    
    struct sockaddr_un sockaddr;
    char sockbuf[100];
    int sockfd
    int rc;
    
       
    /// Initialize Thread Mutexes & Conds.
    assert( pthread_mutex_init(&cli.kill_mutex, NULL) == 0 );
    pthread_cond_init(&cli.kill_cond, NULL);

    /// Initialize the signal handlers for this process.
    /// These are activated by Ctl+C (SIGINT) and Ctl+\ (SIGQUIT) as is
    /// typical in POSIX apps.  When activated, the threads are halted and
    /// Otter is shutdown.
    cli.exitcode = EXIT_SUCCESS;
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
        
        if (pid < 0) {
            fprintf(stderr, "Failed to fork OTDB.\n");
            goto client_main_END;
        }
    
        /// Child Process per fork()
        if (pid == 0) { 
            close(otdb_stdout[0]);
            dup2(otdb_stdout[1], 1);
            execl("/bin/sh", "sh", "-c", otdbcall, NULL);
            perror("execl");
            exit(1);
        }
        
        otdb_stdout_fd = otdb_stdout[0];

        // Just waiting for OTDB to start-up
        sleep(1);
    }
    
    /// Connect to OTDB socket
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket error");
        cli.exitcode = -1;
        goto client_main_END;
    }

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = AF_UNIX;
    strncpy(sockaddr.sun_path, otdbpath, sizeof(sockaddr.sun_path)-1);

    if (connect(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) == -1) {
        perror("connect error");
        cli.exitcode = -1;
        goto client_main_END;
    }

    while (1) {
        if (i >= TEST_CMDS) {
            i = 0;
        }
        
        
        
    
        // Receive response from OTDB
        rc = recv(sockfd, sockbuf, sizeof(sockbuf), MSG_WAITALL);
        if (rc > 0) {
            fprintf(stdout, "OTDB sent %d bytes:\n%*s\n", rc, rc, sockbuf);
        }
        else {
            fprintf(stderr, "OTDB socket receive error %d\n", rc);
        }
        
        sleep(1);
        i++;
    }


    while ( (rc=read(STDIN_FILENO, sockbuf, sizeof(sockbuf))) > 0) {
        if (write(fd, sockbuf, rc) != rc) {
            if (rc > 0) fprintf(stderr,"partial write");
            else {
                perror("write error");
                exit(-1);
            }
        }
    }



    
    
    


    
    /// Threads are now running.  The rest of the main() code, below, is
    /// blocked by pthread_cond_wait() until the kill_cond is sent by one of 
    /// the child threads.  This will cause the program to quit.
    pthread_mutex_lock(&cli.kill_mutex);
    pthread_cond_wait(&cli.kill_cond, &cli.kill_mutex);
    
    client_main_END:
    free(otdbcall);
    
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






 
