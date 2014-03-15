/*
 * Old Amiigo protocol and definitions for backward compatibility
 *
 * @date March 14, 2014
 * @author: dashesy
 */

#ifndef AMOLDPROTO_H
#define AMOLDPROTO_H

#include "amidefs.h"
#include "amproto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint8 unused0;
	uint8 unused1;

	// Samples will be taken at the above rate for 'duration' seconds
	// every 'interval' seconds. Sampling is disabled if interval is 0.
	uint16 fast_interval;
	uint16 slow_interval;
	uint16 sleep_interval;
	uint8 duration;

	uint8 unused2;
	uint8 unused3;
	uint8 unused4;
	uint8 unused5;

	uint8 debug;     // Console output debug level, set to 0 for normal operation
	uint8 unused6;
	uint8 unused7;
	uint8 movement;  // average movement level at wich starting a reading is allowed
	uint8 flags;     // Contol Bits
} PACKED WEDConfigLS_1816;

// Top-level struct for characteristic UUID AMI_UUID(WED_UUID_CONFIG)
typedef struct {
    // WED_CFG_TYPE identifying following struct type
    uint8 config_type;

    // Structure corresponding to config_type
    union {
        WEDConfigGeneral general;
        WEDConfigAccel accel;
        WEDConfigLS_1816 lightsensor;
        WEDConfigTemp temp;
        WEDConfigMaint maint;
        WEDConfigLog log;
        WEDConfigConn conn;
        WEDConfigName name;
    };
} PACKED WEDConfig_1816;

int exec_configls_18116(int sock);

#ifdef __cplusplus
}
#endif

#endif // include guard
