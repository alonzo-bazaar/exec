#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

#define EXEC_IMPLEMENTATION_
#include "exec.h"

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
