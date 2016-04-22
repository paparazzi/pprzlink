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
 * @file examples/C/linux_uart_example.c
 *
 * Example on how to use pprzlink with pprz_transport on Linux over a UART like device.
 */

#include "linux_uart_pprz_transport.h"

// for telemetry messages
#include "pprzlink/messages.h"
// for datalink messages
#include "pprzlink/dl_protocol.h"

#include <unistd.h>

struct pprz_transport pprz_tp;
struct link_device link_dev;

#define SENDER_AC_ID 0

void do_stuff(void);
void do_stuff(void)
{
  // send a datalink message
  uint8_t ac_id = 42;
  uint8_t settings_index = 42;
  float foo_value = 123.45;
  pprz_msg_send_SETTING(&pprz_tp.trans_tx, &link_dev, SENDER_AC_ID, &settings_index, &ac_id, &foo_value);
}


int main(void)
{
  pprz_link_init();

  while (1) {
    usleep(1000);
    do_stuff();
  }
}
