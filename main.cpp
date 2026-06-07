#include"exec.h"
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
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
