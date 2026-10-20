/*
 * mach_ipc.c — Mach-message-style IPC for OpenMac, implemented over UNIX
 * domain sockets on Linux/Android. Original code, proprietary (see LICENSE).
 * No XNU source is used or derived from; only public API *shapes* are
 * reproduced for interoperability.
 *
 * Port namespace: each "port" is backed by a socket in $OMAC_IPC_DIR
 * (default ~/.omac/ipc). mach_port_name_t encodes a small table index so
 * processes can share names via env/argv if desired.
 */
#include "mach.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <poll.h>

/* Blocking timeout for lazy-accept rendezvous (ms). Prevents callers from
 * hanging forever when a peer never connects (stability hardening, 2026-10). */
#define OMACH_ACCEPT_TIMEOUT_MS 5000

#define OMACH_MAX_PORTS 64

struct omach_port {
    int listen_fd;
    int conn_fd;      /* write side (loopback client fd or accepted peer) */
    int read_fd;      /* read side (loopback accepted fd or accepted peer)   */
    int loopback;     /* 1 when send-to-self queue was established            */
    char path[256];
};

static struct omach_port g_ports[OMACH_MAX_PORTS];

/* Loopback rendezvous: connect() to our own listening socket is allowed by
 * the AF_UNIX implementation and gives us a connected pair of fds — this is
 * how in-process Mach "send-to-self" message queues behave. The client fd
 * becomes the write side, the accepted fd the read side. The listener stays
 * open so an external peer can still connect later (it will then take over
 * both sides, matching Mach's "port right moved to another task" semantics). */
static int loopback_establish(struct omach_port *p) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un sa = { .sun_family = AF_UNIX };
    strncpy(sa.sun_path, p->path, sizeof sa.sun_path - 1);
    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) != 0) { close(fd); return -1; }
    int acc = accept(p->listen_fd, NULL, NULL);
    if (acc < 0) { close(fd); return -1; }
    if (p->conn_fd > 0) close(p->conn_fd);
    if (p->read_fd > 0 && p->read_fd != p->conn_fd) close(p->read_fd);
    p->conn_fd = fd;   /* write side */
    p->read_fd = acc;  /* read side  */
    p->loopback = 1;
    return 0;
}

static const char *ipc_dir(void) {
    const char *d = getenv("OMAC_IPC_DIR");
    return d && *d ? d : "/tmp/omach-ipc";
}

kern_return_t omach_port_allocate(mach_port_t *name) {
    if (!name) return KERN_FAILURE;
    mkdir(ipc_dir(), 0700);
    for (int i = 1; i < OMACH_MAX_PORTS; i++) {
        if (g_ports[i].listen_fd) continue;
        char path[256];
        snprintf(path, sizeof path, "%s/p%d.%d", ipc_dir(), (int)getpid(), i);
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return KERN_FAILURE;
        struct sockaddr_un sa = { .sun_family = AF_UNIX };
        strncpy(sa.sun_path, path, sizeof sa.sun_path - 1);
        unlink(path);
        if (bind(fd, (struct sockaddr *)&sa, sizeof sa) != 0 ||
            listen(fd, 1) != 0) { close(fd); return KERN_FAILURE; }
        g_ports[i].listen_fd = fd;
        memcpy(g_ports[i].path, path, sizeof g_ports[i].path);
        *name = (mach_port_t)i;
        return KERN_SUCCESS;
    }
    return KERN_FAILURE;
}

kern_return_t omach_port_connect(mach_port_t name, const char *target_path) {
    if (name == 0 || name >= OMACH_MAX_PORTS) return KERN_FAILURE;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return KERN_FAILURE;
    struct sockaddr_un sa = { .sun_family = AF_UNIX };
    strncpy(sa.sun_path, target_path, sizeof sa.sun_path - 1);
    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) != 0) {
        close(fd);
        return MACH_SEND_INVALID_DEST;
    }
    /* External peer takes over this port right: drop any loopback queue and
     * use the peer socket for both directions. */
    struct omach_port *p = &g_ports[name];
    if (p->conn_fd > 0) close(p->conn_fd);
    if (p->read_fd > 0 && p->read_fd != p->conn_fd) close(p->read_fd);
    p->conn_fd = fd;
    p->read_fd = fd;
    p->loopback = 0;
    return KERN_SUCCESS;
}

const char *omach_port_path(mach_port_t name) {
    if (name == 0 || name >= OMACH_MAX_PORTS) return NULL;
    return g_ports[name].path;
}

kern_return_t omach_msg_send(mach_port_t name, const void *buf, size_t len) {
    if (name == 0 || name >= OMACH_MAX_PORTS) return KERN_FAILURE;
    struct omach_port *p = &g_ports[name];
    int fd = p->conn_fd;
    if (fd <= 0) {
        /* No established queue yet. First try a real peer that is already
         * connecting to us (external rendezvous); if nobody shows up within
         * the timeout, fall back to the loopback send-to-self queue so an
         * in-process port behaves like a Mach message queue instead of
         * deadlocking against itself. */
        struct pollfd pfd = { .fd = p->listen_fd, .events = POLLIN };
        int pr = poll(&pfd, 1, 50); /* short grace for a genuine incoming peer */
        if (pr > 0) {
            fd = accept(p->listen_fd, NULL, NULL);
            if (fd < 0) return MACH_SEND_INVALID_DEST;
            p->conn_fd = fd;
            p->read_fd = fd;
        } else if (loopback_establish(p) != 0) {
            return MACH_SEND_INVALID_DEST;
        } else {
            fd = p->conn_fd;
        }
    }
    /* length-prefixed framing: read side relies on SOCK_STREAM preserving
     * message boundaries only in theory; in practice small writes coalesce /
     * large reads truncate. Prefix with 4-byte LE length for exact roundtrip. */
    unsigned char hdr[4] = { (unsigned char)(len), (unsigned char)(len >> 8),
                             (unsigned char)(len >> 16), (unsigned char)(len >> 24) };
    if (write(fd, hdr, 4) != 4) return KERN_FAILURE;
    ssize_t w = write(fd, buf, len);
    return w == (ssize_t)len ? KERN_SUCCESS : KERN_FAILURE;
}

kern_return_t omach_msg_recv(mach_port_t name, void *buf, size_t maxlen,
                             size_t *out_len) {
    if (name == 0 || name >= OMACH_MAX_PORTS) return KERN_FAILURE;
    struct omach_port *p = &g_ports[name];
    int fd = p->read_fd > 0 ? p->read_fd : p->conn_fd;
    if (fd <= 0) {
        /* Nothing queued and no loopback yet: a bare recv must not deadlock.
         * Wait for an external peer up to the timeout; on timeout fall back
         * to the loopback queue (empty read will then time out cleanly in
         * the poll below rather than hang). */
        struct pollfd pfd = { .fd = p->listen_fd, .events = POLLIN };
        int pr = poll(&pfd, 1, OMACH_ACCEPT_TIMEOUT_MS);
        if (pr > 0) {
            fd = accept(p->listen_fd, NULL, NULL);
            if (fd < 0) return KERN_FAILURE;
            p->conn_fd = fd;
            p->read_fd = fd;
        } else if (loopback_establish(p) != 0) {
            return KERN_FAILURE;
        } else {
            fd = p->read_fd;
        }
    }
    /* Block only as long as the timeout allows; r<=0 with errno EAGAIN means
     * "no message", matching Mach's MACH_RCV_TIMED_OUT behaviour. */
    unsigned char hdr[4];
    size_t off = 0;
    while (off < 4) {
        struct pollfd rp = { .fd = fd, .events = POLLIN };
        if (poll(&rp, 1, OMACH_ACCEPT_TIMEOUT_MS) <= 0) return KERN_FAILURE;
        ssize_t r = read(fd, hdr + off, 4 - off);
        if (r <= 0) return KERN_FAILURE;
        off += (size_t)r;
    }
    uint32_t mlen = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) |
                    ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    if (mlen > maxlen) {
        /* drain oversized payload so stream stays in sync, then report fail */
        char scratch[1024];
        uint32_t left = mlen;
        while (left) {
            ssize_t r = read(fd, scratch, left < sizeof scratch ? left : sizeof scratch);
            if (r <= 0) return KERN_FAILURE;
            left -= (uint32_t)r;
        }
        return KERN_FAILURE;
    }
    off = 0;
    while (off < mlen) {
        ssize_t r = read(fd, (char *)buf + off, mlen - off);
        if (r <= 0) return KERN_FAILURE;
        off += (size_t)r;
    }
    if (out_len) *out_len = off;
    return KERN_SUCCESS;
}

kern_return_t omach_port_deallocate(mach_port_t name) {
    if (name == 0 || name >= OMACH_MAX_PORTS) return KERN_FAILURE;
    struct omach_port *p = &g_ports[name];
    /* Mach semantics: deallocating a dead/null right must fail, not silently
     * succeed — this also makes double-deallocate detectable by callers. */
    if (p->listen_fd <= 0 && p->conn_fd <= 0) return KERN_FAILURE;
    if (p->conn_fd > 0) close(p->conn_fd);
    if (p->read_fd > 0 && p->read_fd != p->conn_fd) close(p->read_fd);
    if (p->listen_fd > 0) close(p->listen_fd);
    if (*p->path) unlink(p->path);
    memset(p, 0, sizeof *p);
    return KERN_SUCCESS;
}
