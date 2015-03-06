/*
 * hcitool.c
 *
 *  modified from tool of the same name
 */

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "jni/bluetooth.h"
#include "jni/hci.h"
#include "jni/hci_lib.h"

#include "hcitool.h"
extern char g_src[512];

static volatile int signal_received = 0;

#define FLAGS_AD_TYPE 0x01
#define FLAGS_LIMITED_MODE_BIT 0x01
#define FLAGS_GENERAL_MODE_BIT 0x02

#define EIR_FLAGS                   0x01  /* flags */
#define EIR_UUID16_SOME             0x02  /* 16-bit UUID, more available */
#define EIR_UUID16_ALL              0x03  /* 16-bit UUID, all listed */
#define EIR_UUID32_SOME             0x04  /* 32-bit UUID, more available */
#define EIR_UUID32_ALL              0x05  /* 32-bit UUID, all listed */
#define EIR_UUID128_SOME            0x06  /* 128-bit UUID, more available */
#define EIR_UUID128_ALL             0x07  /* 128-bit UUID, all listed */
#define EIR_NAME_SHORT              0x08  /* shortened local name */
#define EIR_NAME_COMPLETE           0x09  /* complete local name */
#define EIR_TX_POWER                0x0A  /* transmit power level */
#define EIR_DEVICE_ID               0x10  /* device ID */

static void sigint_handler(int sig) {
    signal_received = sig;
}

static int read_flags(uint8_t *flags, const uint8_t *data, size_t size) {
    size_t offset;

    if (!flags || !data)
        return -EINVAL;

    offset = 0;
    while (offset < size) {
        uint8_t len = data[offset];
        uint8_t type;

        /* Check if it is the end of the significant part */
        if (len == 0)
            break;

        if (len + offset > size)
            break;

        type = data[offset + 1];

        if (type == FLAGS_AD_TYPE) {
            *flags = data[offset + 2];
            return 0;
        }

        offset += 1 + len;
    }

    return -ENOENT;
}

static int check_report_filter(uint8_t procedure, le_advertising_info *info) {
    uint8_t flags;

    /* If no discovery procedure is set, all reports are treat as valid */
    if (procedure == 0)
        return 1;

    /* Read flags AD type value from the advertising report if it exists */
    if (read_flags(&flags, info->data, info->length))
        return 0;

    switch (procedure) {
    case 'l': /* Limited Discovery Procedure */
        if (flags & FLAGS_LIMITED_MODE_BIT)
            return 1;
        break;
    case 'g': /* General Discovery Procedure */
        if (flags & (FLAGS_LIMITED_MODE_BIT | FLAGS_GENERAL_MODE_BIT))
            return 1;
        break;
    default:
        fprintf(stderr, "Unknown discovery procedure\n");
    }

    return 0;
}

static void eir_parse_name(uint8_t *eir, size_t eir_len, char *buf,
        size_t buf_len) {
    size_t offset = 0;

    while (offset < eir_len) {
        uint8_t field_len = eir[0];
        size_t name_len;

        /* Check for the end of EIR */
        if (field_len == 0)
            break;

        if (offset + field_len > eir_len)
            goto failed;

        switch (eir[1]) {
        case EIR_NAME_SHORT:
        case EIR_NAME_COMPLETE:
            name_len = field_len - 1;
            if (name_len > buf_len)
                name_len = buf_len - 1;

            memcpy(buf, &eir[2], name_len);
            return;
        }

        offset += field_len + 1;
        eir += field_len + 1;
    }

    failed: snprintf(buf, buf_len, "(unknown)");
}

static void eir_parse_uuid(uint8_t *eir, size_t eir_len, char *buf,
        size_t buf_len) {
    size_t offset = 0;

    while (offset < eir_len) {
        uint8_t field_len = eir[0];
        size_t uuid_len;

        /* Check for the end of EIR */
        if (field_len == 0)
            break;

        if (offset + field_len > eir_len)
            goto failed;

        switch (eir[1]) {
        case EIR_UUID128_ALL:
            uuid_len = field_len - 1;
            if (uuid_len != 16 || buf_len < 2 * uuid_len)
                goto failed;
            uint8_t * ba = &eir[2];
            sprintf(buf, "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
                    ba[15], ba[14], ba[13], ba[12], ba[11], ba[10], ba[9], ba[8], ba[7], ba[6], ba[5], ba[4], ba[3], ba[2], ba[1], ba[0]);
            return;
        }

        offset += field_len + 1;
        eir += field_len + 1;
    }

    failed:
    memset(buf, 0, buf_len);
}

static int print_advertising_devices(int dd, uint8_t filter_type) {
    unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
    struct hci_filter nf, of;
    struct sigaction sa;
    socklen_t olen;
    int len;

    olen = sizeof(of);
    if (getsockopt(dd, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
        printf("Could not get socket options\n");
        return -1;
    }

    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);

    if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
        printf("Could not set socket options\n");
        return -1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    while (1) {
        evt_le_meta_event *meta;
        le_advertising_info *info;
        char addr[18];

        while ((len = read(dd, buf, sizeof(buf))) < 0) {
            if (errno == EINTR && signal_received == SIGINT) {
                len = 0;
                goto done;
            }

            if (errno == EAGAIN || errno == EINTR)
                continue;
            goto done;
        }

        ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
        len -= (1 + HCI_EVENT_HDR_SIZE);

        meta = (void *) ptr;

        if (meta->subevent != 0x02)
            goto done;

        /* Ignoring multiple reports */
        info = (le_advertising_info *) (meta->data + 1);
        if (check_report_filter(filter_type, info)) {
            char name[30];
            char uuid[16*2 + 1];

            memset(name, 0, sizeof(name));

            ba2str(&info->bdaddr, addr);
            eir_parse_name(info->data, info->length, name, sizeof(name) - 1);
            eir_parse_uuid(info->data, info->length, uuid, sizeof(uuid) - 1);

            printf("%s %s %s\n", addr, name, uuid);
        }
    }

    done: setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of));

    if (len < 0)
        return -1;

    return 0;
}

int do_lescan() {
    int dev_id, sock;
    int ret;

    if (!strncmp(g_src, "hci", 3))
        dev_id = atoi(g_src + 3);
    else
        dev_id = hci_get_route(NULL);
    sock = hci_open_dev(dev_id);
    if (dev_id < 0 || sock < 0) {
        perror("opening socket");
        exit(1);
    }

    uint8_t own_type = 0x00;
    uint16_t interval = htobs(0x0012);
    uint16_t window = htobs(0x0012);
    uint8_t scan_type = 0x01;
    uint8_t filter_dup = 1;
    uint8_t filter_type = 0;
    ret = hci_le_set_scan_parameters(sock, scan_type, interval, window,
            own_type, 0x00, 1000);
    if (ret) {
        perror("hci_le_set_scan_parameters");
        exit(1);
    }

    ret = hci_le_set_scan_enable(sock, 0x01, filter_dup, 1000);
    if (ret) {
        perror("hci_le_set_scan_enable");
        exit(1);
    }

    print_advertising_devices(sock, filter_type);

    ret = hci_le_set_scan_enable(sock, 0x00, filter_dup, 1000);

    close(sock);
    return 0;
}

