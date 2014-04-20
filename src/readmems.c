#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <libgen.h>
#include "mems.h"

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
    MC_Num_Commands = 10
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
                                  "injectors"
                                };

int main(int argc, char** argv)
{
    bool success = false;
    int cmd_idx = 0;
    mems_data data;
    mems_data_frame frame;
    libmemsinjection_version ver;
    mems_info info;
    uint8_t readval = 0;
    uint8_t iac_limit_count = 80;
    int read_loop_count = 1;
    bool read_inf = false;

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

    printf("Running command: %s\n", commands[cmd_idx]);

    mems_init(&info);

    if (mems_connect(&info, argv[1]))
    {
        if (mems_startup(&info))
        {
            switch (cmd_idx)
            {
            case MC_Read:
                while (read_inf || (read_loop_count-- > 0))
                {
                    if (mems_read(&info, &data))
                    {
                        printf("RPM: %u\nCoolant (deg F): %u\nAmbient (deg F): %u\nIntake air (deg F): %u\n"
                               "MAP (psi): %u\nMain voltage: %f\nThrottle pot voltage: %f\nIdle switch: %u\n"
                               "Park/neutral switch: %u\nFault codes: %u\nIAC position: %u\n-------------\n",
                                data.engine_rpm, data.coolant_temp_f, data.ambient_temp_f,
                                data.intake_air_temp_f, data.map_psi, data.battery_voltage,
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
                               frame.ambient_temp, frame.intake_air_temp, frame.B, frame.map_kpa,
                               frame.battery_voltage, frame.throttle_pot, frame.idle_switch, frame.C,
                               frame.park_neutral_switch, frame.dtc0, frame.dtc1, frame.D, frame.E, frame.F,
                               frame.iac_position, frame.G_hi, frame.G_lo, frame.H, frame.I, frame.J_hi, frame.J_lo,
                               frame.K, frame.L, frame.M);
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
                if (mems_ptc_relay_control(&info, true))
                {
                    sleep(2);
                    success = mems_ptc_relay_control(&info, false);
                }
                break;

            case MC_FuelPump:
                if (mems_fuel_pump_control(&info, true))
                {
                    sleep(2);
                    success = mems_fuel_pump_control(&info, false);
                }
                break;

            case MC_IAC_Close:
                do
                {
                    success = mems_move_idle_bypass_motor(&info, true, &readval);

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
                    success = mems_move_idle_bypass_motor(&info, false, &readval);
                } while(success && (readval < 0xB4));
                break;

            case MC_AC:
                if (mems_ac_relay_control(&info, true))
                {
                    sleep(2);
                    success = mems_ac_relay_control(&info, false);
                }
                break;

            case MC_Coil:
                success = mems_test_coil(&info);
                break;

            case MC_Injectors:
                success = mems_test_injectors(&info);
                break;

            default:
                printf("Error: invalid command\n");
                break;
            }
        }
        else
        {
            printf("Error sending startup command.\n");
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

