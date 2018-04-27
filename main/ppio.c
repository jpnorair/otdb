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



void* ppio_listen(pktlist_t* pktlist, ppipe_t* pipes) {
/// Thread that listens to all pipes in the list which are opened for reading.
///
    //pthread_t*  pubthread;
    int i;
    int fds;
    int pollcode;
    
    sub_t** pubtable;
    struct pollfd *listen_fd;
    uint8_t* dbuf;
    
    if ((pktlist == NULL) || (pipes == NULL)) {
        goto ppio_listen_END;
    }

    // -------------------------------------------------------------
    // Done with Input Parameter Checks.  Now get the list of fds
    // -------------------------------------------------------------
    
    // Find pipes in pipelist that are for reading.
    
    
    // Malloc an array of fds. These fds go into the poll() call.
    listen_fd = malloc(sizeof(struct pollfd) * pub_args->topiclist->size);
    if (listen_fd == NULL) {
        goto ppio_listen_END;
    }
    
    
    pubtable = malloc(sizeof(sub_t*) * pub_args->topiclist->size);
    if (pubtable == NULL) {
        free(pubfd);
        goto ppio_listen_END;
    }
    dbuf = malloc(pub_args->arglist->max);
    if (dbuf == NULL) {
        free(pubfd);
        free(pubtable);
        goto ppio_listen_END;
    }

    pub = pub_args->topiclist->front;
    for (i=0, fds=0; i<pub_args->topiclist->size; i++) {

        if (pub == NULL) {
            break;
        }
        pub->ppd = ppipe_new("pub", (const char*)pub->topic, "rb");
        if (pub->ppd < 0) {
            fprintf(stderr, "Error: pub pipe could not be created for topic %s.  (Code %d)\n",
                    pub->topic, pub->ppd);
        }
        else {
            pubtable[fds]     = pub;
            pubfd[fds].fd     = open(ppipe_getpath(pub->ppd), O_RDONLY|O_NONBLOCK);
            pubfd[fds].events = (POLLIN | POLLNVAL | POLLHUP);
            fds++;
        }
        
        pub = pub->next;
    }

    while (publoop_on) {
        pollcode = poll(pubfd, fds, 1000);
        
        /// 0 = timeout, -1 = error of some kind, positive = data ready.
        if (pollcode == 0) {
            continue;
        }
     
        for (i=0; i<fds; i++) {
            if (pubfd[i].fd >= 0) {
                if (pubfd[i].revents & POLLNVAL) {
                    pubfd[i].fd = -1;
                }
                else if (pubfd[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    close(pubfd[i].fd); 
                    pubfd[i].fd = -1;
                }
                else if (pubfd[i].revents & POLLIN) {
                    uint8_t charbuf;
                    int     bufbytes;
                    
#                   if 0
                    read(pubfd[i].fd, &charbuf, 1);
                    if (charbuf == 0) {
                        bufbytes = pipe_getbinary(&pubfd[i], dbuf, pub_args->arglist->max);
                    }
                    else {
                        dbuf[0] = charbuf;
                        bufbytes = pipe_gettext(&pubfd[i], &dbuf[1], pub_args->arglist->max-1);
                    }
#                   else
                    bufbytes = pipe_gethex(&pubfd[i], dbuf, pub_args->arglist->max);
                    
#                   endif
                    if (bufbytes > 0) {
                        paho_pubonce(pubtable[i], dbuf, (size_t)bufbytes); 
                    }
                    else {
                        // close pipe on error or timeout
                        close(pubfd[i].fd); 
                        pubfd[i].fd = -1;
                    }
                }
            }
        }
    }
    
    // Close remaining pipes
    for (i=0; i<fds; i++) {
        if (pubfd[i].fd >= 0) {
            close(pubfd[i].fd);
        }
    }

    free(pubfd);
    free(pubtable);
    free(dbuf);

    /// This occurs on thread death
    ppio_listen_END:
    return NULL;
}

