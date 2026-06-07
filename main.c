#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

#include "exec.h"

// readapted from the wonderful fucking answer at
// https://stackoverflow.com/questions/79219926/how-to-write-to-stdin-of-another-process-in-c-on-linux

// forked children inherit the parent's file descriptors(?)
// even if opening/closing a file descriptor in a child does not affect the same
// file descriptor in the parent(?)
// meaning a parent can just call pipe, get two fds
// and tell the child, ight, this is your stdin, this is your stdout

#define LOG_ERRNO()					\
    fprintf(stderr, "errno is %d, meaning %s\n\n",	\
	    errno, strerror(errno));			\

int warn(int call_ret, const char* callexpr, int linenum, const char* filename) {
    if(call_ret == -1) {
	fprintf(stderr, "[WARNING] during call %s\n", callexpr);
	fprintf(stderr, "[WARNING] at line %d of file %s\n",
		linenum, filename);
	fprintf(stderr, "[WARNING] returned %d\n", call_ret);
	fprintf(stderr, "[WARNING] errno is %d, meaning %s\n\n",
		errno, strerror(errno));
    }
    return call;
}
#define WARN_ON_FAIL(call) warn(call, callexpr, __LINE__, __FILE__)

#define NANOSLEEP_NSECS(nsecs, ...)			\
    ORDIE(nanosleep(&(struct timespec){		\
		.tv_sec = 0,			\
		.tv_nsec = 1000*1000* 100,	\
	    }, NULL) __VA_ARGS__)

int close_pipe(int pipe[2]) {
    int c1 = WARN_ON_FAIL(close(pipe[1]));
    int c2 = WARN_ON_FAIL(close(pipe[0])); 
    if(c1 != -1 && c2 != -1) return 0; return -1;
}

// [0] = read end, [1] = write end
// bind stdin to pipe's out
// dup2(pipe[1], STDOUT_FILENO) readjusts stdout so it now poits to pipe[1]
// and closes the old file pointed to by stdout
// 
// then close pipe 'cause you don't need its fileno's after you bound stdout
// to file's out
int grab_pipe_stdout(int pipe[2]) {
    int d = WARN_ON_FAIL(dup2(pipe[1], STDOUT_FILENO));
    int c = WARN_ON_FAIL(close_pipe(pipe));
    if(c != -1 && d != -1) return 0; return -1;
}

// bind stdin to pipe's in
// then close pipe
int grab_pipe_stdin(int pipe[2]) {
    int c = WARN_ON_FAIL(dup2(pipe[0], STDIN_FILENO));
    int d = WARN_ON_FAIL(close_pipe(pipe));
    if(c != -1 && d != -1) return 0; return -1;
}

int* exec_pipeline(command_arr cmds) {
    if(cmds.size == 0) {
	fprintf(stderr, "pipeline have command of length zero!\n");
	return NULL;
    }
    if(cmds.size == 1) {
	int* ret = (int*)malloc(sizeof(int));
	*ret = exec(cmds.cmds[0]);
	return ret;
    }
    for(size_t i = 0; i<cmds.size; ++i)
	if(cmds.cmds[i].argc == 0) {
	    fprintf(stderr, "cannot have command of length zero!\n");
	    return NULL;
	}

    pid_t* children = (pid_t*)calloc(cmds.size, sizeof(pid_t));
    memset(children, 0, cmds.size*sizeof(pid_t));

    int* outpipe = (int*)calloc(2, sizeof(int));
    memset(outpipe, 0, 2*sizeof(int));
    int* inpipe = (int*)calloc(2, sizeof(int));
    memset(inpipe, 0, 2*sizeof(int));

    // initially error value, if all goes well we change it at the end
    // if all doesn't go well we return it unaltered to signal an error
    // to the caller
    int* res = NULL;

    for(size_t i = 0; i<cmds.size; ++i) {
	// (if not the last command) create output pipe for command
	// (future input pipe at next iteration)
	if(i != cmds.size-1)
	    if(WARN_ON_FAIL(pipe(outpipe)) == -1) {
		fprintf(stderr,
			"failed to create %zu-th pipe in pipeline\n", i);
		goto loop_error;
	    }

	pid_t p = WARN_ON_FAIL(fork());
	if(p == -1) {
	    fprintf(stderr,
		    "failed to create %zu-th child in pipeline\n", i);
	    goto loop_error;
	}

	if(p==0) {
	    // (if you're not the first process) get your input pipe
	    if(i != 0)
		if(WARN_ON_FAIL(grab_pipe_stdin(inpipe)) == -1) {
		    fprintf(stderr,
			   "child process failed to acquire standard input\n");
		    exit(EXIT_FAILURE);
		}
	    // (if you're not the last process) get your output pipe
	    if(i != cmds.size-1)
		if(WARN_ON_FAIL(grab_pipe_stdout(outpipe)) == -1) {
		    fprintf(stderr,
			    "child process failed to acquire standard output\n");
		    exit(EXIT_FAILURE);
		}

	    // exec what you gotta exec
	    command cmd = cmds.cmds[i];
	    const char* const prog = GETSTR_CONST(cmd.argv[0]);
	    char** args = (char**)calloc(cmd.argc+1,sizeof(char*));
	    for(size_t i = 0; i<cmd.argc; ++i)
		args[i] = GETSTR(cmd.argv[i]);
	    args[cmd.argc] = (char*)NULL;
	    WARN_ON_FAIL(execvp(prog, args));
	}
	else {
	    children[i] = p;
	    // if a process got its input pipe that pipe has run its course
	    // through this loop (for the parent/main process at least)
	    if(i != 0)
		if(WARN_ON_FAIL(close_pipe(inpipe)) == -1) {
		    fprintf(stderr,
			    "main process to close pipe after assigning it to child\n");
		    goto loop_error;
		}

	    int* tmp = outpipe;
	    outpipe = inpipe;
	    inpipe = tmp;
	}
	continue;

    loop_error:
	// teardown if this loop iteration was erroneous
	int unused = -1;
	for(size_t j = 0; j<i; ++j)
	    waitpic(hildren[i], &unused, 0);
	// close the other  pipe
	if(i != 0 && (close(inpipe[0]) == -1)) {
	    fprintf(stderr, "failed to close pipe during teardown\n");
	    LOG_ERRNO();
	}
	goto end;
    }

    res = (int*)calloc(cmds.size, sizeof(int));
    memset(rets, 0, cmds.size*sizeof(int));
    for(size_t i = 0; i<cmds.size; ++i)
	ORDIE(waitpid(children[i], res+i, 0));

 end:
    free(inpipe);
    free(outpipe);
    free(children);

    return res;
}

int main(void)
{
    int* ret = exec_pipeline(CMDS(
		       CMD("ls", "-l", "."),
		       CMD("grep", "c$"),
		       CMD("xxd")
		      ));
    printf("\n");
    for(size_t i = 0; i<2; ++i)
	printf("%d ", ret[i]);
    printf("\n");
    free(ret);
    return 0;
}

/*
int firsttry(void) {
    int pipe1[2]; ORDIE(pipe(pipe1), "failed to create pipe");

    // first fork
    pid_t f1 = fork(); ORDIE(f1, "failed to create first child");
    if (f1 == 0) {
	// this runs in the first child
	grab_pipe_stdout(pipe1);
        execlp("ls", "ls", "-l", ".", (char *) NULL);
	exit(1);
    }

    // second fork
    int pipe2[2]; ORDIE(pipe(pipe2), "failed to create pipe");
    pid_t f2 = fork(); ORDIE(f2, "failed to create second child");
    if (f2 == 0) {
	// this runs in the second child
	grab_pipe_stdin(pipe1);
	grab_pipe_stdout(pipe2);
        execlp("grep", "grep", "c$", (char *) NULL);
	exit(1);
    }

    // third fork
    close_pipe(pipe1);
    pid_t f3 = fork(); ORDIE(f3, "failed to create second child");
    if (f3 == 0) {
	grab_pipe_stdin(pipe2);
        execlp("xxd", "xxd", (char *) NULL);
	exit(1);
    }

    // pipeline teardown
    close_pipe(pipe2);

    int r1 = -1, r2 = -1, r3 = -1;
    int r = -1;
    ORDIE(waitpid(f1, &r1, 0));
    ORDIE(waitpid(f2, &r2, 0));
    ORDIE(waitpid(f3, &r3, 0));

    printf("process 1 exit status: %d\n", r1);
    printf("process 2 exit status: %d\n", r2);
    printf("process 3 exit status: %d\n", r3);
    return 0;
}
*/
