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

#define OMACH_MAX_PORTS 64

struct omach_port {
    int listen_fd;
    int conn_fd;      /* accepted peer for send side */
    char path[256];
};

static struct omach_port g_ports[OMACH_MAX_PORTS];

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
    g_ports[name].conn_fd = fd;
    return KERN_SUCCESS;
}

const char *omach_port_path(mach_port_t name) {
    if (name == 0 || name >= OMACH_MAX_PORTS) return NULL;
    return g_ports[name].path;
}

kern_return_t omach_msg_send(mach_port_t name, const void *buf, size_t len) {
    if (name == 0 || name >= OMACH_MAX_PORTS) return KERN_FAILURE;
    int fd = g_ports[name].conn_fd;
    if (fd <= 0) {
        /* lazy accept: treat our own listen socket as rendezvous */
        fd = accept(g_ports[name].listen_fd, NULL, NULL);
        if (fd < 0) return MACH_SEND_INVALID_DEST;
        g_ports[name].conn_fd = fd;
    }
    ssize_t w = write(fd, buf, len);
    return w == (ssize_t)len ? KERN_SUCCESS : KERN_FAILURE;
}

kern_return_t omach_msg_recv(mach_port_t name, void *buf, size_t maxlen,
                             size_t *out_len) {
    if (name == 0 || name >= OMACH_MAX_PORTS) return KERN_FAILURE;
    int fd = g_ports[name].listen_fd;
    if (g_ports[name].conn_fd > 0) fd = g_ports[name].conn_fd;
    else {
        fd = accept(g_ports[name].listen_fd, NULL, NULL);
        if (fd < 0) return KERN_FAILURE;
        g_ports[name].conn_fd = fd;
    }
    ssize_t r = read(fd, buf, maxlen);
    if (r < 0) return KERN_FAILURE;
    if (out_len) *out_len = (size_t)r;
    return KERN_SUCCESS;
}

kern_return_t omach_port_deallocate(mach_port_t name) {
    if (name == 0 || name >= OMACH_MAX_PORTS) return KERN_FAILURE;
    struct omach_port *p = &g_ports[name];
    if (p->conn_fd > 0) close(p->conn_fd);
    if (p->listen_fd > 0) close(p->listen_fd);
    if (*p->path) unlink(p->path);
    memset(p, 0, sizeof *p);
    return KERN_SUCCESS;
}
