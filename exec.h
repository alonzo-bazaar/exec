#ifndef EXEC_H_
#define EXEC_H_

#ifdef __cplusplus
#include<string>
#include<vector>
#include<algorithm> // std::transform

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

#define LOG_ERRNO()					\
    fprintf(stderr, "at line %d of file %s\n"			\
	    "errno is %d: meaning %s\n\n",			\
	    __LINE__, __FILE__, errno, strerror(errno));	\
int warn(int call_ret, const char* callexpr, int linenum, const char* filename);
#define WARN_ON_FAIL(call) warn(call, #call, __LINE__, __FILE__)
// for hunting eventual timing related heisenbugs
#define NANOSLEEP_NSECS(nsecs)			\
    WARN_ON_FAIL(nanosleep(&(struct timespec){	\
	    .tv_sec = 0,			\
	    .tv_nsec = 1000*1000* 100,		\
	})

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
#endif // EXEC_H_

// we want this to work both as a c and c++ file
// problem is, the intersection between c and c++ is what I'd like to call
// "ungodly fucking unuseable c with no decent feature that must malloc every damn thing"
// so a lot of behavious was duplicated between the c and c++ version
// with some macro abuse provided to unify the c and c++ interfaces

#ifdef EXEC_IMPLEMENTATION_
int warn(int call_ret, const char* callexpr, int linenum, const char* filename) {
    if(call_ret == -1) {
	fprintf(stderr, "[WARNING] during call %s\n", callexpr);
	fprintf(stderr, "[WARNING] at line %d of file %s\n",
		linenum, filename);
	fprintf(stderr, "[WARNING] returned %d\n", call_ret);
	fprintf(stderr, "[WARNING] errno is %d, meaning %s\n\n",
		errno, strerror(errno));
    }
    return call_ret;
}

#ifndef __cplusplus
typedef struct command {
    const size_t argc;
    char** argv;
} command;
typedef struct command_arr {
    const size_t size;
    const command* cmds;
} command_arr;
#define CMD(...) ((command){				\
            .argc = ARG_COUNT(__VA_ARGS__),		\
            .argv = (char*[]){ __VA_ARGS__, NULL },	\
        })
#define CMDS(...) ((command_arr){               \
            .size = ARG_COUNT(__VA_ARGS__),     \
            .cmds = (command[]){ __VA_ARGS__ }  \
        })
#else
struct command {
    const size_t argc;
    const char** argv;
    command(size_t argc, const char** argv):argc(argc),argv(argv){}
    ~command() = default;
};
struct command_arr {
    const size_t size;
    const command* cmds;
};
command _CMD(std::initializer_list<const char*> argv) {
    const char* char_argv[argv.size()+1];
    size_t i = 0;
    for(auto s : argv)
	char_argv[i++] = s;

    char_argv[argv.size()] = NULL;
    return command(argv.size(), char_argv);
}
#define CMD(...) _CMD({__VA_ARGS__})
#define CMDS(...) ((command_arr){               \
            .size = ARG_COUNT(__VA_ARGS__),     \
            .cmds = (command[]){ __VA_ARGS__ }  \
        })
#endif // __cplusplus

// ARG_COUNT defined
// preprocessor abuse adapted from
// https://groups.google.com/g/comp.std.c/c/d-6Mj5Lko_s?pli=1
// found in the second anser to
// https://stackoverflow.com/questions/2124339/c-preprocessor-va-args-number-of-arguments
// 
// from 1 to 29 arguments, this macro will return the number of arguments
// passed to the macro
#define ARG_COUNT(...) EXTRACT_30TH_IN                                  \
    (__VA_ARGS__, 29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)
// if __VA_ARGS__ has 1 element __VA_ARGS__, 29, 28, ... will have 30 args
// and the 30th will be 1
//
// with 2 argument the sequence will have 31 args
// and the 30th will be 2
//
// with 3 argument the sequence will have 32 args
// and the 30th will be 3
//
// and so on
#define EXTRACT_30TH_IN(_01, _02, _03, _04, _05, _06, _07, _08, _09, _10, \
                        _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
                        _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, ...) _30

#if defined(unix) || defined(__unix__) || defined(__unix)
pid_t spawn(command cmd) {
    if(cmd.argc == 0)
        return -1;

    pid_t pid = fork();
    if(pid == -1) {
        fprintf(stderr, "[ERROR] failed to create child process!\n");
        fprintf(stderr, "[ERROR] %s\n", strerror(errno));
        return -1;
    }
    else if(pid != 0) {
        dbg("[PARENT] child created succesfully...");
        return pid;
    } else {
        dbg("[CHILD] running...");
        execvp(cmd.argv[0], (char**)cmd.argv);
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

// unix pipeline functions readapted from the wonderful fucking answer at
// https://stackoverflow.com/questions/79219926/how-to-write-to-stdin-of-another-process-in-c-on-linux
// and a bit of
// https://tldp.org/LDP/lpg/node11.html

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

// forked children inherit the parent's file descriptors(?)
// even if opening/closing a file descriptor in a child does not affect the same
// file descriptor in the parent, closing just means that process won't use that file anymore
// if other processes still have it open, it doesn't close for them
// this means a parent can just call pipe, get two fds
// and then after fork is called a child can see those file descriptors
// then hook its stdin and stdout to the ends of the pipe before calling exec
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

    pid_t p;
    for(size_t i = 0; i<cmds.size; ++i) {
	// (if not the last command) create output pipe for command
	// (future input pipe at next iteration)
	if(i != cmds.size-1)
	    if(WARN_ON_FAIL(pipe(outpipe)) == -1) {
		fprintf(stderr,
			"failed to create %zu-th pipe in pipeline\n", i);
		goto loop_error;
	    }

	p = WARN_ON_FAIL(fork());
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
	    WARN_ON_FAIL(execvp(cmds.cmds[i].argv[0], (char**)cmds.cmds[i].argv));
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
	    waitpid(children[i], &unused, 0);
	// close the other  pipe
	if(i != 0 && (close(inpipe[0]) == -1)) {
	    fprintf(stderr, "failed to close pipe during teardown\n");
	    LOG_ERRNO();
	}
	goto end;
    }

    res = (int*)calloc(cmds.size, sizeof(int));
    memset(res, 0, cmds.size*sizeof(int));
    for(size_t i = 0; i<cmds.size; ++i)
	WARN_ON_FAIL(waitpid(children[i], res+i, 0));

 end:
    free(inpipe);
    free(outpipe);
    free(children);

    return res;
}

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
#endif // _WIN32
#endif // EXEC_IMPLEMENTATION_
