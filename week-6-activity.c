#define _GNU_SOURCE

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/route.h>
#include <linux/sched.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define DELEGATE_CGROUP_CMD "exec delegate-cgroup"
#define MAX_NET_PACKET 4096
#define TUN_IFACE_NAME "eth0"
#define TUN_IFACE_ADDR "10.255.255.1"

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

int send_fd(int unix_sock, int fd) {
    uint8_t dummy_data = 0; // must send at least one byte
    struct iovec iov = {
        .iov_base = &dummy_data,
        .iov_len = sizeof(dummy_data)
    };

    union {
        char buf[CMSG_SPACE(sizeof(fd))];
        struct cmsghdr align;
    } u;

    struct msghdr msg = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = u.buf,
        .msg_controllen = sizeof(u.buf)
    };

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    *cmsg = (struct cmsghdr) {
        .cmsg_level = SOL_SOCKET,
        .cmsg_type = SCM_RIGHTS,
        .cmsg_len = CMSG_LEN(sizeof(fd))
    };

    memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

    ssize_t nbytes = sendmsg(unix_sock, &msg, 0);
    if (nbytes != sizeof(dummy_data)) {
        if (nbytes < 0)
            perror("sendmsg");
        return -1;
    }
    return 0;
}

int recv_fd(int unix_sock) {
    uint8_t dummy_data = 0;
    struct iovec iov = {
        .iov_base = &dummy_data,
        .iov_len = sizeof(dummy_data)
    };

    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    struct msghdr msg = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = u.buf,
        .msg_controllen = sizeof(u.buf)
    };

    ssize_t nbytes = recvmsg(unix_sock, &msg, 0);
    if (nbytes != sizeof(dummy_data)) {
        if (nbytes < 0)
            perror("recvmsg");
        return -1;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg->cmsg_level != SOL_SOCKET ||
        cmsg->cmsg_type != SCM_RIGHTS ||
        cmsg->cmsg_len != CMSG_LEN(sizeof(int)))
        return -1;
    int fd;
    memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));
    return fd;
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

int init_net(int unix_sock) {
    // create a new tun interface by opening /dev/net/tun
    struct ifreq ifr = {
        .ifr_flags = IFF_TUN | IFF_NO_PI,
        .ifr_name = TUN_IFACE_NAME
    };
    // initialize the interface with the name TUN_IFACE_NAME by passing the ifreq struct to
    // the TUNSETIFF ioctl with the file descriptor of the interface

    // create a dummy socket to configure the newly created interface by calling ioctl
    // on the socket. See the netdevice(7) man page for the ioctl numbers and arguments

    // set the interface's IP address to TUN_IFACE_ADDR (defined above). You can use the
    // inet_pton function to convert the address from a string to a numeric representation

    // set the subnet mask to /32 (255.255.255.255 or 0xffffffff)

    // bring the interface up. To do this, you should first get the interface flags, enable
    // the appropriate bit and then set the flags back

    struct rtentry rt = {
        .rt_flags = RTF_UP,
        .rt_dev = TUN_IFACE_NAME
    };
    // add an entry to the routing table that routes all outgoing connections to the interface.
    // This can be done by setting the destination address and the mask of the entry to 0.0.0.0
    // (INADDR_ANY). You will need to fill in the rtentry struct and pass it to the SIOCADDRT ioctl

    // send the file descriptor of the tun interface (NOT the dummy socket) to the parent using the
    // send_fd helper function

    return 0;
}

// For debugging
void hexdump(const uint8_t *buf, size_t n) {
    for (size_t i = 0; i < n; i++)
        printf("%02x%c", buf[i], (i == n - 1 || i % 16 == 15) ? '\n' : ' ');
}

union packet {
    struct {
        // ip header
        uint8_t version_and_header_len;
        uint8_t unused1;
        uint16_t ip_len;
        uint16_t ident;
        uint16_t unused2;
        uint8_t ttl;
        uint8_t protocol;
        uint16_t ip_checksum;
        uint32_t src_addr;
        uint32_t dst_addr;
        // udp header
        uint16_t src_port;
        uint16_t dst_port;
        uint16_t udp_len;
        uint16_t udp_checksum;
        uint8_t udp_data[];
    } __attribute__((packed));

    uint8_t buf[MAX_NET_PACKET];
};

#define IP_HEADER_LEN 20
#define UDP_HEADER_LEN 8
#define PROTO_UDP 17

uint16_t checksum_ip(const uint8_t *ptr, size_t count) {
    uint32_t sum = 0;
    for (size_t i = 0; i < count; i += 2) {
        if (count - i < 2)
            sum += ptr[i];
        else
            sum += *(uint16_t *)(ptr + i);
    }
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}

void *handle_packets(void *arg) {
    int tun_fd = *(int *)arg;

    for (;;) {
        // read an IPv4 packet from tun_fd to the packet union provided above (to facilitate parsing the packet)

        // verify the following fields in the packet's IP and UDP header:
        // - IP version: 4
        // - IP header length: 5 (20 bytes)
        // - protocol: UDP (17)
        // - IP (total) packet length: should be equal to the number of bytes received
        // - UDP packet length: should be equal to the total length minus the length of the IP header
        // NOTE: lengths, addresses and ports are all represented in network byte order (big endian), so it is
        // necessary to convert using ntoh/hton

        // create a UDP socket and call connect on it with the destination address and port from the received
        // packet. Note that this does not actually establish a connection like a TCP socket would. It only
        // sets the socket's destination address so you can use send/recv instead of sendto/recvfrom, with the
        // added benefit that you will not receive irrelevant packets sent from other addresses or ports

        // send the UDP payload to the socket and receive a response

        // repackage the received data into an IP packet. Only the following fields are necessary,
        // and the rest can be set to zero:
        // - IP version and header length: same as above
        // - IP (total) packet length: total header length + data length
        // - UDP packet length: UDP header length + data length
        // - time-to-live (TTL): any sufficiently large number
        // - protocol: UDP (17)
        // - IP checksum: computed over the IP header using the checksum_ip helper function
        // - source and destination addresses and ports
        // - UDP payload

        // write the IP packet to tun_fd
    }
    return NULL;
}

int create_jail(const char *jail_path) {
    uid_t parent_uid = geteuid();
    gid_t parent_gid = getegid();

    // get the delegated cgroup path

    int ret;
    // before cloning, first initialize cgroup in the parent

    // create a pair of UNIX sockets, which will be used to pass file descriptors between parent and child

    struct clone_args ca = {
        .flags = CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET,
        .exit_signal = SIGCHLD,
    };
    pid_t pid = clone3(&ca);
    if (pid < 0) {
        perror("clone3");
        return 1;
    }

    if (pid == 0) {
        // close one end of the UNIX socket pair

        // initialize the network namespace. We do this first because we need to access the /dev filesystem

        // initialize cgroup in the child

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
        // close the other end of the UNIX socket pair

        // receive the file descriptor of the tun interface from the child using recv_fd

        // create another thread that handles packets arriving on the interface's file descriptor

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
