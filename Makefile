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
Q ?=@

MAKE = make

PPRZLINK_MSG_VERSION ?= 1.0
PPRZLINK_LIB_VERSION ?= 2.0

# by default make a local install in current directory
# PREFIX should be an absolute path
PREFIX ?= $(shell pwd)

MESSAGES_XML ?= message_definitions/v$(PPRZLINK_MSG_VERSION)/messages.xml
MESSAGES_DTD ?= message_definitions/v$(PPRZLINK_MSG_VERSION)/messages.dtd
UNITS_XML ?= message_definitions/common/units.xml
MESSAGES_INSTALL ?= $(PREFIX)/var
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

all: libpprzlink

# This compiles the sources then copy the library
libpprzlink++:
	$(Q)Q=$(Q) DESTDIR=$(DESTDIR)/C++ $(MAKE) -C lib/v$(PPRZLINK_LIB_VERSION)/C++ libpprzlink++

libpprzlink++-install: libpprzlink++ 
	$(Q)Q=$(Q) DESTDIR=$(DESTDIR)/C++ $(MAKE) -C lib/v$(PPRZLINK_LIB_VERSION)/C++ install
	$(Q)Q=$(Q) DESTDIR=$(DESTDIR)/C++ $(MAKE) -C lib/v$(PPRZLINK_LIB_VERSION)/C++ copy-ivyqt


libpprzlink:
	$(Q)Q=$(Q) $(MAKE) -C lib/v$(PPRZLINK_LIB_VERSION)/ocaml

libpprzlink-install:
	$(Q)Q=$(Q) DESTDIR=$(DESTDIR)/ocaml $(MAKE) -C lib/v$(PPRZLINK_LIB_VERSION)/ocaml install
	$(Q)Q=$(Q) DESTDIR=$(DESTDIR)/python $(MAKE) -C lib/v$(PPRZLINK_LIB_VERSION)/python install

pre_messages_dir:
	$(Q)test -d $(MESSAGES_INCLUDE) || mkdir -p $(MESSAGES_INCLUDE)
	$(Q)test -d $(MESSAGES_LIB) || mkdir -p $(MESSAGES_LIB)

post_messages_install:
	@echo 'Copy extra lib files'
	$(Q)cp $(MESSAGES_XML) $(MESSAGES_DTD) $(UNITS_XML) $(MESSAGES_INSTALL)
	$(Q)Q=$(Q) MESSAGES_INCLUDE=$(MESSAGES_INCLUDE) MESSAGES_LIB=$(MESSAGES_LIB) $(MAKE) -C lib/v$(PPRZLINK_LIB_VERSION)/C install

pygen_messages: pre_messages_dir
	@echo 'Generate C messages (Python) at location $(MESSAGES_INCLUDE)'
	$(Q)./tools/generator/gen_messages.py $(VALIDATE_FLAG) --protocol $(PPRZLINK_LIB_VERSION) --messages $(PPRZLINK_MSG_VERSION) --lang C -o $(TELEMETRY_H) $(MESSAGES_XML) telemetry
	$(Q)./tools/generator/gen_messages.py $(VALIDATE_FLAG) --protocol $(PPRZLINK_LIB_VERSION) --messages $(PPRZLINK_MSG_VERSION) --lang C -o $(DATALINK_H) $(MESSAGES_XML) datalink
	$(Q)./tools/generator/gen_messages.py $(VALIDATE_FLAG) --protocol $(PPRZLINK_LIB_VERSION) --messages $(PPRZLINK_MSG_VERSION) --lang C -o $(INTERMCU_H) $(MESSAGES_XML) intermcu

pymessages: pygen_messages post_messages_install

clean :
	$(Q)$(MAKE) -C tools/generator clean
	$(Q)$(MAKE) -C lib/v$(PPRZLINK_LIB_VERSION)/ocaml clean

uninstall: clean
	$(Q)rm -rf var bin build $(MESSAGES_INCLUDE) $(MESSAGES_LIB)

validate_messages:
	$(Q)./tools/generator/gen_messages.py --only-validate $(MESSAGES_XML) telemetry
	$(Q)./tools/generator/gen_messages.py --only-validate $(MESSAGES_XML) datalink
	$(Q)./tools/generator/gen_messages.py --only-validate $(MESSAGES_XML) intermcu

.PHONY: libpprzlink++-install libpprzlink++ libpprzlink libpprzlink-install pre_messages_dir post_messages_install pygen_messages pymessages clean uninstall validate_messages
