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

#define DEBUG_P 1

#ifdef DEBUG_P
  #define dprintf_err printf
#else
  #define dprintf_err
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum mems_commands
{
    MEMS_FuelPumpOn     = 0x11,
    MEMS_FuelPumpOff    = 0x01,

    MEMS_PTCRelayOn     = 0x12,
    MEMS_PTCRelayOff    = 0x02,

    MEMS_ACRelayOn      = 0x13,
    MEMS_ACRelayOff     = 0x03,

    MEMS_ReqData        = 0x80,
    MEMS_Heartbeat      = 0xF4,

    MEMS_TestInjectors  = 0xF7,
    MEMS_FireCoil       = 0xF8,
    MEMS_GetIACPosition = 0xFB,
    MEMS_OpenIAC        = 0xFD,
    MEMS_CloseIAC       = 0xFE,

    MEMS_ClearFaults    = 0xCC
};

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
    uint8_t B;
    uint8_t map_kpa;
    uint8_t battery_voltage;
    uint8_t throttle_pot;
    uint8_t idle_switch;
    uint8_t C;
    uint8_t park_neutral_switch;
    uint8_t dtc0;
    uint8_t dtc1;
    uint8_t D;
    uint8_t E;
    uint8_t F;
    uint8_t iac_position;
    uint8_t G_hi;
    uint8_t G_lo;
    uint8_t H;
    uint8_t I;
    uint8_t J_hi;
    uint8_t J_lo;
    uint8_t K;
    uint8_t L;
    uint8_t M;
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
    float map_psi;
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

uint16_t swapShort(const uint16_t source);

void mems_init(mems_info* info);
bool mems_init_link(mems_info* info, uint8_t* d0_response_buffer);
void mems_cleanup(mems_info* info);
bool mems_connect(mems_info* info, const char *devPath);
void mems_disconnect(mems_info* info);
bool mems_is_connected(mems_info* info);
bool mems_read_raw(mems_info* info, mems_data_frame* frame);
bool mems_read(mems_info* info, mems_data* data);
bool mems_startup_sequence(mems_info* info);
bool mems_fuel_pump_control(mems_info* info, bool pump_on);
bool mems_ptc_relay_control(mems_info* info, bool relay_on);
bool mems_ac_relay_control(mems_info* info, bool relay_on);
bool mems_test_injectors(mems_info* info);
bool mems_test_coil(mems_info* info);
bool mems_move_idle_bypass_motor(mems_info* info, bool close, uint8_t *position);
bool mems_clear_faults(mems_info* info);
bool mems_heartbeat(mems_info* info);

libmemsinjection_version mems_get_lib_version();

/* Closing brace for 'extern "C"' */
#ifdef __cplusplus
}
#endif

#endif // MEMS_H

