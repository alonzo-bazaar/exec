#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include <sys/wait.h>
void rebind(int dst, int src) {
    close(dst);
    dup2(src, dst);
    close(src);
}
void grab_pipe_stdout(int pipe[2]) {
    rebind(STDOUT_FILENO, pipe[1]);
    close(pipe[0]);
}
void grab_pipe_stdin(int pipe[2]) {
    rebind(STDIN_FILENO, pipe[0]);
    close(pipe[1]);
}

void die(int exit_code, const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
    fprintf(stderr, "errno: %s\n", strerror(errno));
    exit(exit_code);
}

int main(void)
{
    int pipe1[2];
    if(pipe(pipe1) == -1)
	die(EXIT_FAILURE, "could not open pipe1\n");

    pid_t f1 = fork();
    if (f1 < 0)
	die(EXIT_FAILURE, "failed to create first child!\n");
    else if (f1 == 0) {
	grab_pipe_stdout(pipe1);
        // execlp("ls", "ls", "-l", ".", (char *) NULL);
        // exit(-1);
	puts("hello (first) motherfucker");
	exit(0);
    }

    int pipe2[2];
    if(pipe(pipe2) == -1)
	die(EXIT_FAILURE, "could not open pipe2\n");

    pid_t f2 = fork();
    if (f2 < 0)
	die(EXIT_FAILURE, "failed to create second child!\n");
    if (f2 == 0) {
	grab_pipe_stdin(pipe1);
	grab_pipe_stdout(pipe2);
	write(STDOUT_FILENO, "hello (before)\n", 6);
	int c = fgetc(stdin);
	while(c != EOF) {
	    fputc(c, stdout);
	    c = fgetc(stdin);
	}
	printf("that said: hello (second) motherfucker\n");
	write(STDOUT_FILENO, "hello (after)\n", 6);
	exit(0);
    }

    pid_t f3 = fork();
    if (f3 < 0)
	die(EXIT_FAILURE, "failed to create third child!\n");
    if (f3 == 0) {
	printf("hello bitch\n");
     	// grab_pipe_stdin(pipe2);
	// execlp("cat", "cat", (char *) NULL);
	// exit(-1);
	char c;
	read(pipe2[0], &c, 1);
	// while(c != EOF) {
	//     fputc(c, stdout);
	//     c = fgetc(stdin);
	// }
     	printf("that said: hello (third) motherfucker\n");
     	exit(0);
    }

    close(pipe1[0]); close(pipe1[1]);
    close(pipe2[0]); close(pipe2[1]);
    int r1 = -1;
    int r2 = -1;
    int r3 = -1;
    waitpid(f1, &r1, 0);
    waitpid(f2, &r2, 0);
    waitpid(f3, &r3, 0);

    printf("process 1 exit status: %d\n", r1);
    printf("process 2 exit status: %d\n", r2);
    printf("process 3 exit status: %d\n", r3);
    return 0;
}
