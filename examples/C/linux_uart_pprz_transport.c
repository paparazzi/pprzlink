/*
 * Copyright (C) 2016 Felix Ruess <felix.ruess@gmail.com>
 *
 * This file is part of Paparazzi.
 *
 * Paparazzi is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * Paparazzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with paparazzi; see the file COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

/**
 * @file examples/C/linux_uart_pprz_transport.c
 *
 * Example on how to use pprzlink with pprz_transport on Linux over a UART like device.
 */

#include "linux_uart_pprz_transport.h"


struct pprz_transport pprz_tp;
struct link_device link_dev;


void put_byte(void *p, long fd __attribute__((unused)), uint8_t data);
bool check_free_space(void *p, long *fd __attribute__((unused)), uint16_t len);
void put_buffer(void *p, long fd, const uint8_t *data, uint16_t len);
void send_message(void *p __attribute__((unused)), long fd __attribute__((unused)));
uint8_t get_byte(void *p);
uint16_t char_available(void *p);


/// at least implement this function
void put_byte(void *p, long fd __attribute__((unused)), uint8_t data)
{
  // add your implementation here to write one byte to the uart
}

/// check if there is enough space to send len bytes in your uart
bool check_free_space(void *p, long *fd __attribute__((unused)), uint16_t len)
{
  return true;
}

/// call put_buffer byte by byte, if you have an interface that can write multiple bytes to your uart, use that here instead
void put_buffer(void *p, long fd, const uint8_t *data, uint16_t len)
{
  int i = 0;
  for (i = 0; i < len; i++) {
    put_byte(p, fd, data[i]);
  }
}

/// called after each message, here you could e.g. flush the buffer, but usually not needed
void send_message(void *p __attribute__((unused)), long fd __attribute__((unused)))
{
}

/// read one char from the uart, not needed for sending only
uint8_t get_byte(void *p)
{
  return 0;
}

/// return number of chars that are available for reading
uint16_t char_available(void *p)
{
  return 0;
}


void pprz_link_init(void)
{
  // initialize the PPRZ transport
  pprz_transport_init(&pprz_tp);

  // setup the link device
  link_dev.periph = NULL; // if you want to pass a pointer to the other functions, set it here
  link_dev.check_free_space = (check_free_space_t) check_free_space;
  link_dev.put_byte = (put_byte_t) put_byte;
  link_dev.put_buffer = (put_buffer_t) put_buffer;
  link_dev.send_message = (send_message_t) send_message;
  link_dev.char_available = (char_available_t) char_available;
  link_dev.get_byte = (get_byte_t) get_byte;
}
