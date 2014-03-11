/*
 * Amiigo Link data processing
 *
 * @date March 8, 2014
 * @author: dashesy
 */

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <time.h>

#include "common.h"
#include "amidefs.h"
#include "amdev.h"
#include "amlprocess.h"
#include "amchar.h"
#include "gapproto.h"
#include "fwupdate.h"

/******************************************************************************/
typedef struct {
    uint8* buf;
    uint8 pos;
} GetBits;
/******************************************************************************/
static inline void cmpGetBitsInit(GetBits* gb, void* buf) {

    gb->buf = buf;
    gb->pos = 0;
}
/******************************************************************************/
static int8 cmpGetBits(GetBits* gb, uint8 nbits) {

    int8 val = gb->buf[0] << gb->pos;

    uint8 buf0bits = 8 - gb->pos;
    if (buf0bits < nbits)
        val |= gb->buf[1] >> buf0bits;

    gb->pos += nbits;
    if (gb->pos >= 8) {
        gb->buf++;
        gb->pos -= 8;
    }

    return val >> (8 - nbits);
}

// Open file for logging
// Inputs:
//   szBase - the base name of the log
FILE * log_file_open() {
    // Downloaded file
    char szFullName[1024] = { 0 };
    char szDateTime[256];
    time_t now = time(NULL);
    // Use date-time to avoid overwriting logs
    strftime(szDateTime, 256, "%Y-%m-%d-%H-%M-%S", localtime(&now));
    // Use other metadata to distinguish each log
    sprintf(szFullName, "Log_%s.log", szDateTime);
    printf("\ndownloading %s ...\n", szFullName);

    FILE * fp = fopen(szFullName, "w");
    return fp;
}

// Dump content of the buffer
int dump_buffer(uint8_t * buf, ssize_t buflen) {
    int i;
    printf("Unhandled (%s): \t", att_op2str(buf[0]));
    for (i = 0; i < buflen; ++i)
        printf("%02x ", buf[i]);
    printf("\n");

    return 0;
}

// Add a single accel log line to the file (and optionally to console)
void add_accel_line(FILE * logFile, const WEDLogAccel * logAccel) {
    char log_line[512] = {0};
    sprintf(log_line, "[\"accelerometer\",[%d,%d,%d]]\n", logAccel->accel[0], logAccel->accel[1], logAccel->accel[2]);
    fprintf(logFile, log_line);
    if (g_opt.console) {
        printf(log_line);
        fflush(stdout);
    }
}

// Continue downloading packets
int process_download(amdev_t * dev, uint8_t * buf, ssize_t buflen) {
    int i;
    uint16_t handle = 0;

    handle = att_get_u16(&buf[1]);
    if (buflen < 4) {
        printf("Last notification handle = 0x%04x\n", handle);
        dev->state = STATE_COUNT;
        return 0;
    }

    // Note: Each packet starts a log entry

    WEDLogAccelCmp logAccelCmp;
    WEDLogLSData logLSData;
    WEDLogTemp logTemp;
    WEDLogLSConfig logLSConfig;

    // TODO: check packet sizes
    // TODO: check data integrity

    int packet_len;
    int payload = 3; // Payload starting position

    WED_LOG_TYPE log_type = buf[payload] & 0x0F;
    while (payload < buflen) {
        if (log_type != WED_LOG_ACCEL_CMP)
            dev->read_logs++; // Total number of log points downloaded so far

        switch (log_type) {
        uint16_t val16;
        uint8_t field_count, reset_detected;
        uint8_t * pdu;
        int nbits;

        case WED_LOG_TIME:
            packet_len = sizeof(dev->logTime);

            if (dev->logFile == NULL)
                dev->logFile = log_file_open();

            dev->logTime.type = log_type;
            dev->logTime.timestamp = att_get_u32(&buf[payload + 1]);
            dev->logTime.flags = buf[payload + 5];
            reset_detected = dev->logTime.flags & TIMESTAMP_REBOOTED;

            if (reset_detected)
                printf(" (reboot detected)\n");

            fprintf(dev->logFile, "[\"timestamp\",[%u,%u]]\n", dev->logTime.timestamp, dev->logTime.flags);

            break;
        case WED_LOG_ACCEL:
            packet_len = sizeof(dev->logAccel);

            if (dev->logFile == NULL)
                dev->logFile = log_file_open();

            dev->bValidAccel = 1;
            dev->logAccel.type = log_type;
            for (i = 0; i < 3; ++i)
                dev->logAccel.accel[i] = buf[payload + 1 + i];
            add_accel_line(dev->logFile, &dev->logAccel);
            break;
        case WED_LOG_LS_CONFIG:
            packet_len = sizeof(logLSConfig);

            if (dev->logFile == NULL)
                dev->logFile = log_file_open();

            logLSConfig.type = log_type;
            logLSConfig.dac_on = buf[payload + 1];
            logLSConfig.reserved = buf[payload + 2];
            logLSConfig.level_led = buf[payload + 3];
            logLSConfig.gain = buf[payload + 4];
            logLSConfig.log_size = buf[payload + 5];
            fprintf(dev->logFile, "[\"lightsensor_config\",[\"dac_on\",%u],"
                    "[\"level_led\",%u],[\"gain\",%u],[\"log_size\",%u]]\n", logLSConfig.dac_on,
                    logLSConfig.level_led, logLSConfig.gain, logLSConfig.log_size);
            break;
        case WED_LOG_LS_DATA:
            logLSData.type = log_type;
            if (dev->ver_flat < FW_VERSION(1,8,84))
            {
                packet_len = 3 + (sizeof(uint16) * (((WEDLogLSData*)&buf[payload])->val[0] >> 14));
                val16 = att_get_u16(&buf[payload + 1]);
                field_count = ((val16 & 0xC000) >> 14) + 1;
                if (field_count > 3) {
                    fprintf(stderr, "Invalid LS_DATA ignored\n");
                    break;
                }
                logLSData.val[0] = val16 & 0x3FFF;
                for (i = 1; i < field_count; ++i)
                    logLSData.val[i] = att_get_u16(&buf[payload + 1 + i * 2]);
                val16 = 0;
                switch(field_count)
                {
                case 1:
                    val16 = 2;
                    break;
                case 2:
                    val16 = 6;
                    break;
                case 3:
                    val16 = 7;
                    break;
                }
            } else {
                packet_len = WEDLogLSDataSize(&buf[payload]);
                val16 = (buf[payload] & 0xE0) >> 5;
                field_count = (val16 & 1 ? 1 : 0) + (val16 & 2 ? 1 : 0) + (val16 & 4 ? 1 : 0);
                for (i = 0; i < field_count; ++i)
                    logLSData.val[i] = att_get_u16(&buf[payload + 1 + i * 2]);
            }

            if (dev->logFile == NULL)
                dev->logFile = log_file_open();

            if (field_count)
            {
                int cnt = 0;
                fprintf(dev->logFile, "[\"lightsensor\"");
                if (val16 & 1 ? 1 : 0)
                    fprintf(dev->logFile, ",[\"red\",%u]", logLSData.val[cnt++]);
                if (val16 & 2 ? 1 : 0)
                    fprintf(dev->logFile, ",[\"ir\",%u]", logLSData.val[cnt++]);
                if (val16 & 4 ? 1 : 0)
                    fprintf(dev->logFile, ",[\"off\",%u]", logLSData.val[cnt++]);
                fprintf(dev->logFile, "]\n");
            }

            break;
        case WED_LOG_TEMP:
            packet_len = sizeof(logTemp);

            if (dev->logFile == NULL)
                dev->logFile = log_file_open();

            logTemp.type = log_type;
            logTemp.temperature = att_get_u16(&buf[payload + 1]);
            fprintf(dev->logFile, "[\"temperature\",%d]\n", logTemp.temperature);
            break;
        case WED_LOG_TAG:
            packet_len = sizeof(dev->logTag);

            if (dev->logFile == NULL)
                dev->logFile = log_file_open();

            dev->logTag.type = log_type;
            dev->logTag.tag = att_get_u32(&buf[payload + 1]);
            fprintf(dev->logFile, "[\"tag\",%u]\n", dev->logTag.tag);
            break;
        case WED_LOG_ACCEL_CMP:
            packet_len = WEDLogAccelCmpSize(&buf[payload]);

            if (dev->logFile == NULL)
                dev->logFile = log_file_open();

            logAccelCmp.type = log_type;
            logAccelCmp.count_bits = buf[payload + 1];
            field_count = (logAccelCmp.count_bits & 0xF) + 1;
            dev->read_logs += field_count;
            if (g_opt.leave_compressed) {
                fprintf(dev->logFile, "[\"accelerometer_compressed\",[\"count_bits\",%u],[\"data\",[",logAccelCmp.count_bits);
                for (i = 0; i < packet_len - 2; ++i) {
                    fprintf(dev->logFile, "%u", buf[payload + 2 + i]);
                    if (i < packet_len - 3)
                        fprintf(dev->logFile, ",");
                }
                fprintf(dev->logFile, "]]]\n");
                break;
            }

            nbits = -1;
            switch ((logAccelCmp.count_bits & 0x70) >> 4) {
            case WED_LOG_ACCEL_CMP_3_BIT:
                nbits = 3;
                break;
            case WED_LOG_ACCEL_CMP_4_BIT:
                nbits = 4;
                break;
            case WED_LOG_ACCEL_CMP_5_BIT:
                nbits = 5;
                break;
            case WED_LOG_ACCEL_CMP_6_BIT:
                nbits = 6;
                break;
            case WED_LOG_ACCEL_CMP_8_BIT:
                nbits = 8;
                dev->bValidAccel = 1;
                break;
            case WED_LOG_ACCEL_CMP_STILL:
                if (logAccelCmp.count_bits & 0x80)
                    nbits = 0;
                break;
            default:
                break;
            }
            if (nbits < 0) {
                fprintf(stderr, "Invalid ACCEL_CMP ignored\n");
                break;
            }


            // We must first get at least one uncompressed accel log
            if (!dev->bValidAccel)
                break;

            if (dev->logFile == NULL)
                dev->logFile = log_file_open();

            if (nbits == 0) {
                // It is still, just replicate
                while (field_count--)
                    add_accel_line(dev->logFile, &dev->logAccel);
                break;
            }
            pdu = &buf[payload];
            pdu += 2;

            if (nbits == 8) {
                while (field_count--) {
                    for (i = 0; i < 3; ++i)
                        dev->logAccel.accel[i] = pdu[i];
                    pdu += 3;
                    add_accel_line(dev->logFile, &dev->logAccel);
                }
            } else {
                GetBits gb;
                cmpGetBitsInit(&gb, pdu);
                while (field_count--) {
                    uint8 i;
                    for (i = 0; i < 3; i++) {
                        int8 diff = cmpGetBits(&gb, nbits);
                        dev->logAccel.accel[i] += diff;
                    }
                    add_accel_line(dev->logFile, &dev->logAccel);
                }
            }

            break;
        default:
            packet_len = (buflen - payload);

            printf("Notification handle: 0x%04x total: %u value: ", handle, (uint32_t)buflen);
            for (i = payload; i < buflen; ++i)
                printf("%02x ", buf[i]);
            printf("\n");
            break;
        }

        // Move forward within aggregate packet
        payload += packet_len;

        //printf("Type: %u, len: %u, total: %u\n", log_type, packet_len, buflen);
        if (payload < buflen)
            log_type = buf[payload] & 0x0F;
    } // end while (payload < buflen

    if (!g_opt.live) {
        printf("\rdownloading ... %u out of %u  (%2.0f%%)", dev->read_logs,
                dev->total_logs, (100.0 * dev->read_logs) / dev->total_logs);
        fflush(stdout);
        if (dev->read_logs >= dev->total_logs || dev->status.num_log_entries == 0)
            dev->state = STATE_COUNT; // Done with command
    }

    return 0;
}

// Set device status
int process_status(amdev_t * dev, uint8_t * buf, ssize_t buflen) {
    uint8_t * pdu = &buf[1];
    dev->status.num_log_entries = att_get_u32(&pdu[0]);
    dev->status.battery_level = pdu[4];
    dev->status.status = pdu[5];
    dev->status.cur_time = att_get_u32(&pdu[6]);
    memcpy(&dev->status.cur_tag, &pdu[10], WED_TAG_SIZE);
    if (buflen >= sizeof(WEDStatus) + 1)
        dev->status.reboot_count = pdu[10 + WED_TAG_SIZE];


    uint32_t tag;
    memcpy(&tag, &dev->status.cur_tag[0], 4);
    printf("\nStatus: Build: %s\t Version: %s \n\t Logs: %u\t Battery: %u%%\t Time: %3.3f s\tReboots: %u\tTag: %10u",
            dev->szBuild, dev->szVersion, dev->status.num_log_entries, dev->status.battery_level,
            dev->status.cur_time * 1.0 / WED_TIME_TICKS_PER_SEC, dev->status.reboot_count, tag);

    if (dev->status.status & STATUS_UPDATE)
        printf(" (Updating) ");
    if (dev->status.status & STATUS_FASTMODE)
        printf(" (Fast Mode) ");
    else if (dev->status.status & STATUS_SLEEPMODE)
        printf(" (Sleep Mode) ");
    else
        printf(" (Slow Mode) ");
    if (dev->status.status & STATUS_CHARGING)
        printf(" (Charging) ");
    if (dev->status.status & STATUS_LS_INPROGRESS)
        printf(" (Light Capture) ");
    printf("\n");

    return 0;
}

// i2c result
int process_debug_i2c(amdev_t * dev, uint8_t * buf, ssize_t buflen) {
    dev->state = STATE_COUNT;
    if (buflen < sizeof(WEDDebugI2CResult) + 1)
        return -1;
    uint8_t * pdu = &buf[1];
    WEDDebugI2CResult i2c_res;
    i2c_res.status = att_get_u8(&pdu[0]);
    i2c_res.data = att_get_u8(&pdu[1]);

    if (i2c_res.status) {
        printf("failed (%d)\n", i2c_res.status);
        return 0;
    }
    printf(" %x\n", i2c_res.data);

    return 0;
}

// State machine to get necessary information.
// Discover device current status, and running firmware
int discover_device(amdev_t * dev) {

    uint16_t handle;

    DISCOVERY_STATE new_state = STATE_NONE;
    switch (dev->state) {
    case STATE_NONE:
        handle = g_char[AMIIGO_UUID_BUILD].value_handle;
        new_state = STATE_BUILD;
        if (handle == 0) {
            // Ignore information
            dev->state = new_state;
            return discover_device(dev);
        }
        break;
    case STATE_BUILD:
        handle = g_char[AMIIGO_UUID_VERSION].value_handle;
        new_state = STATE_VERSION;
        if (handle == 0) {
            // Ignore information
            dev->state = new_state;
            return discover_device(dev);
        }
        break;
    case STATE_VERSION:
        handle = g_char[AMIIGO_UUID_STATUS].value_handle;
        new_state = STATE_STATUS;
        if (handle == 0) {
            fprintf(stderr, "No device status to proceed!\n");
            return -1; // Not ready yet
        }
        break;
    default:
        return 0; // already handled
        break;
    }
    dev->state = new_state;

    // Read the handle of interest
    int ret = exec_read(dev->sock, handle);
    return ret;
}

//----------------------------------------------------------------------------------------
// Process incoming raw data
int process_data(amdev_t * dev, uint8_t * buf, ssize_t buflen) {
    int ret = 0;
    struct att_data_list *list = NULL;
    uint16_t handle = 0;
    uint8_t err = ATT_ECODE_IO;
    int i, j;
    switch (buf[0]) {
    case ATT_OP_ERROR:
        if (buflen > 4)
            err = buf[4];
        if (err == ATT_ECODE_ATTR_NOT_FOUND) {
            // If no battery information, we do not have status
            if (dev->status.battery_level == 0)
                discover_device(dev);
        } else {
            if (buflen > 1)
                handle = att_get_u16(&buf[1]);
            fprintf(stderr, "Error (%s) on handle (0x%4.4x)\n",
                    att_ecode2str(err), handle);
        }
        break;
    case ATT_OP_HANDLE_NOTIFY:
        // Proceed with download
        process_download(dev, buf, buflen);
        break;
    case ATT_OP_READ_BY_TYPE_RESP:
        list = dec_read_by_type_resp((uint8_t *) buf, buflen);
        if (list == NULL)
            return -1;

        handle = 0;
        for (i = 0; i < list->num; i++) {
            uint8_t *value = list->data[i];
            bt_uuid_t uuid;

            handle = att_get_u16(value);
            if (list->len == 7) {
                bt_uuid_t uuid16 = att_get_uuid16(&value[5]);
                bt_uuid_to_uuid128(&uuid16, &uuid);
            } else {
                uuid = att_get_uuid128(&value[5]);
            }
            uint8_t properties = value[2];
            uint16_t value_handle = att_get_u16(&value[3]);
            for (j = 0; j < AMIIGO_UUID_COUNT; ++j) {
                if (bt_uuid_cmp(&uuid, &g_char[j].uuid) == 0) {
                    g_char[j].handle = handle;
                    g_char[j].properties = properties;
                    g_char[j].value_handle = value_handle;
                    break;
                }
            }
            char str_uuid[MAX_LEN_UUID_STR + 1] = { 0 };
            bt_uuid_to_string(&uuid, str_uuid, sizeof(str_uuid));
            printf("handle: 0x%04x\t properties: 0x%04x\t value handle: 0x%04x\t UUID: %s \n",
                    handle, properties, value_handle, str_uuid);
        } // end for(i = 0

        // Get the rest of handles
        if (handle != 0 && (handle + 1) < OPT_END_HANDLE) {
            discover_handles(dev->sock, handle + 1, OPT_END_HANDLE);
        }
        break;
    case ATT_OP_READ_RESP:
        switch (dev->state) {
        case STATE_BUILD:
            strncpy(dev->szBuild, (const char *) &buf[1], buflen - 1);
            // More to discover
            discover_device(dev);
            break;
        case STATE_VERSION:
            dev->ver.Major = buf[1];
            dev->ver.Minor = buf[2];
            dev->ver.Build = att_get_u16(&buf[3]);
            dev->ver_flat = FW_VERSION(dev->ver.Major, dev->ver.Minor, dev->ver.Build);
            sprintf(dev->szVersion, "%u.%u.%u%s", dev->ver.Major, dev->ver.Minor, dev->ver.Build,
                    dev->ver_flat < FW_VERSION(1,8,89) ? " (< 1.8.89: incompatible config)" : "");
            // More to discover
            discover_device(dev);
            break;
        case STATE_STATUS:
            process_status(dev, buf, buflen);
            break;
        case STATE_DOWNLOAD:
            // This must be keep-alive
            process_status(dev, buf, buflen);
            break;
        case STATE_FWSTATUS:
        case STATE_FWSTATUS_WAIT:
            // Act upon new firmware update status
            ret = process_fwstatus(dev, buf, buflen);
            break;
        case STATE_I2C:
            // i2c result
            ret = process_debug_i2c(dev, buf, buflen);
            break;
        default:
            dump_buffer(buf, buflen);
            break;
        }
        break;
    default:
        dump_buffer(buf, buflen);
        break;
    }

    if (list != NULL)
        att_data_list_free(list);

    return ret;
}
