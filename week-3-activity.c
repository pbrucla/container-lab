#define _GNU_SOURCE

#include <fcntl.h>
#include <linux/sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

long clone3(struct clone_args *cl_args) {
    return syscall(SYS_clone3, cl_args, sizeof(*cl_args));
}

long pivot_root(const char *new_root, const char *put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
}

int writef(const char *path, const char *fmt, ...) {
    int ret = 1;
    va_list args;
    va_start(args, fmt);

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("open");
        goto out;
    }
    if (vdprintf(fd, fmt, args) < 0) {
        perror("write");
        goto out;
    }
    ret = 0;

out:
    close(fd);
    va_end(args);
    return ret;
}

int map_users(uid_t outside_uid, uid_t outside_gid) {
    // disable setgroups

    // map outside_gid to itself

    // map outside_uid to itself

    return 0;
}

int init_mounts(const char *jail_path) {
    // make the root mount private recursively

    // make the jail a mount point

    // chdir into the jail

    // mount proc into jail

    // pivot root

    // unmount old root by unmounting "."

    // fully change into new root using chdir

    return 0;
}

int create_jail(const char *jail_path) {
    // get the effective uid and gid of the current process (the parent)
    pid_t parent_uid = 0;
    pid_t parent_gid = 0;

    // set up arguments for clone3 to unshare user, mount and pid namespaces
    struct clone_args ca = {
        .flags = 0, // TODO
        .exit_signal = 0, // TODO
    };

    // call clone3 with the above arguments
    pid_t pid = 0;
    if (pid < 0) {
        perror("clone3");
        return 1;
    }

    if (pid == 0) {
        // this is the child

        // init mounts, and then map users

        setenv("PATH", "/bin", 1);

        char *argv[] = {"/bin/sh", NULL};
        if (execv(argv[0], argv) < 0) {
            perror("execv(/bin/sh)");
            return 1;
        }
    } else {
        // this is the parent

        // wait for the child to exit using the waitpid function
    }

    return 0;
}

int main(int argc, const char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <jail_path>\n", argv[0]);
        return 1;
    }
    return create_jail(argv[1]);
}
