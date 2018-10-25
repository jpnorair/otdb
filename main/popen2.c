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

#include "popen2.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <sys/wait.h>



#define _READ 0
#define _WRITE 1

int popen2_s(childproc_t* childproc, const char* cmdline) {
    int rc;
    
    if ((childproc != NULL) && (cmdline != NULL)) {
        childproc->pid = popen2(cmdline, &childproc->fd_writeto, &childproc->fd_readfrom);
        rc = (childproc->pid > 0);
    }
    else {
        rc = -1;
    }
    
    return rc;
}


void popen2_kill_s(childproc_t* childproc) {
    if (childproc != NULL) {
        popen2_kill(childproc->pid, childproc->fd_writeto, childproc->fd_readfrom);
    }
}


pid_t popen2(const char* cmdline, int* fd_tochild, int* fd_fromchild) {
    pid_t pid;
    int pipe_stdin[2];
    int pipe_stdout[2];

    if ((pipe(pipe_stdin) != 0) || (pipe(pipe_stdout) != 0)) {
        return -1;
    }

    pid = fork();
    
    if (pid < 0) {
        /// Fork Failed
        return pid;
    }
    
    if (pid == 0) { 
        /// Child Process
        close(pipe_stdin[_WRITE]);
        dup2(pipe_stdin[_READ], _READ);
        close(pipe_stdout[_READ]);
        dup2(pipe_stdout[_WRITE], _WRITE);
        
        execl("/bin/sh", "sh", "-c", cmdline, NULL);
        perror("execl"); 
        exit(1);
    }
    
    if (fd_tochild == NULL) {
        close(pipe_stdin[_WRITE]);
    }
    else {
        *fd_tochild = pipe_stdin[_WRITE];
    }
    if (fd_fromchild == NULL) {
        close(pipe_stdout[_READ]);
    }
    else {
        *fd_fromchild = pipe_stdout[_READ];
    }
    
    return pid;
}



void popen2_kill(pid_t pid, int fd_tochild, int fd_fromchild) {
    close(fd_tochild);
    close(fd_fromchild);               // Is this needed?
    
    kill(pid, 0);
    waitpid(pid, NULL, 0);
    kill(pid, 0);              // Is this needed?
}




//#define TESTING
#ifdef TESTING
int main(void) {
    char buf[1000];
    popen2_t kid;
    popen2("tr a-z A-Z", &kid);
    write(kid.to_child, "testing\n", 8);
    close(kid.to_child);
    memset(buf, 0, 1000);
    read(kid.from_child, buf, 1000);
    printf("kill(%d, 0) -> %d\n", kid.child_pid, kill(kid.child_pid, 0)); 
    printf("from child: %s", buf); 
    printf("waitpid() -> %d\n", waitpid(kid.child_pid, NULL, 0));
    printf("kill(%d, 0) -> %d\n", kid.child_pid, kill(kid.child_pid, 0)); 
    return 0;
}
#endif
