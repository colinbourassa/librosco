#ifndef LIBMEMS_INTERNAL_H
#define LIBMEMS_INTERNAL_H

#include <stdbool.h>

bool mems_openserial(mems_info *info, const char *devPath);
bool mems_send_command(mems_info *info, uint8_t cmd);
int16_t mems_read_serial(mems_info* info, uint8_t *buffer, uint16_t quantity);
int16_t mems_write_serial(mems_info* info, uint8_t *buffer, uint16_t quantity);
bool mems_lock(mems_info* info);
void mems_unlock(mems_info* info);
uint8_t temperature_value_to_degrees_f(uint8_t val);

#endif // LIBMEMS_INTERNAL_H

