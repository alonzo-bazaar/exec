#ifndef EXEC_H_
#define EXEC_H_

#ifdef __cplusplus
#include<vector>
#include<string>

extern "C" {
#endif

#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<assert.h>
#include<string.h>
#include<errno.h>

// for the various #if defined(...) used to distinguish target platform see:
// https://stackoverflow.com/questions/7063303/macro-unix-not-defined-in-macos-x
// https://web.archive.org/web/20160306052035/http://nadeausoftware.com/articles/2012/01/c_c_tip_how_use_compiler_predefined_macros_detect_operating_system

#if defined(unix) || defined(__unix__) || defined(__unix) || defined(__linux__)
#include<unistd.h>
#include<sys/wait.h>
#define NL "\n"

#elif defined(_WIN32)
#include<windows.h>
#define NL "\r\n"

#else
#error "unsupported platform!"
#endif

// debugging utilities
#ifdef PRINT_DBG
#define dbg(msg)fprintf(stderr, "[DEBUG] "msg NL)
#define dbgf(msg, ...)fprintf(stderr, "[DEBUG] "msg NL, __VA_ARGS__ )
#else
#define dbg(...) (void)0
#define dbgf(...) (void)0
#endif

// exec's actual api
struct command;
typedef struct command command;
struct command_arr;
typedef struct command_arr command_arr;

int exec(command cmd);
int* exec_parallel(command_arr cmds);
int* exec_pipeline(command_arr cmds);

#ifdef __cplusplus
}
#endif

// we want this to work both as a c and c++ file
// problem is, the intersection between c and c++ is what I'd like to call
// "ungodly fucking unuseable c with no decent feature that must malloc every damn thing"
// so a lot of behavious was duplicated between the c and c++ version
// with some macro abuse provided to unify the c and c++ interfaces
#ifdef __cplusplus
#define STRING std::string
char* GETSTR(const std::string& s) {
    char* c_str = (char*)calloc(s.size()+1, sizeof(char));
    strcpy(c_str, s.c_str());
    return c_str;
}
const char* GETSTR_CONST(const std::string& s) {
    return s.c_str();
}

struct command {
    const size_t argc;
    std::vector<STRING> argv;
    command(std::initializer_list<STRING> args):argc(args.size()),argv(args){}
};
struct command_arr {
    const size_t size;
    const std::vector<command> cmds;
    command_arr(std::initializer_list<command> args):size(args.size()),cmds(args){}
};
#define CMD(...) command({__VA_ARGS__})
#define CMDS(...) command_arr({__VA_ARGS__})

#else
#define STRING char*
#define GETSTR(s) s
#define GETSTR_CONST(s) s
typedef struct command {
    const size_t argc;
    STRING* argv;
} command;
typedef struct command_arr {
    const size_t size;
    const command* cmds;
} command_arr;
#define CMD(...) ((command){                    \
            .argc = ARG_COUNT(__VA_ARGS__),     \
            .argv = (char*[]){ __VA_ARGS__ },   \
        })
#define CMDS(...) ((command_arr){               \
            .size = ARG_COUNT(__VA_ARGS__),     \
            .cmds = (command[]){ __VA_ARGS__ }  \
        })

// ARG_COUNT defined below: preprocessor abuse adapted from
// https://groups.google.com/g/comp.std.c/c/d-6Mj5Lko_s?pli=1
// found in the second anser to
// https://stackoverflow.com/questions/2124339/c-preprocessor-va-args-number-of-arguments
// 
// from 1 to 19 arguments, this macro will return the number of arguments
// passed to the macro
#define ARG_COUNT(...) EXTRACT_20TH_IN                                  \
    (__VA_ARGS__, 19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)
// if __VA_ARGS__ has 1 element __VA_ARGS__, 19, 18, ... will have 20 args
// and the 20th will be 1
//
// with 2 argument the sequence will have 21 args
// and the 20th will be 2
//
// with 3 argument the sequence will have 22 args
// and the 20th will be 3
//
// and so on
// 
// to find the  20th argument in __VA_ARGS__ we occupy the first 19 args
// with dummy arguments, bind the 20th arg to N, ignore everything else
// and return N
#define EXTRACT_20TH_IN(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10,        \
                        _11, _12, _13, _14, _15, _16, _17, _18, _19,    \
                        N, ...) N
#endif

#if defined(unix) || defined(__unix__) || defined(__unix)
pid_t spawn(command cmd) {
    if(cmd.argc == 0)
        return -1;

    const char* const child_prog = GETSTR_CONST(cmd.argv[0]);
    char** child_args = (char**)calloc(cmd.argc+1,sizeof(char*));
    for(size_t i = 0; i<cmd.argc; ++i)
        child_args[i] = GETSTR(cmd.argv[i]);
    child_args[cmd.argc] = (char*)NULL;

    pid_t pid = fork();
    if(pid == -1) {
        fprintf(stderr, "[ERROR] failed to create child process!\n");
        fprintf(stderr, "[ERROR] %s\n", strerror(errno));
#ifdef __cplusplus
        // c++ GETSTR must copy the string passed to it since std::string::c_str
        // returns const char* and execve needs its argument be non const
        for(size_t i = 0; i<cmd.argc; ++i) free(child_args[i]);
#endif
        free(child_args);
        return -1;
    }
    else if(pid != 0) {
        dbg("[PARENT] child created succesfully...");
#ifdef __cplusplus
        for(size_t i = 0; i<cmd.argc; ++i) free(child_args[i]);
#endif
        free(child_args);
        return pid;
    } else {
        dbg("[CHILD] running...");
        execvp(child_prog, child_args);
        dbg("[CHILD] I fucked up...");
        exit(-1);
    }
}

/*
 * runs one command and blocks until command is done
 * returns exit code of command, or -1 if a failure occured
 */
int exec(command cmd) {
    pid_t child_pid = spawn(cmd);
    if(child_pid == -1)
        return -1;

    int child_ret = -1;
    pid_t p = waitpid(child_pid, &child_ret, 0);
    if(p == -1) {
        fprintf(stderr, "[ERROR] failure while waiting on child process!\n");
        fprintf(stderr, "[ERROR] %s\n", strerror(errno));
        return -1;
    }
    dbgf("[PARENT] returned %d", child_ret);
    return child_ret;
}

/*
 * runs a bunch of commands in parallel and waits for all the commands to finish
 * returns malloc'd array the length of cmds.size with the return
 * codes of all spawned commands appearing in the same order as they did in the
 * command array passed to the function
 */
int* exec_parallel(command_arr cmds) {
    pid_t* children_pid = (pid_t*)calloc(cmds.size, sizeof(pid_t));
    int* children_ret = (int*)calloc(cmds.size+1, sizeof(int));
    memset(children_ret, 0, cmds.size*sizeof(int));
    memset(children_pid, 0, cmds.size*sizeof(pid_t));

    // spawn all kids
    for(size_t i = 0; i<cmds.size; ++i) {
        pid_t child_pid = spawn(cmds.cmds[i]);
        if(child_pid != -1)
            children_pid[i] = child_pid;
        else {
            // we're still in parent, but -1 return means child creation has failed
            // free all resources we have acquired so far and return error
            // wait on all previous child processes to avoid zombies
            int unused = -1;
            for(size_t j = 0; j<i; ++j) {
                pid_t p = waitpid(children_pid[i], &unused, 0);
                if(p == -1) {
                    fprintf(stderr, "[ERROR] failure while waiting on child process!\n");
                    fprintf(stderr, "[ERROR] %s\n", strerror(errno));
                }
            }
            free(children_ret);
            return NULL;
        }
    }

    // wait on all kids and return array of exit statuses
    for(size_t i = 0; i<cmds.size; ++i) {
        pid_t p = waitpid(children_pid[i], &(children_ret[i]), 0);
        if(p == -1) {
            fprintf(stderr, "[ERROR] failure while waiting on child process!\n");
            fprintf(stderr, "[ERROR] %s\n", strerror(errno));
            children_ret[i] = -1;
        }
    }
    return children_ret;
}

/*
 * runs a bunch of commands in parallel and waits for all the commands to finish
 * returns malloc'd NULL terminated array the length of cmds.size with the return
 * codes of all spawned commands appearing in the same order as they did in the
 * command array passed to the function
 */
// int* exec_pipeline(command_arr cmds) {
//     
//     assert(0&&"TODO");
// }

#elif defined(_WIN32)
int exec(command cmd) {
    (void)cmds;
    assert(0 && "TODO");
}
int* exec_parallel(command_arr cmds) {
    (void)cmds;
    assert(0 && "TODO");
}

int* exec_pipeline(command_arr cmds) {
    (void)cmds;
    assert(0 && "TODO");
}
#endif

#endif // EXEC_H_
