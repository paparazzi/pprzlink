# Hey Emacs, this is a -*- makefile -*-
#
# Copyright (C) 2016 Felix Ruess <felix.ruess@gmail.com>
#
# This file is part of paparazzi.
#
# paparazzi is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# paparazzi is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with paparazzi; see the file COPYING.  If not, see
# <http://www.gnu.org/licenses/>.
#

# Quiet compilation
Q=@

CC = gcc
CXX = g++
LD = $(CC)

OPT ?= 3
# Slightly bigger .elf files but gains the ability to decode macros
DEBUG_FLAGS ?= -ggdb3
CSTANDARD ?= -std=gnu99

CFLAGS += -O$(OPT) -fPIC
CFLAGS += $(CSTANDARD)
CFLAGS += -fno-short-enums
CFLAGS += -Wall -Wcast-qual -Wimplicit -Wcast-align
CFLAGS += -Wpointer-arith -Wswitch
CFLAGS += -Wredundant-decls -Wreturn-type -Wshadow -Wunused
CFLAGS += -Wstrict-prototypes -Wmissing-declarations
CFLAGS += -Wmissing-prototypes -Wnested-externs
CFLAGS += -ffunction-sections -fdata-sections

CXXFLAGS += $(CFLAGS)

# directory where pprzlink generated files are installed
PPRZLINK_DIR ?= ../../var

PPRZLINK_FLAGS += -I$(PPRZLINK_DIR)/include

CFLAGS += $(PPRZLINK_FLAGS) -DDOWNLINK
CXXFLAGS += $(PPRZLINK_FLAGS) -DDOWNLINK
LDFLAGS += -fPIC

all: linux_uart_example

linux_uart_example: linux_uart_example.o linux_uart_pprz_transport.o pprz_transport.o
	@echo LD $@
	$(Q)$(LD) -o $@ $^ $(LDFLAGS)

pprz_transport.o: $(PPRZLINK_DIR)/share/pprzlink/src/pprz_transport.c
	@echo CC $@
	$(Q)$(CC) -c $(CFLAGS) $< -o $@

# Compile: create object files from C source files
%.o : %.c
	@echo CC $@
	$(Q)test -d $(dir $@) || mkdir -p $(dir $@)
	$(Q)$(CC) -c $(CFLAGS) $< -o $@

# Compile: create object files from C++ source files
%.o : %.cpp
	@echo CXX $@
	$(Q)test -d $(dir $@) || mkdir -p $(dir $@)
	$(Q)$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	$(Q)rm -rf $(OBJDIR) *.o linux_uart_example

.PHONY: all clean
