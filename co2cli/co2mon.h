/*
 * co2mon - programming interface to CO2 sensor.
 * Copyright (C) 2015  Oleg Bulatov <oleg@bulatov.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CO2MON_H_INCLUDED_
#define CO2MON_H_INCLUDED_

#include "hidapi.h"

#define VENDOR_ID 0x04d9
#define PRODUCT_ID 0xa052

typedef hid_device *co2mon_device;
typedef unsigned char co2mon_data_t[8];

typedef enum {
  temperature = 10,
  CO2 = 20,
  other = 99
} sensor_type;

typedef enum {
  DeviceReadError = 10,
  UnexpectedData = 20,
  ChecksumError = 30,
  UnknownData = 40,
  OK = 50
} message;

typedef struct {
  double value;
  sensor_type sensor;
  message message;
} reading;

extern int co2mon_init(int);
extern void co2mon_exit(void);
extern co2mon_device co2mon_open_device(void);
extern co2mon_device co2mon_open_device_path(const char *path);
extern reading read_one(co2mon_device dev);
extern void dev_loop_init(co2mon_device dev);
extern void co2mon_close_device(co2mon_device dev);
extern int co2mon_device_path(co2mon_device dev, char *str, size_t maxlen);
extern int co2mon_send_magic_table(co2mon_device dev, co2mon_data_t magic_table);
extern int co2mon_read_data(co2mon_device dev, co2mon_data_t magic_table, co2mon_data_t result);

#endif
