/* Copyright 2017, JP Norair
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

#include "ppipe.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fcntl.h>
#include <errno.h>
#include <assert.h>



// Must be a power of two
#define PPIPE_GROUP_SIZE    16


#define PPIPE_BASEPATH      "./pipes/"


//static ppipe_t ppipe = {
//    .basepath   = { 0 },
//    .fifo       = NULL,
//    .num        = 0
//};




int sub_assure_path(char* assure_path, mode_t mode) {
    char* p;
    char* file_path = NULL;
    //size_t pathlen;
    int rc          = 0;

    assert(assure_path && *assure_path);

    //pathlen     = strlen(assure_path);
    //file_path   = malloc(pathlen+1);
    //if (file_path == NULL) {
    //    return -2;
    //}
    
    ///@note Originally used strcpy, but bizarre side-effects were observed
    /// on assure_path, in certain cases.  Adding manual terminators.
    //strncpy(file_path, assure_path, pathlen);
    //file_path[pathlen]      = 0;
    //assure_path[pathlen]    = 0;
    
    /// Now not using copying at all.  strcpy/strncpy were being chaotic.
    file_path = assure_path;
    
    for (p=strchr(file_path+1, '/'); p!=NULL; p=strchr(p+1, '/')) {
        *p='\0';
        if (mkdir(file_path, mode) == -1) {
            if (errno!=EEXIST) { 
                *p='/'; 
                rc = -1; 
                goto sub_assure_path_END;
            }
        }
        *p='/';
    }
    
    sub_assure_path_END:
    
    //free(file_path);
    return rc;
}





int ppipe_init(ppipe_t* pipes, const char* basepath) {
/// pipes input parameter must be allocated by caller.
    const char* def_basepath = PPIPE_BASEPATH;
    size_t alloc_size;

    if (pipes == NULL) {
        return -1;
    }
    if (strlen(basepath) > 255) {
        return -2;
    }
    
    pipes->basepath[0]  = 0;
    pipes->fifo         = NULL;
    pipes->num          = 0;

    /// Set basepath, either to default or supplied path.
    if (basepath == NULL) {
        basepath = def_basepath;
    }
    strcpy(pipes->basepath, basepath);

    /// Malloc the first group of pipe files
    alloc_size  = PPIPE_GROUP_SIZE * sizeof(ppipe_fifo_t);
    pipes->fifo = malloc(alloc_size);
    if (pipes->fifo == NULL) {
        return -3;
    }
    memset(pipes->fifo, 0, alloc_size);
    
    return 0;
}



void ppipe_deinit(ppipe_t* pipes) {
/// Go through all pipes, close, delete, free data

    if (pipes != NULL) {
        for (int i=0; i<pipes->num; i++) {
            ppipe_del(pipes, i);
        }
        
        pipes->basepath[0]  = 0;
        pipes->num          = 0;
        if (pipes->fifo != NULL) {
            free(pipes->fifo);
        }
    }
}



int ppipe_new(ppipe_t* pipes, const char* prefix, const char* name, const char* fmode) {
    ppipe_fifo_t*   fifo;
    int             ppd;
    size_t          alloc_size;
    int             test_fd;
    struct stat     st;
    const char*     null_prefix = "";
    size_t          prefix_len;
    int             group;
    int             open_mode, file_test;  
    char*           file_path;
    
    if ((pipes == NULL) || (name == NULL) || (fmode == NULL)) {
        return -1;
    }

    // Derive file modes based on fmode input.
    // The fmode input must be "r", "r+", "w", or "w+", just like posix standards.
    prefix_len  = strlen(fmode);
    if ((prefix_len < 1) || (prefix_len > 2)) {
        return -4;
    }
    if (prefix_len == 1) {
        if (fmode[0] == 'r') {
            open_mode   = O_RDONLY;
        }
        else if (fmode[0] == 'w') {
            open_mode   = O_WRONLY;
        }
        else {
            return -4;
        }
    }
    else if (fmode[1] == '+') {
        open_mode   = O_RDWR;
    }
    else {
        return -4;
    }

    // ------------------------------------------------------------------------
    // Done with basic input parameter checks, now check if the file is usable
    // ------------------------------------------------------------------------

    // Derive File Path from inputs
    // The "+2" is for the intermediate '/' and the null terminator
    if (prefix == NULL) {
        prefix = null_prefix;
    }
    prefix_len  = strlen(prefix);
    alloc_size  = strlen(pipes->basepath) + prefix_len + strlen(name) + 2;
    file_path   = malloc(alloc_size);
    memset(file_path, 0, alloc_size);
    strcpy(file_path, pipes->basepath); 
    if (prefix_len != 0) {
        strcat(file_path, prefix); 
        strcat(file_path, "/");
    }
    strcat(file_path, name);
    
    // Determine if file already exists.  
    // If already exists, then make sure the open modes are compatible.
    // If already exists and compatible, then we are done
    // If not already exists, then create it (and open).
    file_test = access(file_path, F_OK);
    if (file_test != -1) {
        test_fd     = open(file_path, open_mode | O_NONBLOCK);
        file_test   = fstat(test_fd, &st);
        close(test_fd);
        
        if ((file_test == 0) && S_ISFIFO(st.st_mode) && ((st.st_mode&0777)==0666)) {
            //printf("File already exists, is fifo, and has proper mode: continuing.\n");
        }
        else {
            //printf("File already exists, is not FIFO and has improper mode: exiting.\n");
            goto ppipe_new_FIFOERR;
        }
    }
    else {
        // FIFO does not exist, so create it.
        // We create with Read and Write, because always one side writes, one side reads.
        umask(0);
        file_test = sub_assure_path(file_path, 0755);
        if (file_test == 0) {
            file_test = mkfifo(file_path, 0666);
        }
        if (file_test != 0) {
            goto ppipe_new_FIFOERR;
        }
    }
    
    // ----------------------------------------------------------
    // Done with File checks, now add FIFO to array of pipes
    // ----------------------------------------------------------
    
    // This code block is for re-allocating the fifo array if it is 
    // too small to fit another pipe.
    group = pipes->num & (PPIPE_GROUP_SIZE-1);
    
    if ((pipes->num != 0) && (group == 0)) {
        size_t elem = (pipes->num / PPIPE_GROUP_SIZE) + 1;
        alloc_size  = PPIPE_GROUP_SIZE * sizeof(ppipe_fifo_t);
        pipes->fifo = realloc(pipes->fifo, elem*alloc_size);
        memset(&pipes->fifo[pipes->num], 0, alloc_size);
    }
    if (pipes->fifo == NULL) {
        ppd = -6;   // Error: cannot add to allocation array.
    }
    else {
        ppd = (int)pipes->num;
        pipes->num++;
        fifo = &pipes->fifo[ppd];
        if (fifo->fpath != NULL) {
            ppipe_del(pipes, ppd);  // Delete leftover data if exists
        }
        fifo->fpath = file_path;
        fifo->fd    = -1;
        fifo->fmode = open_mode;
    }

    return ppd;
    
    ppipe_new_FIFOERR:
    fprintf(stderr, "Could not make FIFO \"%s\": code=%d\n", file_path, -5);
    free(file_path);
    return -5;
}




int ppipe_del(ppipe_t* pipes, int ppd) {
    ppipe_fifo_t* fifo;
    size_t i = (size_t)ppd;
    
    if (i >= pipes->num) {
        return -1;
    }
    fifo = &pipes->fifo[i];
    if (fifo == NULL) {
        return -2;
    }
    
    if (i == (pipes->num-1)) {
        pipes->num--;
    }
    
    if (fifo->fpath != NULL) {
        int rc;
        rc = remove(fifo->fpath);
        if (rc != 0) {
            fprintf(stderr, "Could not remove FIFO \"%s\": code=%d\n", fifo->fpath, rc);
        }
        
        free(fifo->fpath);
        fifo->fpath = NULL;
        fifo->fmode = 0;
    }
    
    return 0;
}



int ppipe_searchmode(ppipe_t* pipes, ppipe_fifo_t** dst, size_t listmax, int fmode) {
/// Build list of ppd's based on the file mode.
/// Presently this is a linear search, which is probably the best impl as long
/// as the pipe list is not indexed on file mode.  Considering that this search
/// is done rarely (ideally just once, with ppio_listen()), this is fine.
///
    int         list_sz;
    int         ppd;
    
    if ((pipes == NULL) || (dst == NULL)) {
        return -1;
    }
    
    list_sz = 0;
    ppd     = 0;
    
    while ((ppd < pipes->num) && (list_sz < listmax)) {
        ppipe_fifo_t* fifo = &pipes->fifo[ppd];
        
        if ((fifo->fmode & fmode) == fmode) {
            list_sz++;
            dst[ppd] = fifo;
        }

        ppd++;
    }
    
    return list_sz;
}



int ppipe_searchname(ppipe_t* pipes, ppipe_fifo_t** dst, const char* prefix, const char* name) {
/// Linear search of ppipe array.  This will need to be replaced with an
/// indexed search at some point.
    int ppd;
    
    if (pipes == NULL) {
        return -1;
    }
    
    ppd = (int)pipes->num - 1;
    
    while (ppd >= 0) {
        ppipe_fifo_t* fifo = &pipes->fifo[ppd];
        int prefix_size = (int)strlen(prefix);
    
        if (strncmp(fifo->fpath, prefix, prefix_size) == 0) {
            if (strcmp(&fifo->fpath[prefix_size+1], name) == 0) {
                *dst = fifo;
                return ppd;
            }
        }
        ppd--;
    }
    
    *dst = NULL;
    return 0;
}


int ppipe_pollfds(ppipe_t* pipes, struct pollfd** pollfd_list, int poll_events) {
/// Client must free *pollfd_list!

    int burned_fds;
    int list_sz;
    ppipe_fifo_t** fifolist;
    
    if ((pollfd_list == NULL) || (pipes == NULL)) {
        return -1;
    }
    if (pipes->num == 0) {
        return 0;
    }
    
    fifolist = malloc(pipes->num * sizeof(ppipe_fifo_t*));
    if (fifolist == NULL) {
        return -2;
    }
    
    //-------------------------------------------------------------
    // Done with input checks.
    // fifolist will be freed at end of function.
    //-------------------------------------------------------------
    
    /// 1. Search for pipes that are meant for reading.
    ///    ppipe_searchmode() returns an array of fifo pointers into fifolist.
    ///    On error, return large negative value.
    list_sz = ppipe_searchmode(pipes, fifolist, pplist.num, O_RDONLY);
    if (list_sz < 0) {
        list_sz -= 10;
    }
    
    /// 2. Go through list of read-fifos and open any fifos that aren't already open.
    ///    Build a list of poll structs containing the relevant fd information.
    else if (list_sz > 0) {
        // try to open all the files for reading. Files already open are not re-opened.
        // Files that won't open get ignored.
        burned_fds = 0;
        for (int i=0; i<list_sz; i++) {
            if (fifolist[i]->fd < 0) {
                fifolist[i]->fd = open(fifolist[i]->fpath, O_RDONLY|O_NONBLOCK);
                if (fifolist[i]->fd < 0) {
                    burned_fds++;
                }
            }
        }
        
        // The output list size is adjusted to account for files that wouldn't open.
        // Now build the output list.  The client must free this list!!!
        list_sz -= burned_fds;
        if (list_sz > 0) {
            *pollfd_list = malloc(list_sz * sizeof(struct pollfd));
            if (*pollfd_list == NULL) {
                list_sz = -3;
            }
            else {
                for (int i=0, j=0; i<list_sz; j++) {
                    if (fifolist[j]->fd >= 0) {
                        (*pollfd_list)[i].fd        = fifolist[j]->fd;
                        (*pollfd_list)[i].events    = poll_events;
                        i++;
                    }
                }
            }
        }
    }
    
    free(fifolist);
    return list_sz;
}


FILE* ppipe_getfile(ppipe_t* pipes, int ppd) {
/// Not currently implemented.
/// I might delete this function, because we are working with file descriptors.
    size_t i = (size_t)ppd;

    if (i <= pipes->num) {
        return NULL;
    }
    
    return NULL;
    //return pipes->fifo[i].file;
}



const char* ppipe_getpath(ppipe_t* pipes, int ppd) {
    size_t i = (size_t)ppd;
    
    if (i <= pipes->num) {
        return NULL;
    }
    return pipes->fifo[i].fpath;
}


ppipe_t* ppipe_ref(ppipe_t* pipes) {
/// Deprecated Function
    return pipes;
}


void ppipe_print(ppipe_t* pipes) {
    printf("ppipe size=%zu, basepath=%s\n", pipes->num, pipes->basepath);

    for (int i=0; i<pipes->num; i++) {
        printf("fifo[%d].fd     = %d\n", i, pipes->fifo[i].fd);
        printf("fifo[%d].fpath  = %s\n", i, pipes->fifo[i].fpath);
    }
}


