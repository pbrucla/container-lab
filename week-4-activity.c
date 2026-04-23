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

#define DELEGATE_CGROUP_CMD "exec delegate-cgroup"

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

int writef_subpath(const char *path, const char *subpath, const char *fmt, ...) {
    int ret = 1;
    va_list args;
    va_start(args, fmt);

    char *fullpath = NULL;
    asprintf(&fullpath, "%s/%s", path, subpath);
    int fd = open(fullpath, O_WRONLY);
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
    free(fullpath);
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

int get_cgroup_path(char **out) {
    // run DELEGATE_CGROUP_CMD as a child process using popen and
    // read a line from its standard output, and store a pointer to it in the out variable

    return 0;
}

int init_cgroup_parent(const char *cgroup_path) {
    // 1. create 2 sub-cgroups under cgroup_path for the parent and the child
    // 2. move ourselves into the parent cgroup

    return 0;
}

int init_cgroup_child(const char *cgroup_path) {
    // you will have to do the following 4 things, not necessarily in this order:
    // 1. enter a new cgroup namespace
    // 2. enable the necessary cgroup controllers (think: which file do you need to write to?)
    // 3. set the necessary limits in each controller (look at the test-limits section of the activity README for what limits you need to set)
    // 4. move ourselves into the child cgroup
    // these need to be done in a specific order; think about what the order should be

    return 0;
}

int create_jail(const char *jail_path) {
    // get the effective uid and gid of the current process (the parent)
    uid_t parent_uid = 0;
    gid_t parent_gid = 0;

    // get the delegated cgroup path

    // before cloning, first initialize cgroup in the parent

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

        // 1. initialize cgroup in the child
        // 2. initialize mounts
        // 3. map users

        char *argv[] = {"/bin/sh", NULL};
        char *envp[] = {"PATH=/bin", NULL};
        if (execve(argv[0], argv, envp) < 0) {
            perror("execve(/bin/sh)");
            return 1;
        }
    } else {
        // this is the parent

        // wait for the child to exit using waitpid
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
