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

#include "att.h"

#include "amidefs.h"
#include "amprocess.h"

// Open file for logging
// Inputs:
//   szBase - the base name of the log
FILE * log_file_open() {
    char szDateTime[256];
    time_t now = time(NULL);
    // Use date-time to avoid overwriting logs
    strftime(szDateTime, 256, "%Y-%m-%d-%H-%M-%S", localtime(&now));
    // Use other metadata to distinguish each log
    sprintf(g_szFullName, "Log_%s.log", szDateTime);

    FILE * fp = fopen(g_szFullName, "w");
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

// Firmware update in progress
int process_fwstatus(uint8_t * buf, ssize_t buflen) {
    int ret = 0;
    WEDFirmwareStatus fwstatus;
    memset(&fwstatus, 0, sizeof(fwstatus));
    fwstatus.status = buf[1];
    fwstatus.error_code = buf[2];

    uint16_t handle = g_char[AMIIGO_UUID_FIRMWARE].value_handle;

    if (g_state == STATE_FWSTATUS)
    {
        if (fwstatus.status != WED_FWSTATUS_IDLE)
        {
            printf("Unfinished previous update detected.\n"
                   "Reset CPU and try again\n");
            return -1;
        }
    }

    if (fwstatus.status == WED_FWSTATUS_WAIT
            || fwstatus.status == WED_FWSTATUS_IDLE) {
        if (g_state == STATE_FWSTATUS)
        {
            WEDFirmwareCommand fwcmd;
            memset(&fwcmd, 0, sizeof(fwcmd));
            fwcmd.pkt_type = WED_FIRMWARE_INIT;
            fread(fwcmd.header, WED_FW_HEADER_SIZE, 1, g_fwImageFile);
            fseek(g_fwImageFile, 0, SEEK_SET);
            g_fwImageWrittenSize = 0;

            ret = exec_write(handle, (uint8_t *) &fwcmd, sizeof(fwcmd));
            if (ret)
                return -1;
            // We have already written the header
            g_state = STATE_FWSTATUS_WAIT;
        }
        ret = exec_read(handle); // Continue polling
    } else if (fwstatus.status == WED_FWSTATUS_ERROR) {
        switch (fwstatus.error_code) {
        case WED_FWERROR_HEADER:
            fprintf(stderr, "Firmware image header was unrecognized\n");
            break;
        case WED_FWERROR_SIZE:
            fprintf(stderr, "Lost packets or image size was too large\n");
            break;
        case WED_FWERROR_CRC:
            fprintf(stderr, "CRC check failed\n");
            break;
        case WED_FWERROR_FLASH:
            fprintf(stderr, "SPI flash error\n");
            break;
        case WED_FWERROR_OTHER:
            fprintf(stderr, "Internal error\n");
            break;
        default:
            fprintf(stderr, "Unknown error (%d)\n", fwstatus.status);
            break;
        }
        ret = -1;
    } else if (fwstatus.status == WED_FWSTATUS_UPLOAD_READY) {

        int bRetry = 0;
        int bFinished = 0;
        int i, j;
        for (j = 0; j < g_fwup_speedup && !bFinished && !bRetry; ++j)
        {
            for (i = 0; i < WED_FW_STREAM_BLOCKS; ++i) {
                WEDFirmwareCommand fwdata;
                memset(&fwdata, 0, sizeof(fwdata));
                fwdata.pkt_type = WED_FIRMWARE_DATA_BLOCK;
                long int offset = ftell(g_fwImageFile);
                size_t len = fread(fwdata.data, 1, WED_FW_BLOCK_SIZE, g_fwImageFile);
                if (len == 0) {
                    bFinished = 1;
                    if (g_bVerbose)
                        printf("(ended)\n");
                    break;
                }
                if (len < WED_FW_BLOCK_SIZE) {
                    bFinished = 1;
                    printf("(uneven image size!)\n");
                    break;
                }
                g_fwImageWrittenSize += len;
                ret = exec_write(handle, (uint8_t *) &fwdata, sizeof(fwdata));
                if (ret)
                {
                    fseek(g_fwImageFile, offset, SEEK_SET);
                    bRetry = 1;
                    printf("(write retry)\n");
                    break;
                }
                if (feof(g_fwImageFile) || g_fwImageWrittenSize == g_fwImageSize)
                {
                    bFinished = 1;
                    break;
                }
            } // end for (i
            g_fwImageWrittenPage++;
            printf("\rUpdating ... %u/%u  page %u/%u (%2.0f%%)", g_fwImageWrittenSize, g_fwImageSize,
                    g_fwImageWrittenPage, g_fwImagePage, (g_fwImageWrittenSize * 100.0) / g_fwImageSize);
            fflush(stdout);
            usleep(100);
        }

        // Done uploding in our end
        if (bFinished) {
            printf(" (data done)");
            WEDFirmwareCommand fwcmd;
            memset(&fwcmd, 0, sizeof(fwcmd));
            fwcmd.pkt_type = WED_FIRMWARE_DATA_DONE;

            ret = exec_write(handle, (uint8_t *) &fwcmd, sizeof(fwcmd));
        }
        fflush(stdout);

        ret = exec_read(handle); // Continue polling
    } else if (fwstatus.status == WED_FWSTATUS_UPDATE_READY) {
        if (g_fwImageWrittenSize != g_fwImageSize) {
            fprintf(stderr, " Update not ready!\n");
            return -1;
        }
        WEDFirmwareCommand fwcmd;
        memset(&fwcmd, 0, sizeof(fwcmd));
        fwcmd.pkt_type = WED_FIRMWARE_UPDATE;

        ret = exec_write(handle, (uint8_t *) &fwcmd, sizeof(fwcmd));
        printf(" (Updating done)\n");
        // Done with command
        g_state = STATE_COUNT;
    } else {
        fprintf(stderr, "Unknown firmware update state (%u)\n", fwstatus.status);
        ret = -1;
    }
    return ret;
}

// Continue downloading packets
int process_download(uint8_t * buf, ssize_t buflen) {
    int i;
    uint16_t handle = 0;

    handle = att_get_u16(&buf[1]);
    if (buflen < 4) {
        printf("Last notification handle = 0x%04x\n", handle);
        g_state = STATE_COUNT;
        return 0;
    }

    // Note: Each packet starts a log entry

    WEDLogAccelCmp logAccelCmp;
    WEDLogLSData logLSData;
    WEDLogTemp logTemp;

    // TODO: check packet sizes
    // TODO: check data integrity

    int packet_len;
    int payload = 3; // Payload starting position

    WED_LOG_TYPE log_type = buf[payload] & 0x0F;
    while (payload < buflen) {
        if (log_type != WED_LOG_ACCEL_CMP)
            g_read_logs++; // Total number of log points downloaded so far

        switch (log_type) {
        uint16_t val16;
        uint8_t field_count, reset_detected;
        uint8_t * pdu;
        int nbits;

        case WED_LOG_TIME:
            packet_len = sizeof(g_logTime);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            g_logTime.type = log_type;
            g_logTime.timestamp = att_get_u32(&buf[payload + 1]);
            g_logTime.flags = buf[payload + 5];
            reset_detected = g_logTime.flags & TIMESTAMP_REBOOTED;

            if (reset_detected)
                printf(" (reboot detected)\n");

            fprintf(g_logFile, "[\"timestamp\",[%u,%u]]\n", g_logTime.timestamp, g_logTime.flags);

            break;
        case WED_LOG_ACCEL:
            packet_len = sizeof(g_logAccel);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            g_bValidAccel = 1;
            g_logAccel.type = log_type;
            for (i = 0; i < 3; ++i)
                g_logAccel.accel[i] = buf[payload + 1 + i];
            fprintf(g_logFile, "[\"accelerometer\",[%d,%d,%d]]\n", g_logAccel.accel[0],
                    g_logAccel.accel[1], g_logAccel.accel[2]);
            if (g_console) {
                printf("[\"accelerometer\",[%02d,%02d,%02d]]\n", g_logAccel.accel[0],
                        g_logAccel.accel[1], g_logAccel.accel[2]);
                fflush(stdout);

            }
            break;
        case WED_LOG_LS_CONFIG:
            packet_len = sizeof(g_logLSConfig);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            g_logLSConfig.type = log_type;
            g_logLSConfig.dac_on = buf[payload + 1];
            g_logLSConfig.reserved = buf[payload + 2];
            g_logLSConfig.level_led = buf[payload + 3];
            g_logLSConfig.gain = buf[payload + 4];
            g_logLSConfig.log_size = buf[payload + 5];
            fprintf(g_logFile, "[\"lightsensor_config\",[\"dac_on\",%u],"
                    "[\"level_led\",%u],[\"gain\",%u],[\"log_size\",%u]]\n", g_logLSConfig.dac_on,
                    g_logLSConfig.level_led, g_logLSConfig.gain, g_logLSConfig.log_size);
            break;
        case WED_LOG_LS_DATA:
            logLSData.type = log_type;
            if (g_flat_ver < FW_VERSION(1,8,84))
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

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            if (field_count)
            {
                int cnt = 0;
                fprintf(g_logFile, "[\"lightsensor\"");
                if (val16 & 1 ? 1 : 0)
                    fprintf(g_logFile, ",[\"red\",%u]", logLSData.val[cnt++]);
                if (val16 & 2 ? 1 : 0)
                    fprintf(g_logFile, ",[\"ir\",%u]", logLSData.val[cnt++]);
                if (val16 & 4 ? 1 : 0)
                    fprintf(g_logFile, ",[\"off\",%u]", logLSData.val[cnt++]);
                fprintf(g_logFile, "]\n");
            }

            break;
        case WED_LOG_TEMP:
            packet_len = sizeof(logTemp);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            logTemp.type = log_type;
            logTemp.temperature = att_get_u16(&buf[payload + 1]);
            fprintf(g_logFile, "[\"temperature\",%d]\n", logTemp.temperature);
            break;
        case WED_LOG_TAG:
            packet_len = sizeof(g_logTag);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            g_logTag.type = log_type;
            g_logTag.tag = att_get_u32(&buf[payload + 1]);
            fprintf(g_logFile, "[\"tag\",%u]\n", g_logTag.tag);
            break;
        case WED_LOG_ACCEL_CMP:
            packet_len = WEDLogAccelCmpSize(&buf[payload]);

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            logAccelCmp.type = log_type;
            logAccelCmp.count_bits = buf[payload + 1];
            field_count = (logAccelCmp.count_bits & 0xF) + 1;
            g_read_logs += field_count;
            if (!g_decompress) {
                fprintf(g_logFile, "[\"accelerometer_compressed\",[\"count_bits\",%u],[\"data\",[",logAccelCmp.count_bits);
                for (i = 0; i < packet_len - 2; ++i) {
                    fprintf(g_logFile, "%u", buf[payload + 2 + i]);
                    if (i < packet_len - 3)
                        fprintf(g_logFile, ",");
                }
                fprintf(g_logFile, "]]]\n");
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
                g_bValidAccel = 1;
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
            if (!g_bValidAccel)
                break;

            if (g_logFile == NULL)
                g_logFile = log_file_open();

            if (nbits == 0) {
                // It is still, just replicate
                while (field_count--) {
                    fprintf(g_logFile, "[\"accelerometer\",[%d,%d,%d]]\n", g_logAccel.accel[0],
                            g_logAccel.accel[1], g_logAccel.accel[2]);
                    if (g_console) {
                        printf("[\"accelerometer\",[%02d,%02d,%02d]]\n", g_logAccel.accel[0],
                                g_logAccel.accel[1], g_logAccel.accel[2]);
                        fflush(stdout);
                    }
                }
                break;
            }
            pdu = &buf[payload];
            pdu += 2;

            if (nbits == 8) {
                while (field_count--) {
                    for (i = 0; i < 3; ++i)
                        g_logAccel.accel[i] = pdu[i];
                    pdu += 3;
                    fprintf(g_logFile, "[\"accelerometer\",[%d,%d,%d]]\n", g_logAccel.accel[0],
                            g_logAccel.accel[1], g_logAccel.accel[2]);
                    if (g_console) {
                        printf("[\"accelerometer\",[%02d,%02d,%02d]]\n", g_logAccel.accel[0],
                                g_logAccel.accel[1], g_logAccel.accel[2]);
                        fflush(stdout);

                    }
                }
            } else {
                GetBits gb;
                cmpGetBitsInit(&gb, pdu);
                while (field_count--) {
                    uint8 i;
                    for (i = 0; i < 3; i++) {
                        int8 diff = cmpGetBits(&gb, nbits);
                        g_logAccel.accel[i] += diff;
                    }
                    fprintf(g_logFile, "[\"accelerometer\",[%d,%d,%d]]\n", g_logAccel.accel[0],
                            g_logAccel.accel[1], g_logAccel.accel[2]);
                    if (g_console) {
                        printf("[\"accelerometer\",[%02d,%02d,%02d]]\n", g_logAccel.accel[0],
                                g_logAccel.accel[1], g_logAccel.accel[2]);
                        fflush(stdout);

                    }
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

    if (!g_live) {
        printf("\rdownloading ... %u out of %u  (%2.0f%%)", g_read_logs,
                g_total_logs, (100.0 * g_read_logs) / g_total_logs);
        if (g_read_logs >= g_total_logs || g_status.num_log_entries == 0)
        {
            printf("\n%s", g_szFullName);
            g_state = STATE_COUNT; // Done with command
        }
        fflush(stdout);
    }

    return 0;
}

// Set device status
int process_status(uint8_t * buf, ssize_t buflen) {
    uint8_t * pdu = &buf[1];
    g_status.num_log_entries = att_get_u32(&pdu[0]);
    g_status.battery_level = pdu[4];
    g_status.status = pdu[5];
    g_status.cur_time = att_get_u32(&pdu[6]);
    memcpy(&g_status.cur_tag, &pdu[10], WED_TAG_SIZE);
    if (buflen >= sizeof(WEDStatus) + 1)
        g_status.reboot_count = pdu[10 + WED_TAG_SIZE];


    printf("\nStatus: Build: %s\t Version: %s \n\t Logs: %u\t Battery: %u%%\t Time: %3.3f s\tReboots: %u\t",
            g_szBuild, g_szVersion, g_status.num_log_entries, g_status.battery_level,
            g_status.cur_time * 1.0 / WED_TIME_TICKS_PER_SEC, g_status.reboot_count);

    if (g_status.status & STATUS_UPDATE)
        printf(" (Updating) ");
    if (g_status.status & STATUS_FASTMODE)
        printf(" (Fast Mode) ");
    else if (g_status.status & STATUS_SLEEPMODE)
        printf(" (Sleep Mode) ");
    else
        printf(" (Slow Mode) ");
    if (g_status.status & STATUS_CHARGING)
        printf(" (Charging) ");
    if (g_status.status & STATUS_LS_INPROGRESS)
        printf(" (Light Capture) ");
    printf("\n");

    return 0;
}

// i2c result
int process_debug_i2c(uint8_t * buf, ssize_t buflen) {
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

//----------------------------------------------------------------------------------------
// Process incoming raw data
int process_data(uint8_t * buf, ssize_t buflen) {
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
            if (g_status.battery_level == 0)
                discover_device();
        } else {
            if (buflen > 1)
                handle = att_get_u16(&buf[1]);
            fprintf(stderr, "Error (%s) on handle (0x%4.4x)\n",
                    att_ecode2str(err), handle);
        }
        break;
    case ATT_OP_HANDLE_NOTIFY:
        // Proceed with download
        process_download(buf, buflen);
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
            discover_handles(handle + 1, OPT_END_HANDLE);
        }
        break;
    case ATT_OP_READ_RESP:
        switch (g_state) {
        case STATE_BUILD:
            strncpy(g_szBuild, (const char *) &buf[1], buflen - 1);
            // More to discover
            discover_device();
            break;
        case STATE_VERSION:
            g_ver.Major = buf[1];
            g_ver.Minor = buf[2];
            g_ver.Build = att_get_u16(&buf[3]);
            g_flat_ver = FW_VERSION(g_ver.Major, g_ver.Minor, g_ver.Build);
            sprintf(g_szVersion, "%u.%u.%u%s", g_ver.Major, g_ver.Minor, g_ver.Build,
                    g_flat_ver < FW_VERSION(1,8,89) ? " (< 1.8.89: incompatible config)" : "");
            // More to discover
            discover_device();
            break;
        case STATE_STATUS:
            process_status(buf, buflen);
            // Done with discovery, perform the command
            if (g_cmd == AMIIGO_CMD_NONE) {
                g_state = STATE_COUNT;
            } else {
                // Now that we have status (e.g. number of logs)
                //  Start execution of the requested command
                ret = exec_command();
            }
            break;
        case STATE_DOWNLOAD:
            // This must be keep-alive
            process_status(buf, buflen);
            break;
        case STATE_FWSTATUS:
        case STATE_FWSTATUS_WAIT:
            // Act upon new firmware update status
            ret = process_fwstatus(buf, buflen);
            break;
        case STATE_I2C:
            // i2c result
            ret = process_debug_i2c(buf, buflen);
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
