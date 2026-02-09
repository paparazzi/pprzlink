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

MY_PYTHON := $(shell echo `which python3`)
MY_CYTHON := $(shell $(MY_PYTHON) -c 'import Cython' 2>&1)

all:

build:
ifneq ($(findstring ModuleNotFoundError, $(MY_CYTHON)),ModuleNotFoundError)
	@echo "cython intalled, compiling pprzlink at location $(DESTDIR)/../build"
	$(Q)python setup.py build_ext --build-lib $(DESTDIR)/../build --build-temp $(DESTDIR)/../build
else
	@echo cython not installed, not compiling pprzlink
endif


install: build
ifneq ($(findstring ModuleNotFoundError, $(MY_CYTHON)),ModuleNotFoundError)
	@echo "INSTALL (cython lib) at location $(DESTDIR)"
	$(Q)test -d $(DESTDIR) || mkdir -p $(DESTDIR)
	$(Q)cp -r $(DESTDIR)/../build/pprzlink $(DESTDIR)
else
	@echo "INSTALL at location $(DESTDIR)"
	$(Q)test -d $(DESTDIR) || mkdir -p $(DESTDIR)
	$(Q)cp -r src/pprzlink $(DESTDIR)
endif

.PHONY: all install

