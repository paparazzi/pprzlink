# Hey Emacs, this is a -*- makefile -*-
#
# Copyright (C) 2017 Fabien Garcia <fabien.garcia@enac.fr>
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

MAKE = make

all:

install:
	$(Q)cp ../../common/C/*.h $(MESSAGES_INCLUDE)
	$(Q)cp ./*.h $(MESSAGES_INCLUDE)
	$(Q)cp ./*.c $(MESSAGES_LIB)
	$(Q)test -d $(MESSAGES_LIB)/hacl-c || mkdir -p $(MESSAGES_LIB)/hacl-c
	$(Q)test -d $(MESSAGES_INCLUDE)/hacl-c || mkdir -p $(MESSAGES_INCLUDE)/hacl-c
	$(Q)cp ../../hacl-c/*.c $(MESSAGES_LIB)/hacl-c
	$(Q)cp ../../hacl-c/*.h $(MESSAGES_INCLUDE)/hacl-c

.PHONY: install
