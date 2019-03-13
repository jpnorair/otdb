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


///@todo way to reopen the childprocess if it exits when POPEN2_PERSISTENT is set


#define _READ 0
#define _WRITE 1

static pid_t sub_popen2(const char* cmdline, int* fd_tochild, int* fd_fromchild, unsigned int* flags);

static void sub_popen2_kill(pid_t pid, int fd_tochild, int fd_fromchild);



int popen2(childproc_t* childproc, const char* cmdline, unsigned int flags) {
    int rc;
    
    if ((childproc != NULL) && (cmdline != NULL)) {
        childproc->flags    = flags;
        childproc->pid      = sub_popen2(cmdline, &childproc->fd_writeto, &childproc->fd_readfrom, &childproc->flags);
        rc                  = (childproc->pid <= 0);
    }
    else {
        rc = -1;
    }
    
    return rc;
}


void popen2_kill(childproc_t* childproc) {
    if (childproc != NULL) {
        childproc->flags &= ~POPEN2_PERSISTENT;
        sub_popen2_kill(childproc->pid, childproc->fd_writeto, childproc->fd_readfrom);
    }
}


pid_t sub_popen2(const char* cmdline, int* fd_tochild, int* fd_fromchild, unsigned int* flags) {
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
        
        ///@todo use execvp, which requires parsing the args into an argv
        ///@note last argv[] must be NULL
        //e.g. execvp(argv[0], argv);
        
        execl("/bin/sh", "sh", "-c", cmdline, NULL);
        
        ///@todo If we get here, the process crapped-out.  In certain cases,
        /// we need to try to restart it.  Can have a way of counting seconds
        /// between crashes.  If it's crashing frequently, give-up
        
        ///@todo send a signal to the parent process, somehow, to notify it
        /// that process died in error.  This might be a good job for main()
        /// thread, actually.
        
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



void sub_popen2_kill(pid_t pid, int fd_tochild, int fd_fromchild) {
    close(fd_tochild);
    close(fd_fromchild);               // Is this needed?
    
    kill(pid, SIGINT);
    waitpid(pid, NULL, 0);
    kill(pid, 0);           // returns status, this is technically unnecessary
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
