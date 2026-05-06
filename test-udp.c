#define _GNU_SOURCE

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_NET_PACKET 4096
#define IPV4(a, b, c, d) (htonl(((a) << 24) | ((b) << 16) | ((c) << 8) | (d)))
#define QUERY_TARGET \
    ("\x08"          \
     "acmcyber"      \
     "\x03"          \
     "com")

void hexdump(uint8_t* buf, int n) {
    char charset[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) {
        printf("%c%c%c", charset[buf[i] >> 4], charset[buf[i] & 0xf], (i == n - 1 || i % 16 == 15) ? '\n' : ' ');
    }
}

int main(void) {
    // construct dns message
    uint8_t dns_msg[MAX_NET_PACKET] = {0};
    uint16_t ident = 0x1337;  // technically should be randomized but doesn't matter
    *(uint16_t*)dns_msg = htons(ident);
    dns_msg[2] = 1;  // set recursion desired flag
    dns_msg[3] = 0x20;
    *(uint16_t*)(dns_msg + 4) = htons(1);  // 1 question
    uint8_t* remainder = (uint8_t*)(stpcpy((char*)(dns_msg + 12), QUERY_TARGET) + 1);
    *(uint16_t*)remainder = htons(1);        // A record
    *(uint16_t*)(remainder + 2) = htons(1);  // internet class
    ssize_t dns_msg_len = remainder + 4 - dns_msg;

    // send to socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket(AF_INET, SOCK_DGRAM, 0)");
        return 1;
    }
    struct sockaddr_in sa = {.sin_family = AF_INET, .sin_port = htons(53), .sin_addr.s_addr = IPV4(1, 1, 1, 1)};
    printf("Sending %zd bytes to 1.1.1.1:53 via UDP...\n", dns_msg_len);
    hexdump(dns_msg, dns_msg_len);
    ssize_t sent = sendto(sock, dns_msg, dns_msg_len, 0, (struct sockaddr*)&sa, sizeof(sa));
    if (sent < 0) {
        perror("sendto");
        return 1;
    }
    if (sent < dns_msg_len) {
        fprintf(stderr, "WARN: Underfull send (only sent %zd/%zd bytes)\n", sent, dns_msg_len);
    }
    printf("Waiting for response...\n");
    struct sockaddr_in sa_recv;
    socklen_t sa_len = sizeof(sa_recv);
    ssize_t n = recvfrom(sock, dns_msg, sizeof(dns_msg), 0, (struct sockaddr*)&sa_recv, &sa_len);
    if (n < 0) {
        perror("recvfrom");
        return 1;
    }
    if (sa_len != sizeof(sa_recv)) {
        fprintf(stderr, "ERROR: sockaddr is unexpected length (got %u, expected %zu)\n", sa_len, sizeof(sa));
        return 1;
    }
    uint32_t addr = ntohl(sa_recv.sin_addr.s_addr);
    printf("Received %zd bytes from %u.%u.%u.%u:%u\n", n, (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
           addr & 0xff, ntohs(sa_recv.sin_port));
    if (sa_recv.sin_addr.s_addr != sa.sin_addr.s_addr || sa_recv.sin_port != sa.sin_port) {
        fprintf(stderr, "WARN: Receiving address/port is different from original destination\n");
    }
    hexdump(dns_msg, n);
    if (n < 12) {
        fprintf(stderr, "ERROR: Response is smaller than DNS header size\n");
        return 1;
    }
    uint16_t resp_ident = ntohs(*(uint16_t*)dns_msg);
    if (resp_ident != ident) {
        fprintf(stderr, "WARN: DNS transaction identifier is wrong (expected %u, got %u)\n", ident, resp_ident);
    }
    uint16_t qcount = ntohs(*(uint16_t*)(dns_msg + 4));
    uint16_t acount = ntohs(*(uint16_t*)(dns_msg + 6));
    uint8_t* cur = dns_msg + 12;
    uint8_t* end = dns_msg + n;
    for (int i = 0; i < qcount; i++) {
        while (1) {
            uint8_t len = *cur;
            if (len & 0xc0) {
                cur += 2;
                break;
            }
            cur += len + 1;
            if (len == 0) {
                break;
            }
            if (cur >= end) {
                fprintf(stderr, "ERROR: Unexpected end of DNS query when parsing questions\n");
                return 1;
            }
        }
        cur += 4;
        if (cur >= end) {
            fprintf(stderr, "ERROR: Unexpected end of DNS query when parsing questions\n");
            return 1;
        }
    }
    printf("Got %u DNS records:\n", acount);
    for (int i = 0; i < acount; i++) {
        while (1) {
            uint8_t len = *cur;
            if (len & 0xc0) {
                cur += 2;
                break;
            }
            cur += len + 1;
            if (len == 0) {
                break;
            }
            if (cur >= end) {
                fprintf(stderr, "ERROR: Unexpected end of DNS query when parsing answers\n");
                return 1;
            }
        }
        if (end - cur < 10) {
            fprintf(stderr, "ERROR: Unexpected end of DNS query when parsing answers\n");
            return 1;
        }
        if (ntohs(*(uint16_t*)cur) != 1 || ntohs(*(uint16_t*)(cur + 2)) != 1) {
            fprintf(stderr, "WARN: Returned DNS record is not A type with IN class.\n");
        }
        uint16_t resp_len = ntohs(*(uint16_t*)(cur + 8));
        cur += 10;
        if (end - cur < resp_len) {
            fprintf(stderr, "ERROR: Unexpected end of DNS query when parsing answers\n");
            return 1;
        }
        if (resp_len != 4) {
            fprintf(stderr, "WARN: Returned A record has unexpected length (expected 4, got %u)\n", resp_len);
            cur += resp_len;
            continue;
        }
        uint32_t resp_ip = ntohl(*(uint32_t*)cur);
        cur += resp_len;
        printf(" - %u.%u.%u.%u\n", (resp_ip >> 24) & 0xff, (resp_ip >> 16) & 0xff, (resp_ip >> 8) & 0xff,
               resp_ip & 0xff);
    }
    hexdump(cur, end - cur);
    return 0;
}
