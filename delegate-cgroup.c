#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int rec_remove_dir(const char *path) {
    DIR *dir_p = opendir(path);
    if (!dir_p) {
        if (errno == ENOENT)
            return 0;
        perror("opendir");
        return 1;
    }

    struct dirent *dirent_p;
    while ((dirent_p = readdir(dir_p))) {
        if (dirent_p->d_type == DT_DIR) {
            if (strcmp(dirent_p->d_name, ".") == 0 || strcmp(dirent_p->d_name, "..") == 0)
                continue;
            char *subdir_path = NULL;
            asprintf(&subdir_path, "%s/%s", path, dirent_p->d_name);
            int ret = rec_remove_dir(subdir_path);
            free(subdir_path);
            if (ret)
                return ret;
        }
    }

    if (rmdir(path) < 0) {
        perror("rmdir");
        return 1;
    }
    return 0;
}

int main(void) {
    uid_t uid = getuid();
    uid_t gid = getgid();

    char *jail_path = NULL;
    if (access("/sys/fs/cgroup/cgroup.procs", F_OK) == 0)
        asprintf(&jail_path, "/sys/fs/cgroup/jail-%u", uid);
    else
        asprintf(&jail_path, "/sys/fs/cgroup/unified/jail-%u", uid);

    int ret = rec_remove_dir(jail_path);
    if (ret)
        return ret;

    if (mkdir(jail_path, 0700) < 0) {
        perror("mkdir(cgroup)");
        return 1;
    }

    if (chown(jail_path, uid, gid) < 0) {
        perror("chown(cgroup)");
        return 1;
    }

    char *cgroup_procs = NULL;
    char *cgroup_subtree_control = NULL;
    asprintf(&cgroup_procs, "%s/cgroup.procs", jail_path);
    asprintf(&cgroup_subtree_control, "%s/cgroup.subtree_control", jail_path);

    if (chown(cgroup_procs, uid, gid) < 0) {
        perror("chown(cgroup.procs)");
        return 1;
    }
    if (chown(cgroup_subtree_control, uid, gid) < 0) {
        perror("chown(cgroup.subtree_control)");
        return 1;
    }

    int fd = open(cgroup_procs, O_WRONLY);
    char buf[16];
    int len = snprintf(buf, 16, "%d", getppid());
    if (fd < 0) {
        perror("open(cgroups.procs)");
        return 1;
    }
    if (write(fd, buf, len) < 1) {
        perror("write(cgroup.procs)");
        return 1;
    }
    close(fd);

    puts(jail_path);
    return 0;
}
