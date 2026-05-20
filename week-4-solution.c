#define _GNU_SOURCE

#include <fcntl.h>
#include <linux/sched.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int get_cgroup_path(char **out) {
    FILE *f = popen(DELEGATE_CGROUP_CMD, "r");
    if (!f) {
        perror("popen");
        return 1;
    }

    size_t n;
    ssize_t nread = getline(out, &n, f);
    if (nread < 0) {
        if (feof(f))
            fputs("getline: unexpected end-of-file\n", stderr);
        else
            perror("getline");
        return 1;
    }
    (*out)[strcspn(*out, "\n")] = '\0';

    int status = pclose(f);
    if (status < 0) {
        perror("pclose");
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(
            stderr,
            "delegate-cgroup exited with %s %d\n",
            WIFEXITED(status) ? "status code" : "signal",
            WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status)
        );
        return 1;
    }
    return 0;
}

int init_cgroup_parent(const char *cgroup_path) {
    // make both parent and child cgroups
    char *parent_cgroup = NULL;
    char *child_cgroup = NULL;
    asprintf(&parent_cgroup, "%s/parent", cgroup_path);
    asprintf(&child_cgroup, "%s/child", cgroup_path);

    if (mkdir(parent_cgroup, 0700) < 0) {
        perror("mkdir(cgroup/parent)");
        return 1;
    }
    if (mkdir(child_cgroup, 0700) < 0) {
        perror("mkdir(cgroup/child)");
        return 1;
    }

    // move into the parent cgroup before clone
    if (writef_subpath(cgroup_path, "parent/cgroup.procs", "0")) {
        perror("writef(cgroup/parent/cgroup.procs)");
        return 1;
    }

    free(parent_cgroup);
    free(child_cgroup);

    return 0;
}

int init_cgroup_child(const char *cgroup_path) {
    // move into the child cgroup
    if (writef_subpath(cgroup_path, "child/cgroup.procs", "0")) {
        perror("writef(cgroup/child/cgroup.procs)");
        return 1;
    }

    // enable controllers in root cgroup
    if (writef_subpath(cgroup_path, "cgroup.subtree_control", "+pids +memory +cpu")) {
        perror("writef(cgroup/cgroup.subtree_control)");
        return 1;
    }

    // write cgroup limits
    if (writef_subpath(cgroup_path, "child/pids.max", "20")) {
        perror("writef(cgroup/child/pids.max)");
        return 1;
    }
    if (writef_subpath(cgroup_path, "child/memory.max", "20M")) {
        perror("writef(cgroup/child/memory.max)");
        return 1;
    }
    if (writef_subpath(cgroup_path, "child/memory.swap.max", "20M")) {
        perror("writef(cgroup/child/memory.swap.max)");
        return 1;
    }
    if (writef_subpath(cgroup_path, "child/cpu.max", "100000 1000000")) {
        perror("writef(cgroup/child/cpu.max)");
        return 1;
    }

    // move into a new cgroup namespace
    if (unshare(CLONE_NEWCGROUP) < 0) {
        perror("unshare(CLONE_NEWCGROUP)");
        return 1;
    }

    return 0;
}

int create_jail(const char *jail_path) {
    uid_t parent_uid = geteuid();
    uid_t parent_gid = getegid();

    char *cgroup_path = NULL;
    int ret = get_cgroup_path(&cgroup_path);
    if (ret)
        return ret;

    ret = init_cgroup_parent(cgroup_path);
    if (ret)
        return ret;

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
        ret = init_cgroup_child(cgroup_path);
        if (ret)
            return ret;
        ret = init_mounts(jail_path);
        if (ret)
            return ret;
        ret = map_users(parent_uid, parent_gid);
        if (ret)
            return ret;

        char *argv[] = {"/bin/sh", NULL};
        char *envp[] = {"PATH=/bin", NULL};
        if (execve(argv[0], argv, envp) < 0) {
            perror("execve(/bin/sh)");
            return 1;
        }
    } else {
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return 1;
        }
        if (!WIFEXITED(status)) {
            fputs("child did not exit\n", stderr);
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
