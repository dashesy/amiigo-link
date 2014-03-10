/*
 * Amiigo Link firmware update process
 *
 * @date March 10, 2014
 * @author: dashesy
 */

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

#include "common.h"
#include "amidefs.h"
#include "amdev.h"
#include "cmdparse.h"

#define FWUP_HDR_ID 0x0101

uint32_t g_fwup_speedup = 1; // How much to overload firmware update

FILE * g_fwImageFile = NULL; // Firmware image file
uint32_t g_fwImageSize = 0; // Firmware image size in bytes
uint32_t g_fwImageWrittenSize = 0; // bytes transmitted
uint16_t g_fwImagePage = 0; // Firmwate image total pages
uint16_t g_fwImageWrittenPage = 0; // pages transmitted

int set_update_file(const char * szName) {
    g_fwImageFile = fopen(szName, "r");
    if (g_fwImageFile == NULL) {
        fprintf(stderr, "Firmware image file (%s) not accessible!\n", szName);
        return -1;
    }
    fseek(g_fwImageFile, 0, SEEK_END);
    g_fwImageSize = ftell(g_fwImageFile);
    fseek(g_fwImageFile, 0, SEEK_SET);
    if (g_fwImageSize < WED_FW_HEADER_SIZE) {
        fprintf(stderr, "Firmware image file (%s) too small!\n", szName);
        return -1;
    }
    WEDFirmwareCommand fwcmd;
    memset(&fwcmd, 0, sizeof(fwcmd));
    fwcmd.pkt_type = WED_FIRMWARE_INIT;
    fread(fwcmd.header, WED_FW_HEADER_SIZE, 1, g_fwImageFile);

    uint16_t * hdr = (uint16_t *) &fwcmd.header[0];
    // TODO: check CRC
    //uint16_t fw_crc = hdr[0];
    uint16_t fw_id = hdr[1];
    g_fwImagePage = hdr[2];

    if (fw_id != FWUP_HDR_ID) {
        fprintf(stderr, "Firmware image (%s) invalid!\n", szName);
        return -1;
    }

    if (WED_FW_BLOCK_SIZE * WED_FW_STREAM_BLOCKS * g_fwImagePage != g_fwImageSize) {
        fprintf(stderr, "Firmware image (%s) invalid size!\n", szName);
        return -1;
    }

    g_fwImageWrittenSize = 0;
    g_fwImageWrittenPage = 0;
    fseek(g_fwImageFile, 0, SEEK_SET);

    g_cmd = AMIIGO_CMD_FWUPDATE;
    return 0;
}

// Firmware update in progress
int process_fwstatus(amdev_t * dev, uint8_t * buf, ssize_t buflen) {
    int ret = 0;
    WEDFirmwareStatus fwstatus;
    memset(&fwstatus, 0, sizeof(fwstatus));
    fwstatus.status = buf[1];
    fwstatus.error_code = buf[2];

    uint16_t handle = g_char[AMIIGO_UUID_FIRMWARE].value_handle;

    if (dev->state == STATE_FWSTATUS)
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
        if (dev->state == STATE_FWSTATUS)
        {
            WEDFirmwareCommand fwcmd;
            memset(&fwcmd, 0, sizeof(fwcmd));
            fwcmd.pkt_type = WED_FIRMWARE_INIT;
            fread(fwcmd.header, WED_FW_HEADER_SIZE, 1, g_fwImageFile);
            fseek(g_fwImageFile, 0, SEEK_SET);
            g_fwImageWrittenSize = 0;

            ret = exec_write(dev->sock, handle, (uint8_t *) &fwcmd, sizeof(fwcmd));
            if (ret)
                return -1;
            // We have already written the header
            dev->state = STATE_FWSTATUS_WAIT;
        }
        ret = exec_read(dev->sock, handle); // Continue polling
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
                    if (g_opt.verbosity)
                        printf("(ended)\n");
                    break;
                }
                if (len < WED_FW_BLOCK_SIZE) {
                    bFinished = 1;
                    printf("(uneven image size!)\n");
                    break;
                }
                g_fwImageWrittenSize += len;
                ret = exec_write(dev->sock, handle, (uint8_t *) &fwdata, sizeof(fwdata));
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

            ret = exec_write(dev->sock, handle, (uint8_t *) &fwcmd, sizeof(fwcmd));
        }
        fflush(stdout);

        ret = exec_read(dev->sock, handle); // Continue polling
    } else if (fwstatus.status == WED_FWSTATUS_UPDATE_READY) {
        if (g_fwImageWrittenSize != g_fwImageSize) {
            fprintf(stderr, " Update not ready!\n");
            return -1;
        }
        WEDFirmwareCommand fwcmd;
        memset(&fwcmd, 0, sizeof(fwcmd));
        fwcmd.pkt_type = WED_FIRMWARE_UPDATE;

        ret = exec_write(dev->sock, handle, (uint8_t *) &fwcmd, sizeof(fwcmd));
        printf(" (Updating done)\n");
        // Done with command
        dev->state = STATE_COUNT;
    } else {
        fprintf(stderr, "Unknown firmware update state (%u)\n", fwstatus.status);
        ret = -1;
    }
    return ret;
}
