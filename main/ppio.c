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
#include <errno.h>

typedef struct {
    pktlist_t*      pktlist;
    struct pollfd*  fdlist;
    int             num_fds;
    size_t          rxbuf_size;
    uint8_t*        rxbuf;
} thread_listen_args_t;




void sub_thread_free(thread_listen_args_t* largs) {
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



void* thread_listen(void* args) {
    uint8_t bufactive[2048];     ///@todo should have max buffer get allocated from args
    uint8_t* cursor = bufactive;
    struct pollfd fdactive;
    size_t bytes_new;
    int bytes_limit;
    int test;
    
    int fds_ready;
    thread_listen_args_t* largs = (thread_listen_args_t*)args;
    
    // This will defer thread cancellation to waits on poll().
    // It is important to prevent messing up the packet list, this way.
    // See man pages for more info on pthread_setcanceltype()
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &test);
    
    while (1) {
        fds_ready = poll(largs->fdlist, largs->num_fds, -1);
        
        if (fds_ready < 0) {
            break;  // exit on poll error
        }

        for (int i=0; i<largs->num_fds; i++) {
        
            // read data from one open file into a packet
            if (largs->fdlist[i].revents & POLLIN) {
                bytes_limit = 2048;
                while (1) {
                    bytes_new       = read(largs->fdlist[i].fd, cursor, bytes_limit);
                    cursor         += bytes_new;
                    bytes_limit    -= bytes_new;
                    
                    // Data is stringified, so go until null terminator, and
                    // packetize the data upon termination
                    if (cursor[-1] == 0) {
                        
                        break;
                    }
                    
                    // Data is too long for the buffer, so cancel read.
                    if (bytes_limit <= 0) {
                        
                        break;
                    }
                    
                    // Wait for data to finish coming. There is a 10ms timeout.
                    // Anything other than a clean exit from poll() ends the read.
                    test = poll(&fdactive, 1, 10);
                    if ((test <= 0) || (fdactive.revents & (POLLERR|POLLHUP|POLLNVAL))) {
                        break;
                    }
                }
            }
            else if (largs->fdlist[i].revents & (POLLERR|POLLHUP|POLLNVAL)) {
                // handle file error condition
                
            }
        }
    }
    
    /// Handle poll() errors.  Could do more here.
    fprintf(stderr, "ERROR: poll() failure %d\n", errno);
    
    /// Termination routine:
    /// 1. Send alarm that thread has collapsed.
    /// 2. De-allocate Thread Arguments
    
    // send alarm

    sub_thread_free(largs);
    
    
    
    return NULL;
}



void ppio_listen_stop(ppio_listen_t* listen) {
    int rc;
    
    if (listen != NULL) {
        rc = pthread_cancel(listen->thread);
        if (rc == 0) {
            pthread_join(listen->thread, NULL);
            sub_thread_free((thread_listen_args_t*)listen->args);
        }
    }
}



void ppio_listen_block(ppio_listen_t* listen) {
    pthread_join(listen->thread, NULL);
}


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
    
    // malloc the rxbuffer: 
    ///@todo the size should be dynamic
    largs->rxbuf
    
    // Get list of open fds for reading, from list of pipes
    // If there is negative output, this is an error.
    // If output is zero, simply there are no pipes to poll.
    largs->num_fds = ppipe_pollfds(pipes, &(largs->fdlist), (POLLIN | POLLNVAL | POLLHUP));
    if (largs->num_fds <= 0) {
        rc = -4;
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

