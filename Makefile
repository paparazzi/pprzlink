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
MESSAGES_DTD ?= message_definitions/v1.0/messages.dtd
UNITS_XML ?= message_definitions/common/units.xml
MESSAGES_INSTALL ?= var
MESSAGES_INCLUDE ?= $(MESSAGES_INSTALL)/include/pprzlink
MESSAGES_LIB ?= $(MESSAGES_INSTALL)/share/pprzlink/src
TELEMETRY_H ?= $(MESSAGES_INCLUDE)/messages.h
DATALINK_H ?= $(MESSAGES_INCLUDE)/dl_protocol.h
INTERMCU_H ?= $(MESSAGES_INCLUDE)/intermcu_msg.h

# validate xml for pygen by default
# to skip validation define VALIDATE_XML to 0 or FALSE
VALIDATE_XML ?= TRUE
ifneq (,$(findstring $(VALIDATE_XML),0 FALSE))
  VALIDATE_FLAG = --no-validate
else
  VALIDATE_FLAG =
endif

all: libs

libs: ocaml_lib_v1

generators: ocaml_lib_v1
	$(Q)$(MAKE) -C tools/generator install

ocaml_lib_v1:
	$(Q)$(MAKE) -C lib/v1.0/ocaml

libpprzlink-install:
	$(Q)$(MAKE) -C lib/v1.0/ocaml install

pre_messages_dir:
	$(Q)test -d $(MESSAGES_INCLUDE) || mkdir -p $(MESSAGES_INCLUDE)
	$(Q)test -d $(MESSAGES_LIB) || mkdir -p $(MESSAGES_LIB)

gen_messages: generators pre_messages_dir
	@echo 'Generate C messages (OCaml) at location $(MESSAGES_INCLUDE)'
	$(Q)./bin/gen_messages_c.byte $(MESSAGES_XML) telemetry $(TELEMETRY_H)
	$(Q)./bin/gen_messages_c.byte $(MESSAGES_XML) datalink $(DATALINK_H)
	$(Q)./bin/gen_messages_c.byte $(MESSAGES_XML) intermcu $(INTERMCU_H)
	$(Q)cp tools/generator/C/include_v1.0/*.h $(MESSAGES_INCLUDE)

post_messages_install:
	@echo 'Copy extra lib files'
	$(Q)cp $(MESSAGES_XML) $(MESSAGES_DTD) $(UNITS_XML) $(MESSAGES_INSTALL)
	$(Q)cp lib/v1.0/C/*.h $(MESSAGES_INCLUDE)
	$(Q)cp lib/v1.0/C/*.c $(MESSAGES_LIB)

pygen_messages: pre_messages_dir
	@echo 'Generate C messages (Python) at location $(MESSAGES_INCLUDE)'
	$(Q)./tools/generator/gen_messages.py $(VALIDATE_FLAG) --lang C -o $(TELEMETRY_H) $(MESSAGES_XML) telemetry
	$(Q)./tools/generator/gen_messages.py $(VALIDATE_FLAG) --lang C -o $(DATALINK_H) $(MESSAGES_XML) datalink
	$(Q)./tools/generator/gen_messages.py $(VALIDATE_FLAG) --lang C -o $(INTERMCU_H) $(MESSAGES_XML) intermcu

messages: gen_messages post_messages_install

pymessages: pygen_messages post_messages_install

clean :
	$(Q)$(MAKE) -C tools/generator clean
	$(Q)$(MAKE) -C lib/v1.0/ocaml clean

uninstall: clean
	$(Q)rm -rf var bin build $(MESSAGES_INCLUDE) $(MESSAGES_LIB)

validate_messages:
	$(Q)./tools/generator/gen_messages.py --only-validate $(MESSAGES_XML) telemetry
	$(Q)./tools/generator/gen_messages.py --only-validate $(MESSAGES_XML) datalink
	$(Q)./tools/generator/gen_messages.py --only-validate $(MESSAGES_XML) intermcu

.PHONY: all libs generators ocaml_lib_v1 pre_messages_dir post_messages_install gen_messages pygen_messages messages pymessages clean uninstall validate_messages
