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
 * @file examples/C/linux_uart_pprz_transport.h
 *
 * Example on how to use pprzlink with pprz_transport on Linux over a UART like device.
 */

#ifndef LINUX_UART_PPRZ_TRANSPORT_H
#define LINUX_UART_PPRZ_TRANSPORT_H

#include "pprzlink/pprz_transport.h"

extern struct pprz_transport pprz_tp;
extern struct link_device link_dev;

extern void pprz_link_init(void);

#endif
