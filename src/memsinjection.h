#ifndef MEMSINJECTION_H
#define MEMSINJECTION_H

/** \file mems.h
 * Header file defining the libmemsinjection functions, structs, and enums.
 */

#include <stdint.h>
#include <stdbool.h>

#if defined(WIN32)
  #include <windows.h>
#else
  #include <pthread.h>
  #include <errno.h>
#endif

#define DEBUG_P

#ifdef DEBUG_P
  #define dprintf_err printf
#else
  #define dprintf_err
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define IAC_MAXIMUM 0xB4

/**
 * These general commands are used to request data and clear fault codes.
 */
enum mems_data_command
{
    MEMS_ReqData        = 0x80,
    MEMS_ClearFaults    = 0xCC,
    MEMS_Heartbeat      = 0xF4,
    MEMS_GetIACPosition = 0xFB
};

/**
 * These commands are used to test actuators on the car.
 * Although some commands have on/off pairs (for controlling relays),
 * MEMS 1.6 (as fitted to the Mini SPi) will automatically shut off these
 * these actuators after a short period of time (< 1s). The "off" command,
 * if sent, will be acknowledged, but there is apparently no action taken.
 */
enum mems_actuator_command
{
    MEMS_FuelPumpOn     = 0x11,
    MEMS_FuelPumpOff    = 0x01,
    MEMS_PTCRelayOn     = 0x12,
    MEMS_PTCRelayOff    = 0x02,
    MEMS_ACRelayOn      = 0x13,
    MEMS_ACRelayOff     = 0x03,
#if 0
    /* I currently have no way to test these commands,
       so I'm excluding them from the build for now. */
    MEMS_PurgeValveOn   = 0x18,
    MEMS_PurgeValveOff  = 0x08,
    MEMS_O2HeaterOn     = 0x19,
    MEMS_O2HeaterOff    = 0x09,
    MEMS_BoostValveOn   = 0x1B,
    MEMS_BoostValveOff  = 0x0B,
    MEMS_Fan1On         = 0x1D,
    MEMS_Fan1Off        = 0x0D,
    MEMS_Fan2On         = 0x1E,
    MEMS_Fan2Off        = 0x0E
#endif
    MEMS_TestInjectors  = 0xF7,
    MEMS_FireCoil       = 0xF8,
    MEMS_OpenIAC        = 0xFD,
    MEMS_CloseIAC       = 0xFE
};

typedef enum mems_actuator_command actuator_cmd;

/**
 * Data sequence returned by the ECU in reply to the command 0x80.
 *
 * Note that the grouping of bytes 19-20 and 23-24 into 16-bit fields is due
 * only to the observed transmission line timing. This is probably accurate, as
 * bytes 1-2 are grouped similarly, and do indeed represent a single 16-bit
 * value.
 */
typedef struct
{
    uint8_t A;
    uint8_t engine_rpm_hi;
    uint8_t engine_rpm_lo;
    uint8_t coolant_temp;
    uint8_t ambient_temp;
    uint8_t intake_air_temp;
    uint8_t fuel_temp;
    uint8_t map_kpa;
    uint8_t battery_voltage;
    uint8_t throttle_pot;
    uint8_t idle_switch;
    uint8_t B;
    uint8_t park_neutral_switch;
    uint8_t dtc0;
    uint8_t dtc1;
    uint8_t C;
    uint8_t D;
    uint8_t E;
    uint8_t iac_position;
    uint8_t F_hi;
    uint8_t F_lo;
    uint8_t G;
    uint8_t H;
    uint8_t I_hi;
    uint8_t I_lo;
    uint8_t J;
    uint8_t K;
    uint8_t L;
} mems_data_frame;

/**
 * Compact structure containing only the relevant data from the ECU.
 */
typedef struct
{
    uint16_t engine_rpm;
    uint8_t coolant_temp_f;
    uint8_t ambient_temp_f;
    uint8_t intake_air_temp_f;
    uint8_t fuel_temp_f;
    float map_kpa;
    float battery_voltage;
    float throttle_pot_voltage;
    uint8_t idle_switch;
    uint8_t park_neutral_switch;
    /**
     * Bit 0: Coolant temp sensor CCT fault (1)
     * Bit 1: Inlet air temp sensor CCT fault (2)
     * Bit 2: Fuel pump circuit fault (10)
     * Bit 3: Throttle pot circuit fault (16)
     */
    uint8_t fault_codes;
    uint8_t iac_position;
} mems_data;

/**
 * Major/minor/patch version numbers for this build of the library
 */
typedef struct
{
  //! Major version number
  uint8_t major;
  //! Minor version number
  uint8_t minor;
  //! Patch version number
  uint8_t patch;
} libmemsinjection_version;

/**
 * Contains information about the state of the current connection to the ECU.
 */
typedef struct
{
#if defined(WIN32)
    //! Descriptor for the serial port device
    HANDLE sd;
    //! Lock to prevent multiple simultaneous open/close/read/write operations
    HANDLE mutex;
#else
    //! Descriptor for the serial port device
    int sd;
    //! Lock to prevent multiple simultaneous open/close/read/write operations
    pthread_mutex_t mutex;
#endif
} mems_info;

void mems_init(mems_info* info);
bool mems_init_link(mems_info* info, uint8_t* d0_response_buffer);
void mems_cleanup(mems_info* info);
bool mems_connect(mems_info* info, const char* devPath);
void mems_disconnect(mems_info* info);
bool mems_is_connected(mems_info* info);
bool mems_read_raw(mems_info* info, mems_data_frame* frame);
bool mems_read(mems_info* info, mems_data* data);
bool mems_read_iac_position(mems_info* info, uint8_t* position);
bool mems_move_iac(mems_info* info, uint8_t desired_pos);
bool mems_test_actuator(mems_info* info, actuator_cmd cmd, uint8_t* data);
bool mems_clear_faults(mems_info* info);
bool mems_heartbeat(mems_info* info);

libmemsinjection_version mems_get_lib_version();

/* Closing brace for 'extern "C"' */
#ifdef __cplusplus
}
#endif

#endif // MEMS_H

