/*
 * GAP protocol using L2CAP sockets
 *
 * @date March 8, 2014
 * @author: dashesy
 */

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>


#include "jni/bluetooth.h"
#include "jni/hci.h"
#include "jni/hci_lib.h"
#include "jni/l2cap.h"
#include "btio.h"
#include "att.h"

#include "common.h"
#include "amidefs.h"

size_t g_buflen; // Established MTU

// Read a characteristic
// Inputs:
//   sock   - socket to perform operation on
//   handle - characteristics handle to read from
int exec_read(int sock, uint16_t handle) {
    if (handle == 0)
        return -1;

    uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);

    uint16_t plen = enc_read_req(handle, buf, g_buflen);

    ssize_t len = send(sock, buf, plen, 0);

    free(buf);
    if (len < 0 || len != plen) {
        return -1;
    }
    return 0;
}

// Write to a characteristic
// Inputs:
//   sock   - socket to perform operation on
//   handle - characteristics handle to write to
//   value  - value to write
//   vlen   - size of value in bytes
int exec_write(int sock, uint16_t handle, const uint8_t * value, size_t vlen) {
    if (handle == 0)
        return -1;

    uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);
    uint16_t plen = enc_write_cmd(handle, value, vlen, buf, g_buflen);

    ssize_t len = send(sock, buf, plen, 0);

    free(buf);
    if (len < 0 || len != plen) {
        return -1;
    }

    return 0;
}

// Write to a characteristic with response
// Inputs:
//   sock   - socket to perform operation on
//   handle - characteristics handle to write to
//   value  - value to write
//   vlen   - size of value in bytes
int exec_write_req(int sock, uint16_t handle, const uint8_t * value, size_t vlen) {
    if (handle == 0)
        return -1;

    uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);
    uint16_t plen = enc_write_req(handle, value, vlen, buf, g_buflen);

    ssize_t len = send(sock, buf, plen, 0);

    free(buf);
    if (len < 0 || len != plen) {
        return -1;
    }

    return 0;
}

// Find handle associated with given UUID
int discover_handles(int sock, uint16_t start_handle, uint16_t end_handle) {
    uint16_t plen;
    bt_uuid_t type_uuid;

    uint8_t * buf = malloc(g_buflen);
    memset(buf, 0, g_buflen);

    bt_uuid16_create(&type_uuid, GATT_CHARAC_UUID);

    plen = enc_read_by_type_req(start_handle, end_handle, &type_uuid, buf,
            g_buflen);

    ssize_t len = send(sock, buf, plen, 0);

    free(buf);
    if (len < 0 || len != plen) {
        return -1;
    }
    return 0;
}

// Connect and get GAP socket
int gap_connect(const char * src, const char * dst) {
    int ret;
    struct set_opts opts;
    memset(&opts, 0, sizeof(opts));

    if (dst == NULL || strlen(dst) == 0) {
        fprintf(stderr,
                "Device address must be specified (use --help to find the usage)\n");
        return -1;
    }
    if (str2ba(dst, &opts.dst)) {
        fprintf(stderr,
                "Invalid device address (use --help to find the usage)!\n");
        return -1;
    }

    if (src != NULL) {
        if (!strncmp(src, "hci", 3)) {
            ret = hci_devba(atoi(src + 3), &opts.src);
            if (ret == 0 && g_opt.verbosity) {
                char addr[18];
                ba2str(&opts.src, addr);
                printf("Source address: %s\n", addr);
            }
        } else {
            ret = str2ba(src, &opts.src);
        }
        if (ret) {
            fprintf(stderr,
                    "Invalid interface (use --help to find the usage)!\n");
            return -1;
        }
    } else {
        bacpy(&opts.src, BDADDR_ANY);
    }
    opts.type = BT_IO_L2CAP;
    opts.src_type = BDADDR_LE_PUBLIC;
    opts.dst_type = BDADDR_LE_PUBLIC;
    opts.sec_level = BT_SECURITY_LOW;
    opts.master = -1;
    opts.flushable = -1;
    opts.mode = L2CAP_MODE_BASIC;
    opts.priority = 0;
    opts.cid = ATT_CID;

    int sock = bt_io_connect(&opts);
    if (sock <= 0) {
        fprintf(stderr, "bt_io_connect (%d)\n", errno);
        return -1;
    }
    {
        // Increase socket's receive buffer
        int opt_rcvbuf = (2 * 1024 * 1024);
        ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *) &opt_rcvbuf, sizeof(int));
        if (ret) {
            fprintf(stderr, "setsockopt SO_RCVBUF (%d)\n", errno);
            return -1;
        }
        // Increase socket's send buffer
        int opt_sndbuf = (4 * 1024 * 1024);
        ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *) &opt_sndbuf, sizeof(int));
        if (ret) {
            fprintf(stderr, "setsockopt SO_SNDBUF (%d)\n", errno);
            return -1;
        }
    }

    int ready;
    struct timeval tv;
    fd_set write_fds;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);

    ready = select(sock + 1, NULL, &write_fds, NULL, &tv);
    if (!(ready > 0 && FD_ISSET(sock, &write_fds))) {
        fprintf(stderr, "Connection time out %d error (%d)\n", ready, errno);
        return -1;
    }
    {
        int soerr = 0;
        socklen_t olen = sizeof(soerr);
        if (getsockopt(sock, SOL_SOCKET , SO_ERROR, &soerr, &olen) < 0)
        {
            fprintf(stderr, "getsockopt() time out error (%d)\n", errno);
            return -1;
        }
        if (soerr)
        {
            fprintf(stderr, "Socket error %d error (%d)\n", soerr, errno);
            return -1;
        }
        // Clear errno
        errno = 0;
    }

    // Get the CID and MTU information
    bt_io_get(sock, BT_IO_OPT_OMTU, &opts.omtu, BT_IO_OPT_IMTU, &opts.imtu,
            BT_IO_OPT_CID, &opts.cid, BT_IO_OPT_INVALID);

    g_buflen = (opts.cid == ATT_CID) ? ATT_DEFAULT_LE_MTU : opts.imtu;

    printf("Session started ('q' to quit):\n\t"
            " SRC: %s OMTU: %d IMTU %d CID %d\n\n", src, opts.omtu, opts.imtu,
            opts.cid);

    return sock;
}

// Read data from gap socket
int gap_recv(int sock, void * buf, size_t buflen) {
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);

    int ready = select(sock + 1, &read_fds, NULL, NULL, &tv);
    if (ready == -1) {
        fprintf(stderr, "main select() error\n");
        return -1;
    }

    int len = 0;
    if (ready > 0 && FD_ISSET(sock, &read_fds)) {
        len = (int)recv(sock, buf, buflen, 0);
        if (len < 0) {
            if (errno == EAGAIN) {
                len = 0;
            } else {
                fprintf(stderr, "main recv() error (%d)\n", errno);
                return -1;
            }
        }
    }

    return len;
}

// Shutdown the socket
int gap_shutdown(int sock) {
    if (sock < 0)
        return -1;

    // Close socket
    shutdown(sock, SHUT_RDWR);
    close(sock);

    return 0;
}

