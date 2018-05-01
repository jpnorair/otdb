//
//  ppio.c
//  otdb
//
//  Created by SolPad on 27/4/18.
//  Copyright © 2018 JP Norair. All rights reserved.
//

#include "ppio.h"

#include "pktlist.h"
#include "ppipe.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/poll.h>


typedef struct {
    pktlist_t*      pktlist;
    struct pollfd*  fdlist;
    int             num_fds;
} thread_listen_args_t;



void* thread_listen(void* args) {
    int test;
    thread_listen_args_t* largs = (thread_listen_args_t*)args;
    
    // This will defer thread cancellation to waits on poll().
    // It is important to prevent messing up the packet list, this way.
    // See man pages for more info on pthread_setcanceltype()
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &test);
    
    
    
    
    
    return NULL;
}



void ppio_listen_stop(ppio_listen_t* listen) {
    int rc;
    thread_listen_args_t* largs;
    
    if (listen != NULL) {
        rc = pthread_cancel(listen->thread);
        if (rc == 0) {
            pthread_join(listen->thread, NULL);
            
            largs = (thread_listen_args_t*)listen->args;
            if (largs != NULL) {
                for (int i=0; i<largs->num_fds; i++) {
                    if (largs->fdlist[i].fd >= 0) {
                        close(largs->fdlist[i].fd);
                    }
                }
                if (largs->fdlist != NULL) {
                    free(largs->fdlist);
                }
                largs->num_fds = 0;
            
                free(largs);
            }
        }
    }
}

//void ppio_listen_block(ppio_listen_t* listen) {
//}

int ppio_listen(ppio_listen_t* listen, pktlist_t* pktlist, ppipe_t* pipes) {
/// Thread that listens to all pipes in the list which are opened for reading.
/// 
    int rc;
    thread_listen_args_t* largs;
    
    if ((listen == NULL) || (pktlist == NULL) || (pipes == NULL)) {
        return -1;
    }

    listen->args = (void*)malloc( sizeof(thread_listen_args_t) );
    if (listen->args == NULL) {
        return -2;
    }

    // -------------------------------------------------------------
    // Done with Input Parameter Checks.  Now get the list of fds
    // -------------------------------------------------------------
    
    // largs = shortcut to (listen->args)
    largs = listen->args;
    
    // Get list of open fds for reading, from list of pipes
    // If there is negative output, this is an error.
    // If output is zero, simply there are no pipes to poll.
    largs->num_fds = ppipe_pollfds(pipes, &(largs->fdlist), (POLLIN | POLLNVAL | POLLHUP));
    if (largs->num_fds <= 0) {
        rc = -3;
        goto ppio_listen_END;
    }
    
    // -------------------------------------------------------------
    // Done with getting fds of open fifos. Start listening thread.
    // -------------------------------------------------------------
    
    rc = pthread_create(&(listen->thread), NULL, &thread_listen, largs);
    
    ppio_listen_END:
    if (rc != 0) {
        // Free allocated memory on error
        if ((largs->fdlist != NULL) && (largs->num_fds != 0)) {
            free(largs->fdlist);
            largs->num_fds = 0;
        }
        
        free(largs);
    }
    
    return rc;
}

