/*
 * Amiigo offline utility
 *
 * @date Aug 25, 2013
 * @author: dashesy
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

#include "amidefs.h"
#include "amdev.h"
#include "hcitool.h"
#include "common.h"
#include "gapproto.h"
#include "amproto.h"
#include "amlprocess.h"
#include "cmdparse.h"
#include "fwupdate.h"

extern void char_init(void);

char g_src[512] = "hci0";

aml_options_t g_opt; // flags option

// Execute the requested command
int exec_command(amdev_t * dev) {
    switch (g_cmd) {
    case AMIIGO_CMD_NONE:
        dev->state = STATE_COUNT; // Done with command
        break;
    case AMIIGO_CMD_DOWNLOAD:
        if (dev->status.num_log_entries == 0 && !g_opt.live) {
            // Nothing to download!
            dev->state = STATE_COUNT; // Done with command
            return 0;
        }

        dev->state = STATE_DOWNLOAD; // Download in progress
        dev->total_logs = dev->status.num_log_entries; // How many logs to download
        return exec_download(dev->sock);
        break;
    case AMIIGO_CMD_CONFIGLS:
        dev->state = STATE_COUNT; // Done with command
        return exec_configls(dev->sock);
        break;
    case AMIIGO_CMD_CONFIGACCEL:
        dev->state = STATE_COUNT; // Done with command
        return exec_configaccel(dev->sock);
        break;
    case AMIIGO_CMD_BLINK:
        dev->state = STATE_COUNT; // Done with command
        return exec_blink(dev->sock);
        break;
    case AMIIGO_CMD_RESET_CPU:
    case AMIIGO_CMD_RESET_LOGS:
    case AMIIGO_CMD_RESET_CONFIGS:
        dev->state = STATE_COUNT; // Done with command
        return exec_reset(dev->sock, g_cmd);
        break;
    case AMIIGO_CMD_FWUPDATE:
        dev->state = STATE_FWSTATUS;
        return exec_fwupdate(dev->sock);
        break;
    case AMIIGO_CMD_I2C_READ:
    case AMIIGO_CMD_I2C_WRITE:
        dev->state = STATE_I2C;
        return exec_debug_i2c(dev->sock);
        break;
    case AMIIGO_CMD_RENAME:
        dev->state = STATE_COUNT; // Done with command
        return exec_rename(dev->sock);
        break;
    case AMIIGO_CMD_TAG:
        dev->state = STATE_COUNT; // Done with command
        return exec_tag(dev->sock);
        break;
    default:
        return 0;
        break;
    }
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
            "       rename: rename the device\n"
            "       tag: set a tag\n"
            "  --input filename|param1=val1[,...]\n"
            "    Configuration file to use for given command.\n"
            "    Or sequence to specify parameters on command line.\n"
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
            g_opt.console = 1;
            break;

        case 'l':
            g_opt.live = 1;
            break;

        case 'v':
            g_opt.verbosity = 1;
            break;

        case 'a':
            g_opt.full = 1;
            break;

        case 'p':
            g_opt.leave_compressed = 1;
            break;

        case 'r':
            g_opt.raw = 1;
            break;

        case 'i':
            if (parse_adapter(optarg))
                exit(1);
            break;

        case 'b':
            if (parse_device(optarg))
                exit(1);
            break;

        case 'x':
            if (parse_command(optarg))
                exit(1);
            break;

        case 'f':
            if (parse_input_file(optarg))
                exit(1);
            break;

        case 'u':
            if (set_update_file(optarg))
                exit(1);
            break;

        case 'd':
            if (parse_i2c_read(optarg))
                exit(1);
            break;

        case 'w':
            if (parse_i2c_write(optarg))
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
    int ret, i;
    time_t start_time[MAX_DEV_COUNT], stop_time[MAX_DEV_COUNT], download_time[MAX_DEV_COUNT];

    // Initialize the characteristics
    char_init();
    // Initialize the command configs
    cmd_init();

    // Amiigo devices to interact with
    amdev_t devices[2];
    // zero-fill the devices state
    memset(&devices[0], 0, sizeof(devices));

    // Set parameters based on command line
    do_command_line(argc, argv);

    for (i = 0; i < g_cfg.count_dst; ++i) {
        amdev_t * dev = &devices[i];
        // Connect to all devices
        dev->sock = gap_connect(g_src, g_cfg.dst[i]);
        if (dev->sock < 0) {
            return -1;
        }
        if (g_opt.full) {
            // Start by discovering Amiigo handles
            ret = discover_handles(dev->sock, OPT_START_HANDLE, OPT_END_HANDLE);
            if (ret) {
                fprintf(stderr, "discover_handles() error in %s\n", g_cfg.dst[i]);
                return -1;
            }
        } else {
            // Use default handles and discover the device
            ret = discover_device(dev);
            if (ret) {
                fprintf(stderr, "discover_device() error in %s\n", g_cfg.dst[i]);
                return -1;
            }
        }

        // Timing of downloads
        start_time[i]= time(NULL);
        stop_time[i] = start_time[i];
        download_time[i] = start_time[i];
    }

    int dev_idx = g_cfg.count_dst - 1;
    for (;;) {
        // See if user ended the run
        if (kbhit() == 'q')
            break;
        dev_idx--;
        if (dev_idx < 0)
            dev_idx = g_cfg.count_dst - 1;
        amdev_t * dev = &devices[dev_idx];

        // No need to keep-alive during firmware update
        if (dev->state != STATE_FWSTATUS_WAIT) {
            stop_time[dev_idx] = time(NULL);
            double diff = difftime(stop_time[dev_idx], start_time[dev_idx]);
            // Keep-alive by reading status every 60s
            if (diff > 60) {
                if (g_opt.verbosity)
                    printf(" (Keep alive %s)\n", g_cfg.dst[dev_idx]);
                start_time[dev_idx] = stop_time[dev_idx];
                // Read the status to keep conection alive
                exec_status(dev->sock);
            }
            if (dev->state == STATE_DOWNLOAD && !g_opt.live) {
                diff = difftime(stop_time[dev_idx], download_time[dev_idx]);
                // Download timeout reached
                if (diff > 2)
                {
                    printf(" (Timeout %s)\n", g_cfg.dst[dev_idx]);
                    break;
                }
            }
        }

        uint8_t buf[1024] = {0};
        int len = gap_recv(dev->sock, &buf[0], sizeof(buf));
        if (len < 0)
            break;
        if (len == 0)
            continue;

        // Last time apacket came
        download_time[dev_idx] = stop_time[dev_idx];

        // Process incoming data
        ret = process_data(dev, buf, len);
        if (ret) {
            fprintf(stderr, "main process_data() error in %s\n", g_cfg.dst[dev_idx]);
            break;
        }
        if (dev->state == STATE_COUNT)
            break; // Done the the command

        // If all devices have their status read, execute the requested command
        if (dev->status.battery_level > 0) {
            // Now that we have status (e.g. number of logs) of all devices
            //  Start execution of the requested command
            ret = exec_command(dev);
        }

        if (dev->state == STATE_COUNT)
            break; // Done the the command

    } //end for(;;

    for (i = 0; i < g_cfg.count_dst; ++i) {
        amdev_t * dev = &devices[i];
        // Reset CPU if need to exit in the middle of firmware update
        if (dev->state == STATE_FWSTATUS_WAIT)
            exec_reset(dev->sock, AMIIGO_CMD_RESET_CPU);


        // Close the socket
        gap_shutdown(dev->sock);

        // Close log files
        if (dev->logFile != NULL)
        {
            fclose(dev->logFile);
            dev->logFile = NULL;
        }
    }
    printf("\n");

    return 0;
}
