// libmemsinjection - a communications library for the Rover MEMS ECU
//
// protocol.c: This file contains routines specific to handling
//             the software protocol used by the ECU over its
//             serial link.

#if defined(WIN32) && defined(linux)
#error "Only one of 'WIN32' or 'linux' may be defined."
#endif

#include <unistd.h>
#include <stdio.h>

#if defined(WIN32)
  #include <windows.h>
#elif defined(__NetBSD__)
  #include <string.h>
#endif

#include "memsinjection.h"
#include "mems_internal.h"

/**
 * Reads bytes from the serial device using an OS-specific call.
 * @param buffer Buffer into which data should be read
 * @param quantity Number of bytes to read
 * @return Number of bytes read from the device, or -1 if no bytes could be read
 */
int16_t mems_read_serial(mems_info* info, uint8_t* buffer, uint16_t quantity)
{
    int16_t totalBytesRead = 0;
    int16_t bytesRead = -1;
    uint8_t *buffer_pt = buffer;
    int x = 0;

    if (mems_is_connected(info))
    {
        do
        {
#if defined(WIN32)
            DWORD w32BytesRead = 0;
            if ((ReadFile(info->sd, (UCHAR *) buffer_pt, quantity, &w32BytesRead, NULL) == TRUE) &&
                    (w32BytesRead > 0))
            {
                bytesRead = w32BytesRead;
            }
#else
            bytesRead = read(info->sd, buffer_pt, quantity);
#endif
            totalBytesRead += bytesRead;
            buffer_pt += bytesRead;

        } while ((bytesRead > 0) && (totalBytesRead < quantity));
    }

    if (totalBytesRead < quantity)
    {
        dprintf_err("mems_read_serial(): expected %d, got %d\n", quantity, totalBytesRead);
    }

    return totalBytesRead;
}

/**
 * Writes bytes to the serial device using an OS-specific call
 * @param buffer Buffer from which written data should be drawn
 * @param quantity Number of bytes to write
 * @return Number of bytes written to the device, or -1 if no bytes could be written
 */
int16_t mems_write_serial(mems_info* info, uint8_t* buffer, uint16_t quantity)
{
    int16_t bytesWritten = -1;
    int x = 0;

    if (mems_is_connected(info))
    {
#if defined(WIN32)
        DWORD w32BytesWritten = 0;
        if ((WriteFile(info->sd, (UCHAR *) buffer, quantity, &w32BytesWritten, NULL) == TRUE) &&
            (w32BytesWritten == quantity))
        {
            bytesWritten = w32BytesWritten;
        }
#else
        bytesWritten = write(info->sd, buffer, quantity);
#endif
    }

    return bytesWritten;
}

/**
 * Sends a single command byte to the ECU and waits for the same byte to be echoed as a response.
 */
bool mems_send_command(mems_info *info, uint8_t cmd)
{
    bool result = false;
    uint8_t response = 0xFF;

    if (mems_write_serial(info, &cmd, 1) == 1)
    {
        if (mems_read_serial(info, &response, 1) == 1)
        {
            if (response == cmd)
            {
                result = true;
            }
            else
            {
                dprintf_err("mems_send_command(): received one nonmatching byte (%02X) in response to command %02X\n", response, cmd);
            }
        }
        else
        {
            dprintf_err("mems_send_command(): did not receive echo of command %02X\n", cmd);
        }
    }
    else
    {
        dprintf_err("mems_send_command(): failed to send command %02X\n", cmd);
    }

    return result;
}

/**
 * Sends an initialization/startup sequence to the ECU. Required to enable further communication.
 */
bool mems_init_link(mems_info* info, uint8_t* d0_response_buffer)
{
    uint8_t command_a = 0xCA;
    uint8_t command_b = 0x75;
    uint8_t command_c = MEMS_Heartbeat; // TODO: this command may not be needed
    uint8_t command_d = 0xD0;
    uint8_t buffer = 0x00;

    if (!mems_send_command(info, command_a))
    {
        dprintf_err("mems_init_link(): Did not see %02X command echo\n", command_a);
        return false;
    }
    if (!mems_send_command(info, command_b))
    {
        dprintf_err("mems_init_link(): Did not see %02X command echo\n", command_b);
        return false;
    }
    if (!mems_send_command(info, command_c))
    {
        dprintf_err("mems_init_link(): Did not see %02X command echo\n", command_c);
        return false;
    }
    if (mems_read_serial(info, &buffer, 1) != 1)
    {
        dprintf_err("mems_init_link(): Did not see null terminator for %02X command\n", command_c);
        return false;
    }
    if (!mems_send_command(info, command_d))
    {
        dprintf_err("mems_init_link(): Did not see %02X command echo\n", command_d);
        return false;
    }

    // Expect four more bytes after the echo of the D0 command byte.
    // Response is 99 00 03 03 for Mini SPi.
    if (mems_read_serial(info, d0_response_buffer, 4) != 4)
    {
        dprintf_err("mems_init_link(): Received fewer bytes than expected after echo of %02X command", command_d);
        return false;
    }

    return true;
}

/**
 * Locks the mutex used for threadsafe access
 */
bool mems_lock(mems_info* info)
{
#if defined(WIN32)
    if (WaitForSingleObject(info->mutex, INFINITE) != WAIT_OBJECT_0)
        return false;
#else
    pthread_mutex_lock(&info->mutex);
#endif
    return true;
}

/**
 * Releases the mutex used for threadsafe access
 */
void mems_unlock(mems_info* info)
{
#if defined(WIN32)
    ReleaseMutex(info->mutex);
#else
    pthread_mutex_unlock(&info->mutex);
#endif
}

/**
 * Converts the temperature value used by the ECU into degrees Fahrenheit.
 */
uint8_t temperature_value_to_degrees_f(uint8_t val)
{
    uint8_t degrees_c = val - 55;
    return (uint8_t)((float)degrees_c * 1.8 + 32);
}

/**
 * Converts kilopascals into pounds per square inch.
 */
float kpa_to_psi(uint8_t kpa)
{
    return ((float)kpa / 6.89475729);
}

/**
 * Sends a command to read a frame of data from the ECU, and returns the raw frame.
 */
bool mems_read_raw(mems_info* info, mems_data_frame* frame)
{
    bool status = false;

    if (mems_lock(info))
    {
        if (mems_send_command(info, MEMS_ReqData))
        {
            if (mems_read_serial(info, (uint8_t*)(frame), sizeof(mems_data_frame)) == sizeof(mems_data_frame))
            {
                status = true;
            }
            else
            {
                dprintf_err("mems_read(): failed to read data frame\n");
            }
        }
        else
        {
            dprintf_err("mems_read(): failed to send read command\n");
        }
        mems_unlock(info);
    }

    return status;
}

/**
 * Sends an command to read a frame of data from the ECU, and parses the returned frame.
 */
bool mems_read(mems_info* info, mems_data* data)
{
    bool success = false;
    mems_data_frame dframe;

    if (mems_lock(info))
    {
        if (mems_send_command(info, MEMS_ReqData))
        {
            if (mems_read_serial(info, (uint8_t*)(&dframe), sizeof(mems_data_frame)) == sizeof(mems_data_frame))
            {
                data->engine_rpm           = ((uint16_t)dframe.engine_rpm_hi << 8) | dframe.engine_rpm_lo;
                data->coolant_temp_f       = temperature_value_to_degrees_f(dframe.coolant_temp);
                data->ambient_temp_f       = temperature_value_to_degrees_f(dframe.ambient_temp);
                data->intake_air_temp_f    = temperature_value_to_degrees_f(dframe.intake_air_temp);
                data->map_psi              = kpa_to_psi(dframe.map_kpa);
                data->battery_voltage      = dframe.battery_voltage / 10.0;
                data->throttle_pot_voltage = dframe.throttle_pot * 0.02;
                data->idle_switch          = (dframe.idle_switch == 0) ? 0 : 1;
                data->park_neutral_switch  = (dframe.park_neutral_switch == 0) ? 0 : 1;
                data->fault_codes          = 0;
                data->iac_position         = dframe.iac_position;

                if (dframe.dtc0 & 0x01)   // coolant temp sensor fault
                    data->fault_codes     |= (1 << 0);

                if (dframe.dtc0 & 0x02)   // intake air temp sensor fault
                    data->fault_codes     |= (1 << 1);

                if (dframe.dtc1 & 0x02)   // fuel pump circuit fault
                    data->fault_codes     |= (1 << 2);

                if (dframe.dtc1 & 0x80)   // throttle pot circuit fault
                    data->fault_codes     |= (1 << 3);

                success = true;
            }
            else
            {
                dprintf_err("mems_read(): failed to read data frame\n");
            }
        }
        else
        {
            dprintf_err("mems_read(): failed to send read command\n");
        }

        mems_unlock(info);
    }

    return success;
}

/**
 * Sends a command to open or close the fuel pump relay.
 */
bool mems_fuel_pump_control(mems_info* info, bool pump_on)
{
    uint8_t cmd = pump_on ? MEMS_FuelPumpOn : MEMS_FuelPumpOff;
    bool status = false;
    uint8_t response = 0xFF;

    if (mems_lock(info))
    {
        status = mems_send_command(info, cmd) &&
                 (mems_read_serial(info, &response, 1) == 1);
        mems_unlock(info);
    }
    return status;
}

/**
 * Sends a command to open or close the manifold heater relay (also called the
 * PTC relay, meaning "positive temperature coefficient").
 */
bool mems_ptc_relay_control(mems_info* info, bool relay_on)
{
    uint8_t cmd = relay_on ? MEMS_PTCRelayOn : MEMS_PTCRelayOff;
    bool status = false;
    uint8_t response = 0xFF;

    if (mems_lock(info))
    {
        status = mems_send_command(info, cmd) &&
                 (mems_read_serial(info, &response, 1) == 1);
        mems_unlock(info);
    }
    return status; 
}

/**
 * Sends a command to open or close the air conditioning relay.
 */
bool mems_ac_relay_control(mems_info* info, bool relay_on)
{
    uint8_t cmd = relay_on ? MEMS_ACRelayOn : MEMS_ACRelayOff;
    bool status = false;
    uint8_t response = 0xFF;

    if (mems_lock(info))
    {
        status = mems_send_command(info, cmd) &&
                 (mems_read_serial(info, &response, 1) == 1);
        mems_unlock(info);
    }
    return status;
}

/**
 * Sends a command to cycle the fuel injector(s).
 */
bool mems_test_injectors(mems_info* info)
{
    bool status = false;
    uint8_t response = 0xFF;

    if (mems_lock(info))
    {
        // the additional byte (after the command byte echo) was observed to be 0x03
        status = mems_send_command(info, MEMS_TestInjectors) &&
                 (mems_read_serial(info, &response, 1) == 1);
        mems_unlock(info);
    }
    return status;
}

/**
 * Sends a command to fire the ignition coil.
 */
bool mems_test_coil(mems_info* info)
{
    bool status = false;
    uint8_t response = 0xFF;

    if (mems_lock(info))
    {
        status = mems_send_command(info, MEMS_FireCoil) &&
                 (mems_read_serial(info, &response, 1) == 1);
        mems_unlock(info);
    }
    return status;
}

/**
 * Reads the current idle air control motor position.
 */
bool mems_read_iac_position(mems_info* info, uint8_t *position)
{
    bool status = false;

    if (mems_lock(info))
    {
        status = mems_send_command(info, MEMS_GetIACPosition) &&
                 (mems_read_serial(info, position, 1) == 1);
        mems_unlock(info);
    }
    return status;
}

/**
 * Sends a command to open or close the idle air control motor by one step.
 */
bool mems_move_idle_bypass_motor(mems_info* info, bool close, uint8_t *position)
{
    uint8_t cmd = close ? MEMS_CloseIAC : MEMS_OpenIAC;
    bool status = false;

    if (mems_lock(info))
    {
        status = mems_send_command(info, cmd) &&
                 (mems_read_serial(info, position, 1) == 1);
        mems_unlock(info);
    }
    return status;
}

/**
 * Sends a command to clear any stored fault codes
 */
bool mems_clear_faults(mems_info* info)
{
    bool status = false;
    uint8_t response = 0xFF;

    if (mems_lock(info))
    {
        // send the command and check for one additional byte after the
        // echoed command byte (should be 0x00)
        status = mems_send_command(info, (uint8_t)MEMS_ClearFaults) &&
                 (mems_read_serial(info, &response, 1) == 1);
        {
            status = true;
        }
        mems_unlock(info);
    }

    return status;
}

/**
 * Sends a simple heartbeat (ping) command to check connectivity
 */
bool mems_heartbeat(mems_info* info)
{
    bool status = false;
    uint8_t response = 0xFF;

    if (mems_lock(info))
    {
        // send the command and check for one additional byte after the
        // echoed command byte (should be 0x00)
        if (mems_send_command(info, (uint8_t)MEMS_Heartbeat) &&
            (mems_read_serial(info, &response, 1) == 1))
        {
            status = true;
        }
        mems_unlock(info);
    }

    return status;
}

