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

// https://github.com/dmage/co2mon

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "co2mon.h"

#define CODE_TAMB 0x42 /* Ambient Temperature */
#define CODE_CNTR 0x50 /* Relative Concentration of CO2 */

#define VALUE_MAX 20

int print_unknown = 0;
int decode_data = 1;

struct co2mon_state {
  uint16_t data[256];
  uint8_t seen[32];
  time_t heatbeat;
  unsigned int deverr;
};

int co2mon_init(int decode) {
  int r = hid_init();
  if (r < 0) fprintf(stderr, "hid_init: error\n");
  decode_data = decode;
  return r;
}

void co2mon_exit(void) { if (hid_exit() < 0) fprintf(stderr, "hid_exit: error\n"); }

hid_device *co2mon_open_device(void) {
  hid_device *dev = hid_open(VENDOR_ID, PRODUCT_ID, NULL);
  if (!dev) fprintf(stderr, "hid_open: error\n");
  return dev;
}

hid_device *co2mon_open_device_path(const char *path) {
  hid_device *dev = hid_open_path(path);
  if (!dev) fprintf(stderr, "hid_open_path: error\n");
  return dev;
}

void co2mon_close_device(hid_device *dev) { hid_close(dev); }

int co2mon_device_path(hid_device *dev, char *str, size_t maxlen) {
  str[0] = '\0';
  return 1;
}

int co2mon_send_magic_table(hid_device *dev, co2mon_data_t magic_table) {
  int r = hid_send_feature_report(dev, magic_table, sizeof(co2mon_data_t));
  if (r < 0 || r != sizeof(co2mon_data_t)) {
    fprintf(stderr, "hid_send_feature_report: error\n");
    return 0;
  }
  return 1;
}

static void swap_char(unsigned char *a, unsigned char *b) {
  unsigned char tmp = *a;
  *a = *b;
  *b = tmp;
}

static void decode_buf(co2mon_data_t result, co2mon_data_t buf, co2mon_data_t magic_table) {
  if (decode_data) {
    swap_char(&buf[0], &buf[2]);
    swap_char(&buf[1], &buf[4]);
    swap_char(&buf[3], &buf[7]);
    swap_char(&buf[5], &buf[6]);
    
    for (int i = 0; i < 8; ++i) buf[i] ^= magic_table[i];
    
    unsigned char tmp = (buf[7] << 5);
    result[7] = (buf[6] << 5) | (buf[7] >> 3);
    result[6] = (buf[5] << 5) | (buf[6] >> 3);
    result[5] = (buf[4] << 5) | (buf[5] >> 3);
    result[4] = (buf[3] << 5) | (buf[4] >> 3);
    result[3] = (buf[2] << 5) | (buf[3] >> 3);
    result[2] = (buf[1] << 5) | (buf[2] >> 3);
    result[1] = (buf[0] << 5) | (buf[1] >> 3);
    result[0] = tmp | (buf[0] >> 3);
    
    const unsigned char magic_word[8] = "Htemp99e";
    for (int i = 0; i < 8; ++i) result[i] -= (magic_word[i] << 4) | (magic_word[i] >> 4);
  } else memcpy(result, buf, 8);
}

int co2mon_read_data(hid_device *dev, co2mon_data_t magic_table, co2mon_data_t result) {
  co2mon_data_t data = { 0 };
  int actual_length = hid_read_timeout(dev, data, sizeof(co2mon_data_t), 5000 /* milliseconds */);
  if (actual_length < 0) {
    fprintf(stderr, "hid_read_timeout: error\n");
    return actual_length;
  }
  if (actual_length != sizeof(co2mon_data_t)) {
    fprintf(stderr, "hid_read_timeout: transferred %d bytes, expected %lu bytes\n", actual_length, (unsigned long)sizeof(co2mon_data_t));
    return 0;
  }
  
  decode_buf(result, data, magic_table);
  return actual_length;
}

static double decode_temperature(uint16_t w) { return (double)w * 0.0625 - 273.15; }

static int write_value(const char *name, const char *value) { return(1); }

static void write_heartbeat() {
  char buf[VALUE_MAX];
  const time_t now = time(0);
  snprintf(buf, VALUE_MAX, "%lld", (long long)now);
  write_value("heartbeat", buf);
}

void dev_loop_init(co2mon_device dev) {
  co2mon_data_t magic_table = { 0 };
  
  if (!co2mon_send_magic_table(dev, magic_table)) {
    fprintf(stderr, "Unable to send magic table to CO2 device\n");
    return;
  }
  
}

reading read_one(co2mon_device dev) {
  co2mon_data_t magic_table = { 0 };
  co2mon_data_t result;
  reading out = { -99, other };
  
  int r = co2mon_read_data(dev, magic_table, result);
  if (r <= 0) {
//    fprintf(stderr, "Error while reading data from device\n");
    out.message = DeviceReadError;
    return(out);
  }
  
  if (result[4] != 0x0d) {
//    fprintf(stderr, "Unexpected data from device (data[4] = %02hhx, want 0x0d)\n", result[4]);
    out.message = UnexpectedData;
    return(out);
  }
  
  unsigned char r0, r1, r2, r3, checksum;
  r0 = result[0];
  r1 = result[1];
  r2 = result[2];
  r3 = result[3];
  checksum = r0 + r1 + r2;
  if (checksum != r3) {
//    fprintf(stderr, "checksum error (%02hhx, await %02hhx)\n", checksum, r3);
    out.message = ChecksumError;
    return(out);
  }
  
//  char buf[VALUE_MAX];
  uint16_t w = (result[1] << 8) + result[2];
  
  switch (r0) {
    case CODE_TAMB:
//      snprintf(buf, VALUE_MAX, "%.4f", decode_temperature(w));
//      printf("Tamb\t%s\n", buf);
//      fflush(stdout);
      write_heartbeat();
      out.sensor = temperature;
      out.value = decode_temperature(w);
      out.message = OK;
      return(out);
      break;
    case CODE_CNTR:
      if ((unsigned)w > 3000) { // Avoid reading spurious (uninitialized?) data
        break;
      }
//      snprintf(buf, VALUE_MAX, "%d", (int)w);
//      printf("CntR\t%s\n", buf);
//      fflush(stdout);
      write_heartbeat();
      out.sensor = CO2;
      out.message = OK;
      out.value = (uint16_t)w;
      break;
    default:
      out.message = UnexpectedData;
      if (print_unknown) {
        printf("0x%02hhx\t%d\n", r0, (int)w);
        fflush(stdout);
      }
  }
  
  return(out);
}

void device_loop(co2mon_device dev) {
  
  co2mon_data_t magic_table = { 0 };
  co2mon_data_t result;
  
  if (!co2mon_send_magic_table(dev, magic_table)) {
    fprintf(stderr, "Unable to send magic table to CO2 device\n");
    return;
  }

  while (1) {

    fprintf(stderr, "Loop\n");
    int r = co2mon_read_data(dev, magic_table, result);
    if (r <= 0) {
      fprintf(stderr, "Error while reading data from device\n");
      break;
    }
    
    if (result[4] != 0x0d) {
      fprintf(stderr, "Unexpected data from device (data[4] = %02hhx, want 0x0d)\n", result[4]);
      continue;
    }
    
    unsigned char r0, r1, r2, r3, checksum;
    r0 = result[0];
    r1 = result[1];
    r2 = result[2];
    r3 = result[3];
    checksum = r0 + r1 + r2;
    if (checksum != r3) {
      fprintf(stderr, "checksum error (%02hhx, await %02hhx)\n", checksum, r3);
      continue;
    }
    
    char buf[VALUE_MAX];
    uint16_t w = (result[1] << 8) + result[2];
    
    switch (r0) {
      case CODE_TAMB:
        snprintf(buf, VALUE_MAX, "%.4f", decode_temperature(w));
        printf("Tamb\t%s\n", buf);
        fflush(stdout);
        write_heartbeat();
        break;
      case CODE_CNTR:
        if ((unsigned)w > 3000) { // Avoid reading spurious (uninitialized?) data
          break;
        }
        snprintf(buf, VALUE_MAX, "%d", (int)w);
        printf("CntR\t%s\n", buf);
        fflush(stdout);
        write_heartbeat();
        break;
      default:
        if (print_unknown) {
          printf("0x%02hhx\t%d\n", r0, (int)w);
          fflush(stdout);
        }
    }
    
  }
  
}
