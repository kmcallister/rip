// Run a program while clearing registers after every instruction,
// except those before the symbol FORBID_REGS.
//
// Build: gcc -O2 -Wall -o regclobber regclobber.c
// Run:   ./regclobber prog args...

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ptrace.h>

#define ERRNO_DIE(_msg) \
    do { perror((_msg)); exit(1); } while (0)

#define CHECK(_call) \
    do { if ((_call) < 0) ERRNO_DIE(#_call); } while (0)

uint64_t inst_count = 0, clobber_count = 0;

__attribute__((noreturn))
void finish(int retval) {
    fprintf(stderr, "\nExecuted %lu instructions; clobbered registers %lu times.\n",
        inst_count, clobber_count);
    exit(retval);
}

// Allow register access below this address, so that the child
// can make system calls.
uint64_t regs_boundary = 0;

// Set regs_boundary from the FORBID_REGS symbol in the child.
void find_regs_boundary(pid_t child) {
    char *cmd;
    CHECK(asprintf(&cmd, "nm /proc/%ld/exe | grep 'FORBID_REGS$'", (long) child));

    FILE *symbols = popen(cmd, "r");
    if (symbols == NULL)
        ERRNO_DIE("popen");

    if (fscanf(symbols, "%lx", &regs_boundary) < 1) {
        fprintf(stderr, "WARNING: Could not find symbol FORBID_REGS.\n"
            "Registers are forbidden everywhere.\n");
    }

    CHECK(pclose(symbols));
    free(cmd);
}

// Bits to preserve in EFLAGS. Everything but the condition codes.
#define EFLAGS_MASK 0xfffffffffffff32a

// Clobber forbidden registers in the traced process.
void clobber_regs(pid_t child) {
    struct user_regs_struct   regs_int;
    struct user_fpregs_struct regs_fp;

    CHECK(ptrace(PTRACE_GETREGS, child, 0, &regs_int));
    if (regs_int.rip < regs_boundary)
        return;

    CHECK(ptrace(PTRACE_GETFPREGS, child, 0, &regs_fp));

    // Clear everything before the instruction pointer,
    // plus the stack pointer and some bits of EFLAGS.
    // See /usr/include/sys/user.h for the layout of
    // this struct.
    memset(&regs_int, 0, offsetof(struct user_regs_struct, rip));
    regs_int.rsp = 0;
    regs_int.eflags &= EFLAGS_MASK;

    // Clear x87 and SSE registers.
    memset(regs_fp.st_space,  0, sizeof(regs_fp.st_space));
    memset(regs_fp.xmm_space, 0, sizeof(regs_fp.xmm_space));

    CHECK(ptrace(PTRACE_SETREGS,   child, 0, &regs_int));
    CHECK(ptrace(PTRACE_SETFPREGS, child, 0, &regs_fp));

    clobber_count++;
}

volatile sig_atomic_t exit_sig = 0;

void handle_exit_sig(int signum) {
    exit_sig = signum;
}

int main(int argc, char *argv[]) {
    int i;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s prog args...\n", argv[0]);
        return 1;
    }

    // Fork and exec the target program.
    pid_t child;
    CHECK(child = fork());

    if (!child) {
        CHECK(ptrace(PTRACE_TRACEME));
        CHECK(raise(SIGSTOP));
        execvp(argv[1], argv+1);
        ERRNO_DIE("execvp");
    }

    // Call finish() if the user presses ^C.
    CHECK(signal(SIGINT, handle_exit_sig));

    // Wait for attach.
    int status;
    CHECK(waitpid(child, &status, 0));

    // Let the child execute into and out of execvp().
    for (i=0; i<2; i++) {
        CHECK(ptrace(PTRACE_SYSCALL, child, 0, 0));
        CHECK(waitpid(child, &status, 0));
    }

    // Look for the FORBID_REGS symbol.
    find_regs_boundary(child);

    // Single-step the program and clobber registers.
    while (!exit_sig) {
        // For this demo it's simpler if we don't deliver signals to
        // the child, so the last argument to ptrace() is zero.
        CHECK(ptrace(PTRACE_SINGLESTEP, child, 0, 0));
        CHECK(waitpid(child, &status, 0));

        if (WIFEXITED(status))
          finish(WEXITSTATUS(status));

        inst_count++;
        clobber_regs(child);
    }

    finish(128 + exit_sig);
}
