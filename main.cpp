#define EXEC_IMPLEMENTATION_
#include"exec.h"

int main(void) {
    exec(CMD("echo", "hello world")); 

    exec_parallel(CMDS(
                       CMD("echo", "hello world"),
                       CMD("echo", "fuck         youuuuuu"),
                       CMD("touch", "file.txt")
                      ));

    exec_parallel(CMDS(
                       CMD("ls", "-l", "pluto"),
                       CMD("echo", "souja boi\nin this hoe\nwatch me crank that"),
                       CMD("rm", "file.txt")
                      )); 

    return 0;
}
