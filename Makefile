# Hey Emacs, this is a -*- makefile -*-
#
# Copyright (C) 2015 Gautier Hattenberger <gautier.hattenberger@enac.fr>
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

MAKE = make

MESSAGES_XML ?= message_definitions/v1.0/messages.xml
UNITS_XML ?= message_definitions/common/units.xml
MESSAGES_INSTALL ?= var
MESSAGES_INCLUDE ?= $(MESSAGES_INSTALL)/include
MESSAGES_LIB ?= $(MESSAGES_INSTALL)/lib
TELEMETRY_H ?= $(MESSAGES_INCLUDE)/messages.h
DATALINK_H ?= $(MESSAGES_INCLUDE)/dl_protocol.h

all: libs

libs: generators ocaml_lib_v1

generators: ocaml_lib_v1
	$(Q)$(MAKE) -C tools/generator install

ocaml_lib_v1:
	$(Q)$(MAKE) -C lib/v1.0/ocaml

messages: generators
	@echo 'Generate C messages at default location'
	$(Q)test -d $(MESSAGES_INCLUDE) || mkdir -p $(MESSAGES_INCLUDE)
	$(Q)test -d $(MESSAGES_LIB) || mkdir -p $(MESSAGES_LIB)
	$(Q)./bin/gen_messages_c.byte $(MESSAGES_XML) telemetry $(TELEMETRY_H)
	$(Q)./bin/gen_messages_c.byte $(MESSAGES_XML) datalink $(DATALINK_H)
	$(Q)cp $(MESSAGES_XML) $(UNITS_XML) $(MESSAGES_INSTALL)
	$(Q)cp tools/generator/C/include_v1.0/*.h $(MESSAGES_INCLUDE)
	$(Q)cp lib/v1.0/C/*.h $(MESSAGES_INCLUDE)
	$(Q)cp lib/v1.0/C/*.c $(MESSAGES_LIB)

pymessages:
	@echo 'Generate C messages from Python generator'
	$(Q)test -d $(MESSAGES_INCLUDE) || mkdir -p $(MESSAGES_INCLUDE)
	$(Q)test -d $(MESSAGES_LIB) || mkdir -p $(MESSAGES_LIB)
	$(Q)./tools/generator/gen_messages.py --lang C -o $(TELEMETRY_H) $(MESSAGES_XML) telemetry
	$(Q)./tools/generator/gen_messages.py --lang C -o $(DATALINK_H) $(MESSAGES_XML) datalink
	$(Q)cp $(MESSAGES_XML) $(UNITS_XML) $(MESSAGES_INSTALL)
	$(Q)cp tools/generator/C/include_v1.0/*.h $(MESSAGES_INCLUDE)
	$(Q)cp lib/v1.0/C/*.h $(MESSAGES_INCLUDE)
	$(Q)cp lib/v1.0/C/*.c $(MESSAGES_LIB)


clean :
	$(Q)$(MAKE) -C tools/generator clean
	$(Q)$(MAKE) -C lib/v1.0/ocaml clean

uninstall: clean
	$(Q)rm -rf var bin build

.PHONY: all libs generators ocaml_lib_v1 messages clean uninstall
