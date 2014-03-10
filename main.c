/*
 * Amiigo offline utility
 *
 * @date Aug 25, 2013
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

#include "jni/bluetooth.h"
#include "jni/hci.h"
#include "jni/hci_lib.h"
#if 0
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#endif

#include "btio.h"
#include "att.h"
#include "hcitool.h"
#include "amidefs.h"
#include "amdev.h"

#include "gapproto.h"
#include "amlprocess.h"
#include "cmdparse.h"

#define FWUP_HDR_ID 0x0101

// Device and interface to use
char g_dst[512] = "";
char g_src[512] = "hci0";

// Firmware build and version information
char g_szBuild[512] = "Unknown";
char g_szVersion[512] = "Unknown";

// Optional all-inclusive handles
#define OPT_START_HANDLE 0x0001
#define OPT_END_HANDLE   0xffff

AMIIGO_CMD g_cmd = AMIIGO_CMD_NONE;

// Amiigo devices to interact with
amdev_t devices[2];

WEDDebugI2CCmd g_i2c; // i2c debugging
WEDConfigLS g_config_ls; // Light configuration
WEDConfigAccel g_config_accel; // Acceleration sensors configuration
WEDMaintLED g_maint_led; // Blink command



FILE * g_logFile = NULL; // file to download logs

FILE * g_fwImageFile = NULL; // Firmware image file
uint32_t g_fwImageSize = 0; // Firmware image size in bytes
uint32_t g_fwImageWrittenSize = 0; // bytes transmitted
uint16_t g_fwImagePage = 0; // Firmwate image total pages
uint16_t g_fwImageWrittenPage = 0; // pages transmitted

//-------- flags -----------
int g_leave_compressed = 0; // If packets should be left in compressed form
int g_raw = 0; // If should download logs in raw format (no compression in firmware)
int g_verbosity = 0; // verbose output
int g_live = 0; // if live output is needed
int g_console = 0; // if should print accel to console
int g_bFull = 0; // If full characteristcs should be discovered

// Downloaded file
char g_szFullName[1024] = { 0 };

// Execute the requested command
int exec_command(int sock) {
    switch (g_cmd) {
    case AMIIGO_CMD_DOWNLOAD:
        return exec_download(sock);
        break;
    case AMIIGO_CMD_CONFIGLS:
        g_state = STATE_COUNT; // Done with command
        return exec_configls(sock);
        break;
    case AMIIGO_CMD_CONFIGACCEL:
        g_state = STATE_COUNT; // Done with command
        return exec_configaccel(sock);
        break;
    case AMIIGO_CMD_BLINK:
        g_state = STATE_COUNT; // Done with command
        return exec_blink(sock);
        break;
    case AMIIGO_CMD_RESET_CPU:
    case AMIIGO_CMD_RESET_LOGS:
    case AMIIGO_CMD_RESET_CONFIGS:
        return exec_reset(sock, g_cmd);
        break;
    case AMIIGO_CMD_FWUPDATE:
        return exec_fwupdate(sock);
        break;
    case AMIIGO_CMD_I2C_READ:
    case AMIIGO_CMD_I2C_WRITE:
        return exec_debug_i2c(sock);
        break;
    default:
        return 0;
        break;
    }
    return 0;
}

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


void show_usage_screen(void) {
    printf("Amiigo Link command line utility.\n");
    printf("Usage: amlink [options] [command]\n"
            "Options:\n"
            "  -v, --verbose Verbose mode \n"
            "    More messages are dumped to console.\n"
            "  --full Full characteristics discovery. \n"
            "    If specified handles are queried.\n"
            "  --i, --adapter uuid|hci<N>[,...]\n"
            "    Interface adapter(s) to use (default is hci0)\n"
            "  --b, --device uuid1[,...] \n"
            "    Amiigo device(s) to connect to, default is shoepod then wristband.\n"
            "    Example: --b 90:59:AF:04:32:82\n"
            "    Use --lescan to find the UUID list\n"
            "  --lescan \n"
            "    Low energy scan (needs root priviledge)\n"
            "  --c, --command cmd \n"
            "    Command to execute:\n"
            "       status: (default) perform device discovery\n"
            "       download: download the logs\n"
            "       configls: configure light sensor\n"
            "       configaccel: configure acceleration sensors\n"
            "       blink: configure blink LED\n"
            "       resetlogs: reset buffered logs\n"
            "       resetcpu: restart the board\n"
            "       resetconfigs: set all configs to default\n"
            "  --input file\n"
            "    Configuration file to use for given command.\n"
            "  -l, --live Live download as a stream.\n"
            "    Hit `q` to end the stream.\n"
            "  --print\n"
            "    Print accelerometer to console as well as the log file.\n"
            "  --fwupdate file\n"
            "    Firmware image file to to use for update.\n"
            "  --i2c_read address:reg\n"
            "    Read i2c address and register (debugging only).\n"
            "  --i2c_write address:reg:value\n"
            "    Write value to i2c address and register (debugging only).\n"
            "  --compressed Leave logs in compressed form.\n"
            "  --raw Download logs in raw format (no compression).\n"
            "  --help         Display this usage screen\n");
    printf("\namlink is Copyright Amiigo inc\n");
}

static void do_command_line(int argc, char * const argv[]) {
    // Parse the input
    while (1) {
        int c;
        int option_index = 0;
        static struct option long_options[] = {
              { "verbose", 0, 0, 'v' },
              { "live", 0, 0, 'l' },
              { "full", 0, 0, 'a' },
              { "compressed", 0, 0, 'p'},
              { "raw", 0, 0, 'r'},
              { "lescan", 0, 0, 's'  },
              { "i", 1, 0, 'i' },
              { "adapter", 1, 0, 'i' },
              { "b", 1, 0, 'b' },
              { "device", 1, 0, 'b' },
              { "c", 1, 0, 'x' },
              { "command", 1, 0, 'x' },
              { "input", 1, 0, 'f' },
              { "print", 0, 0, 'o' },
              { "i2c_read", 1, 0, 'd'},
              { "i2c_write", 1, 0, 'w'},
              { "fwupdate", 1, 0, 'u' },
              { "help", 0, 0, '?' },
              { 0, 0, 0, 0 } };

        c = getopt_long(argc, argv, "vl?", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            printf("option %s", long_options[option_index].name);
            if (optarg)
                printf(" with arg %s", optarg);
            printf("unsupported\n");
            break;

        case 's':
            do_lescan();
            exit(0);
            break;

        case 'o':
            g_console = 1;
            break;

        case 'l':
            g_live = 1;
            break;

        case 'v':
            g_verbosity = 1;
            break;

        case 'a':
            g_bFull = 1;
            break;

        case 'p':
            g_leave_compressed = 1;
            break;

        case 'r':
            g_raw = 1;
            break;

        case 'i':
            if (set_adapter(optarg))
                exit(1);
            break;

        case 'b':
            if (set_device(optarg))
                exit(1);
            break;

        case 'x':
            if (set_command(optarg))
                exit(1);
            break;

        case 'f':
            if (set_input_file(optarg))
                exit(1);
            break;

        case 'u':
            if (set_update_file(optarg))
                exit(1);
            break;

        case 'd':
            if (set_i2c_read(optarg))
                exit(1);
            break;

        case 'w':
            if (set_i2c_write(optarg))
                exit(1);
            break;

        case '?':
            show_usage_screen();
            exit(0);
            break;

        default:
            printf("?? getopt returned character code 0%o ??\n", c);
            break;
        }
    }

    if (optind < argc) {
        printf("Unrecognized command line arguments: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        printf("\n");
        show_usage_screen();
        exit(1);
    }
}

char kbhit() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO);
    newt.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int nread = read(STDIN_FILENO, &ch, 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (nread == 1)
        return ch;
    else
        return 0;
}

int main(int argc, char **argv) {
    int ret;
    time_t start_time, stop_time, download_time;

    //---------- config packets ------------
    memset(&g_config_ls, 0, sizeof(g_config_ls));
    memset(&g_config_accel, 0, sizeof(g_config_accel));
    memset(&g_maint_led, 0, sizeof(g_maint_led));
    memset(&g_i2c, 0, sizeof(g_i2c));

    // Some defulats
    g_maint_led.duration = 5;
    g_maint_led.led = 6;
    g_maint_led.speed = 1;

    // zero-fill the devices state
    memset(&devices[0], 0, sizeof(devices));


    // Initialize the processing logic
    init_process();

    // Set parameters based on command line
    do_command_line(argc, argv);

    int sock; // Link socket(s)

    // Connect to all devices
    sock = gap_connect(g_src, g_dst);


    if (g_bFull) {
        // Start by discovering Amiigo handles
        ret = discover_handles(OPT_START_HANDLE, OPT_END_HANDLE);
        if (ret) {
            fprintf(stderr, "discover_handles() error\n");
            return -1;
        }
    } else {
        // Use default handles and discover the device
        ret = discover_device();
        if (ret) {
            fprintf(stderr, "discover_device() error\n");
            return -1;
        }
    }

    start_time = time(NULL);
    stop_time = start_time;
    download_time = start_time;

    for (;;) {
        // No need to keep-alive during firmware update
        if (g_state != STATE_FWSTATUS_WAIT) {
            stop_time = time(NULL);
            double diff = difftime(stop_time, start_time);
            // Keep-alive by reading status every 60s
            if (diff > 60) {
                if (g_verbosity)
                    printf(" (Keep alive)\n");
                start_time = stop_time;
                // Read the status to keep conection alive
                exec_status();
            }
            if (g_state == STATE_DOWNLOAD && !g_live) {
                diff = difftime(stop_time, download_time);
                // Download timeout reached
                if (diff > 2)
                {
                    printf(" (Timeout)\n");
                    break;
                }
            }
        }

        uint8_t buf[1024] = {0};
        int len = gap_recv(sock, &buf[0], sizeof(buf));
        if (len < 0)
            break;
        if (len == 0)
            continue;

        download_time = stop_time;
        // Process incoming data
        ret = process_data(sock, buf, len);
        if (ret) {
            fprintf(stderr, "main process_data() error\n");
            break;
        }
        if (g_state == STATE_COUNT)
            break; // Done the the command

        // See if user ended the run
        if (kbhit() == 'q')
            break;
    } //end for(;;

    // Reset CPU if need to exit in the middle of firmware update
    if (g_state == STATE_FWSTATUS_WAIT)
        exec_reset(sock, AMIIGO_CMD_RESET_CPU);

    printf("\n");

    // Close the socket
    gap_shutdown(g_sock);

    // Close log files
    if (g_logFile != NULL)
    {
        fclose(g_logFile);
        g_logFile = NULL;
    }

    // Close fw iamge file
    if (g_fwImageFile != NULL) {
        fclose(g_fwImageFile);
        g_fwImageFile = NULL;
    }

    return 0;
}
