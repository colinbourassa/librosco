// librosco - a communications library for the Rover MEMS ECU
//
// protocol.c: This file contains routines specific to handling
//             the software protocol used by the ECU over its
//             serial link.

#if defined(WIN32) && defined(linux)
#error "Only one of 'WIN32' or 'linux' may be defined."
#endif

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#if defined(WIN32)
  #include <windows.h>
#elif defined(__NetBSD__)
  #include <string.h>
#endif

#include "rosco.h"
#include "rosco_internal.h"

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
 * Sends a single command byte to the ECU and waits for the same byte to be
 * echoed as a response. Note that if the ECU sends one or more bytes of
 * data in addition to the echoed command byte, mems_read_serial() must also
 * be called to retrieve that data from the input buffer.
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
  uint8_t command_c = MEMS_Heartbeat;
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
 * Sends a command to read a frame of data from the ECU, and returns the raw frame.
 */
bool mems_read_raw(mems_info* info, mems_data_frame_80* frame80, mems_data_frame_7d* frame7d)
{
    bool status = false;

    if (mems_lock(info))
    {
      if (mems_send_command(info, MEMS_ReqData80))
      {
        if (mems_read_serial(info, (uint8_t*)(frame80), sizeof(mems_data_frame_80)) == sizeof(mems_data_frame_80))
        {
          status = true;
        }
        else
        {
          dprintf_err("mems_read_raw(): failed to read data frame in response to cmd 0x80\n");
        }
      }
      else
      {
        dprintf_err("mems_read_raw(): failed to send read command 0x80\n");
      }

      if (status)
      {
        if (mems_send_command(info, MEMS_ReqData7D))
        {
          if (!mems_read_serial(info, (uint8_t*)(frame7d), sizeof(mems_data_frame_7d)) == sizeof(mems_data_frame_7d))
          {
            dprintf_err("mems_read_raw(): failed to read data frame in response to cmd 0x7D\n");
            status = false;
          }
        }
        else
        {
          dprintf_err("mems_read_raw(): failed to send read command 0x7D\n");
          status = false;
        }
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
  mems_data_frame_80 dframe80;
  mems_data_frame_7d dframe7d;

  if (mems_read_raw(info, &dframe80, &dframe7d))
  {
    memset(data, 0, sizeof(mems_data));

    data->engine_rpm           = ((uint16_t)dframe80.engine_rpm_hi << 8) | dframe80.engine_rpm_lo;
    data->coolant_temp_c       = dframe80.coolant_temp;
    data->ambient_temp_c       = dframe80.ambient_temp;
    data->intake_air_temp_c    = dframe80.intake_air_temp;
    data->fuel_temp_c          = dframe80.fuel_temp;
    data->map_kpa              = dframe80.map_kpa;
    data->battery_voltage      = dframe80.battery_voltage / 10.0;
    data->throttle_pot_voltage = dframe80.throttle_pot * 0.02;
    data->idle_switch          = (dframe80.idle_switch == 0) ? 0 : 1;
    data->park_neutral_switch  = (dframe80.park_neutral_switch == 0) ? 0 : 1;
    data->fault_codes          = 0;
    data->iac_position         = dframe80.iac_position;
    data->coil_time            = (((uint16_t)dframe80.coil_time_hi << 8) | dframe80.coil_time_lo) * 0.002;
    data->idle_error           = ((uint16_t)dframe80.idle_error_hi << 8) | dframe80.idle_error_lo;
    data->ignition_advance     = (dframe80.ignition_advance * 0.5) - 24.0;
    data->lambda_voltage_mv    = dframe7d.lambda_voltage * 5;
    data->fuel_trim            = dframe7d.fuel_trim;
    data->closed_loop          = dframe7d.closed_loop;
    data->idle_base_pos        = dframe7d.idle_base_pos;

    if (dframe80.dtc0 & 0x01)   // coolant temp sensor fault
      data->fault_codes |= (1 << 0);

    if (dframe80.dtc0 & 0x02)   // intake air temp sensor fault
      data->fault_codes |= (1 << 1);

    if (dframe80.dtc1 & 0x02)   // fuel pump circuit fault
      data->fault_codes |= (1 << 2);

    if (dframe80.dtc1 & 0x80)   // throttle pot circuit fault
      data->fault_codes |= (1 << 3);

    success = true;
  }

  return success;
}

/**
 * Reads the current idle air control motor position.
 */
bool mems_read_iac_position(mems_info* info, uint8_t* position)
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
 * Repeatedly send command to open or close the idle air control valve until
 * it is in the desired position. The valve does not necessarily move one full
 * step per serial command, depending on the rate at which the commands are
 * issued.
 */
bool mems_move_iac(mems_info* info, uint8_t desired_pos)
{
  bool status = false;
  uint16_t attempts = 0;
  uint8_t current_pos = 0;
  actuator_cmd cmd;

  // read the current IAC position, and only take action
  // if we're not already at the desired point
  if (mems_read_iac_position(info, &current_pos))
  {
    if ((desired_pos < current_pos) ||
        ((desired_pos > current_pos) && (current_pos < IAC_MAXIMUM)))
    {
      cmd = (desired_pos > current_pos) ? MEMS_OpenIAC : MEMS_CloseIAC;

      do {
        status = mems_test_actuator(info, cmd, &current_pos);
        attempts += 1;
      } while (status && (current_pos != desired_pos) && (attempts < 300));
    }
  }

  status = (desired_pos == current_pos);

  return status;
}

/**
 * Sends a command to run an actuator test, and returns the single byte of data.
 */
bool mems_test_actuator(mems_info* info, actuator_cmd cmd, uint8_t* data)
{
  bool status = false;
  uint8_t response = 0x00;

  if (mems_lock(info))
  {
    if (mems_send_command(info, cmd) &&
        (mems_read_serial(info, &response, 1) == 1))
    {
      if (data)
      {
        *data = response;
      }
      status = true;
    }
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

