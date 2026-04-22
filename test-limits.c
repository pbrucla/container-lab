#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

long long get_monotonic_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main(void) {
    char buf[4096];
    FILE* cgroup = fopen("/proc/self/cgroup", "r");
    if (cgroup == NULL) {
        puts("couldn't read /proc/self/cgroup, FAIL");
    } else {
        fgets(buf, sizeof(buf), cgroup);
        buf[strcspn(buf, "\n")] = '\0';
        printf("cgroup appears to be %s, %s\n", buf, strcmp(buf, "0::/") == 0 ? "OK" : "FAIL");
    }
    pid_t children[40];
    int cur = 0;
    for (int i = 0; i < 40; i ++) {
        pid_t pid = fork();
        if (pid < 0) {
            continue;
        }
        if (pid == 0) {
            sleep(60);
            return 0;
        } else {
            children[cur++] = pid;
        }
    }
    printf("successfully spawned %d children, %s\n", cur, cur < 20 ? "OK" : "FAIL");
    for (int i = 0; i < cur; i ++) {
        kill(children[i], SIGINT);
        if (waitpid(children[i], NULL, 0) < 0) {
            perror("waitpid");
            return 1;
        }
    }
    int pipes[2];
    if (pipe(pipes) < 0) {
        perror("pipe");
        return 1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // prioritize child for OOM killing
        int fd = open("/proc/self/oom_score_adj", O_WRONLY);
        if (fd < 0) {
            perror("open(/proc/self/oom_score_adj)");
            return 1;
        }
        if (write(fd, "1000", 4) < 0) {
            perror("write(/proc/self/oom_score_adj, 1000)");
            return 1;
        }
        if (dup2(pipes[1], 1) < 0) {
            perror("dup2");
            return 1;
        }
        if (close(pipes[0]) < 0) {
            perror("close");
            return 1;
        }
        for (int i = 0; i < 40; i ++) {
            char* ptr = malloc(1000000);
            memset(ptr, '!', 1000000);
            printf("%d\n", i);
            fflush(stdout);
        }
        // wait a bit in case OOM killer is slow for some reason
        sleep(1);
        return 0;
    } else {
        if (close(pipes[1]) < 0) {
            perror("close");
            return 1;
        }
        FILE* f = fdopen(pipes[0], "r");
        if (f == NULL) {
            perror("fdopen");
            return 1;
        }
        char buf[64] = {0};
        errno = 0;
        while (fgets(buf, 64, f)) {}
        if (errno) {
            perror("fgets");
            return 1;
        }
        buf[strcspn(buf, "\n")] = '\0';
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return 1;
        }
        if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL) {
            if (WIFEXITED(status)) {
                printf("child allocated 40 MB of ram without being OOM killed (exited with code %d), FAIL\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("child allocated 40 MB of ram without being OOM killed (exited with signal %d), FAIL\n", WTERMSIG(status));
            } else {
                printf("child allocated 40 MB of ram without being OOM killed (unknown exit reason), FAIL\n");
            }
        } else {
            printf("child allocated %s MB of ram then got OOM killed, OK\n", buf);
        }
        if (fclose(f) < 0) {
            perror("fclose");
            return 1;
        }
    }
    clock_t start_cpu = clock();
    clock_t start_ms = get_monotonic_ms();
    while (get_monotonic_ms() - start_ms < 1000) {}
    clock_t end_cpu = clock();
    double cpu_usage = (double)(end_cpu - start_cpu) / CLOCKS_PER_SEC;
    printf("CPU ran for %.2f%% of the time, %s\n", cpu_usage * 100, cpu_usage < 0.4 ? "OK" : "FAIL");
    return 0;
}
