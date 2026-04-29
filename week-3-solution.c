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
    int ret = writef("/proc/self/setgroups", "deny");
    if (ret != 0)
        goto out;

    ret = writef("/proc/self/gid_map", "%d %d 1", outside_gid, outside_gid);
    if (ret != 0)
        goto out;

    ret = writef("/proc/self/uid_map", "%d %d 1", outside_uid, outside_uid);

out:
    return ret;
}

int init_mounts(const char *jail_path) {
    // make the root mount private
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0) {
        perror("mount(/, MS_PRIVATE)");
        return 1;
    }
    // prepare the jail mount
    if (mount(jail_path, jail_path, NULL, MS_BIND, NULL) < 0) {
        perror("mount(jail_path, jail_path, MS_BIND)");
        return 1;
    }
    if (chdir(jail_path) < 0) {
        perror("chdir(jail_path)");
        return 1;
    }
    // mount proc into jail
    if (mount("none", "./proc", "proc", 0, NULL) < 0) {
        perror("mount(none, ./proc, proc)");
        return 1;
    }
    // pivot root
    if (pivot_root(".", ".") < 0) {
        perror("pivot_root(., .)");
        return 1;
    }
    // unmount old root by unmounting "."
    if (umount2(".", MNT_DETACH) < 0) {
        perror("umount2(., MNT_DETACH)");
        return 1;
    }
    // fully change into new root
    if (chdir("/") < 0) {
        perror("chdir(/)");
        return 1;
    }
    return 0;
}

int create_jail(const char *jail_path) {
    uid_t parent_uid = geteuid();
    gid_t parent_gid = getegid();
    struct clone_args ca = {
        .flags = CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWPID,
        .exit_signal = SIGCHLD,
    };
    pid_t pid = clone3(&ca);
    if (pid < 0) {
        perror("clone3");
        return 1;
    }

    if (pid == 0) {
        int ret = init_mounts(jail_path);
        if (ret != 0)
            return ret;
        ret = map_users(parent_uid, parent_gid);
        if (ret != 0)
            return ret;
        setenv("PATH", "/bin", 1);

        char *argv[] = {"/bin/sh", NULL};
        if (execv(argv[0], argv) < 0) {
            perror("execv(/bin/sh)");
            return 1;
        }
    } else {
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return 1;
        }
        if (!WIFEXITED(status)) {
            fprintf(stderr, "child did not exit\n");
            return 1;
        }
        return WEXITSTATUS(status);
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
