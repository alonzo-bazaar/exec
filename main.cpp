#define EXEC_IMPLEMENTATION_
#include"exec.h"

int main(void) {
    command cmd1 = CMD("echo", "hello world");
    exec(cmd1); 

    command cmd2 = CMD("echo", "fuck    youuuuuuu");
    exec(cmd2); 

    free(exec_parallel(CMDS(
			    CMD("echo", "hello world"),
			    CMD("echo", "fuck         youuuuuu"),
			    CMD("touch", "file.txt")
			   )));

    free(exec_parallel(CMDS(
			    CMD("ls", "-l", "pluto"),
			    CMD("echo", "souja boi\nin this hoe\nwatch me crank that"),
			    CMD("rm", "file.txt")
			   ))); 

    free(exec_pipeline(CMDS(
			    CMD("ls", "-l", "."),
			    CMD("grep", "c$"),
			    CMD("xxd")
			   )));

    return 0;
}
