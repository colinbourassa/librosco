#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <libgen.h>
#include "memsinjection.h"

enum command_idx
{
    MC_Read = 0,
    MC_Read_Raw = 1,
    MC_Read_IAC = 2,
    MC_PTC = 3,
    MC_FuelPump = 4,
    MC_IAC_Close = 5,
    MC_IAC_Open = 6,
    MC_AC = 7,
    MC_Coil = 8,
    MC_Injectors = 9,
    MC_Interactive = 10,
    MC_Num_Commands = 11
};

static const char* commands[] = { "read",
                                  "read-raw",
                                  "read-iac",
                                  "ptc",
                                  "fuelpump",
                                  "iac-close",
                                  "iac-open",
                                  "ac",
                                  "coil",
                                  "injectors",
                                  "interactive"
                                };

void printbuf(uint8_t* buf, unsigned int count)
{
    unsigned int idx = 0;
    while (idx < count)
    {
        idx += 1;
        printf("%02X ", buf[idx-1]);
        if (idx % 16 == 0)
        {
            printf("\n");
        }
    }
    printf("\n");
}

bool interactive_mode(mems_info* info, uint8_t* response_buffer)
{
    size_t icmd_size = 8;
    char* icmd_buf_ptr;
    uint8_t icmd;
    ssize_t bytes_read = 0;
    ssize_t total_bytes_read = 0;
    bool quit = false;

    if ((icmd_buf_ptr = (char*)malloc(icmd_size)) != NULL)
    {
        printf("Enter a command (in hex) or 'quit'.\n> ");
        while (!quit && (getline(&icmd_buf_ptr, &icmd_size, stdin) != -1))
        {
            if (strncmp(icmd_buf_ptr, "quit", 4) == 0)
            {
                quit = true;
            }
            else if (icmd_buf_ptr[0] != '\n' && icmd_buf_ptr[1] != '\r')
            {
                icmd = strtoul(icmd_buf_ptr, NULL, 16);
                if ((icmd >= 0) && (icmd <= 0xff))
                {
                    if (write(info->sd, &icmd, 1) == 1)
                    {
                        bytes_read = 0;
                        total_bytes_read = 0;
                        do {
                            bytes_read = read(info->sd, response_buffer + bytes_read, 1);
                            total_bytes_read += bytes_read;
                        } while (bytes_read > 0);

                        if (total_bytes_read > 0)
                        {
                            printbuf(response_buffer, total_bytes_read);
                        }
                        else
                        {
                            printf("No response from ECU.\n");
                        }
                    }
                    else
                    {
                        printf("Error: failed to write command byte to serial port.\n");
                    }
                }
                else
                {
                    printf("Error: command must be between 0x00 and 0xFF.\n");
                }
                printf("> ");
            }
            else
            {
                printf("> ");
            }
        }

        free(icmd_buf_ptr);
    }
    else
    {
        printf("Error allocating command buffer memory.\n");
    }
}

int main(int argc, char** argv)
{
    bool success = false;
    int cmd_idx = 0;
    mems_data data;
    mems_data_frame frame;
    libmemsinjection_version ver;
    mems_info info;
    uint8_t readval = 0;
    uint8_t iac_limit_count = 80;   // number of times to re-send an IAC move command when
                                    // the ECU is already reporting that the valve has
                                    // reached its requested position
    int read_loop_count = 1;
    bool read_inf = false;
    uint8_t response_buffer[16384]; // this is twice as large as the micro's on-chip ROM,
                                    // so it's probably sufficient
    ver = mems_get_lib_version();

    if (argc < 3)
    {
        printf("readmems using libmemsinjection v%d.%d.%d\n", ver.major, ver.minor, ver.patch);
        printf("Usage: %s <serial device> <command> [read-loop-count]\n", basename(argv[0]));
        printf(" where <command> is one of the following:\n");
        for (cmd_idx = 0; cmd_idx < MC_Num_Commands; ++cmd_idx)
        {
            printf("\t%s\n", commands[cmd_idx]);
        }
        printf(" and [read-loop-count] is either a number or 'inf' to read forever.\n");

        return 0;
    }

    while ((cmd_idx < MC_Num_Commands) && (strcasecmp(argv[2], commands[cmd_idx]) != 0))
    {
        cmd_idx += 1;
    }

    if (cmd_idx >= MC_Num_Commands)
    {
        printf("Invalid command: %s\n", argv[2]);
        return -1;
    }

    if (argc >= 4)
    {
       if (strcmp(argv[3], "inf") == 0)
       {
          read_inf = true;
       }
       else
       {
          read_loop_count = strtoul(argv[3], NULL, 0);
       }
    }

    if (cmd_idx != MC_Interactive)
    {
      printf("Running command: %s\n", commands[cmd_idx]);
    }

    mems_init(&info);

    if (mems_connect(&info, argv[1]))
    {
        if (mems_init_link(&info, response_buffer))
        {
            printf("ECU responded to D0 command with: %02X %02X %02X %02X\n\n",
                   response_buffer[0], response_buffer[1],
                   response_buffer[2], response_buffer[3]);

            switch (cmd_idx)
            {
            case MC_Read:
                while (read_inf || (read_loop_count-- > 0))
                {
                    if (mems_read(&info, &data))
                    {
                        printf("RPM: %u\nCoolant (deg F): %u\nAmbient (deg F): %u\nIntake air (deg F): %u\n"
                               "Fuel temp (deg F): %u\nMAP (kPa): %f\nMain voltage: %f\nThrottle pot voltage: %f\n"
                               "Idle switch: %u\nPark/neutral switch: %u\nFault codes: %u\nIAC position: %u\n"
                               "-------------\n",
                                data.engine_rpm, data.coolant_temp_f, data.ambient_temp_f,
                                data.intake_air_temp_f, data.fuel_temp_f, data.map_kpa, data.battery_voltage,
                                data.throttle_pot_voltage, data.idle_switch, data.park_neutral_switch,
                                data.fault_codes, data.iac_position);
                        success = true;
                    }
                }
                break;

            case MC_Read_Raw:
                while (read_inf || (read_loop_count-- > 0))
                {
                    if (mems_read_raw(&info, &frame))
                    {
                        printf("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                               frame.A, frame.engine_rpm_hi, frame.engine_rpm_lo, frame.coolant_temp,
                               frame.ambient_temp, frame.intake_air_temp, frame.fuel_temp, frame.map_kpa,
                               frame.battery_voltage, frame.throttle_pot, frame.idle_switch, frame.B,
                               frame.park_neutral_switch, frame.dtc0, frame.dtc1, frame.C, frame.D, frame.E,
                               frame.iac_position, frame.F_hi, frame.F_lo, frame.G, frame.H, frame.I_hi, frame.I_lo,
                               frame.J, frame.K, frame.L);
                        success = true;
                    }
                }
                break;

            case MC_Read_IAC:
                if (mems_read_iac_position(&info, &readval))
                {
                    printf("0x%02X\n", readval);
                    success = true;
                }
                break;

            case MC_PTC:
                if (mems_test_actuator(&info, MEMS_PTCRelayOn, NULL))
                {
                    sleep(2);
                    success = mems_test_actuator(&info, MEMS_PTCRelayOff, NULL);
                }
                break;

            case MC_FuelPump:
                if (mems_test_actuator(&info, MEMS_FuelPumpOn, NULL))
                {
                    sleep(2);
                    success = mems_test_actuator(&info, MEMS_FuelPumpOff, NULL);
                }
                break;

            case MC_IAC_Close:
                do
                {
                    success = mems_test_actuator(&info, MEMS_CloseIAC, &readval);

                    // For some reason, diagnostic tools will continue to send send the
                    // 'close' command many times after the IAC has already reached the
                    // fully-closed position. Emulate that behavior here.
                    if (success && (readval == 0x00))
                    {
                        iac_limit_count -= 1;
                    }
                } while(success && iac_limit_count);
                break;

            case MC_IAC_Open:
                // The SP Rover 1 pod considers a value of 0xB4 to represent an opened
                // IAC valve, so repeat the open command until the valve is opened to
                // that point.
                do
                {
                    success = mems_test_actuator(&info, MEMS_OpenIAC, &readval);
                } while(success && (readval < 0xB4));
                break;

            case MC_AC:
                if (mems_test_actuator(&info, MEMS_ACRelayOn, NULL))
                {
                    sleep(2);
                    success = mems_test_actuator(&info, MEMS_ACRelayOff, NULL);
                }
                break;

            case MC_Coil:
                success = mems_test_actuator(&info, MEMS_FireCoil, NULL);
                break;

            case MC_Injectors:
                success = mems_test_actuator(&info, MEMS_TestInjectors, NULL);
                break;

            case MC_Interactive:
                success = interactive_mode(&info, response_buffer);
                break;

            default:
                printf("Error: invalid command\n");
                break;
            }
        }
        else
        {
            printf("Error in initialization sequence.\n");
        }
        mems_disconnect(&info);
    }
    else
    {
        printf("Error: could not open serial device (%s).\n", argv[1]);
    }

    mems_cleanup(&info);

    return success ? 0 : -2;
}

