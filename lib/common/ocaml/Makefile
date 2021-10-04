# Hey Emacs, this is a -*- makefile -*-
#
# Copyright (C) 2003 Pascal Brisset, Antoine Drouin
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

# redirect output if quiet compilation
ifeq ($(Q),@)
QUIET= -quiet
Q_OUT= >/dev/null 2>&1
else
Q_OUT=
endif


OCAMLBUILD=ocamlbuild -use-ocamlfind

#
# NOTE: due to a bug in older ocamlbuild versions the build-dir can NOT be absolute
# see http://caml.inria.fr/mantis/view.php?id=5503
# using a recent ocaml(build) version >= 4,
# but build-dir MUST NOT contain ..
# so use some magic here to get a normalized absolute path if PPRZLINK_DIR is not set
PPRZLINK_DIR ?= $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
BUILDDIR ?= $(PPRZLINK_DIR)/build/ocaml/pprzlink

INSTALL_FLAGS ?=
# use this option leave ocamlfind ldconf file unchanged
INSTALL_FLAGS += -ldconf ignore
DESTDIR ?=
ifneq ($(DESTDIR),)
INSTALL_FLAGS += -destdir $(DESTDIR)
endif

# enforce checking a clear type checking between bytes and string
C_OPTS = -cflags -safe-string
INSTALL_FILES = $(shell ls $(BUILDDIR)/*.so $(BUILDDIR)/*.a $(BUILDDIR)/*.mli $(BUILDDIR)/*.cm* $(BUILDDIR)/common/*.mli $(BUILDDIR)/common/*.cm*)


all: byte native

byte: _tags META
	@echo Build bytecode lib
	$(Q)test -d $(BUILDDIR) || mkdir -p $(BUILDDIR)
	$(Q)$(OCAMLBUILD) $(QUIET) $(C_OPTS) -build-dir $(BUILDDIR) -I common lib-pprzlink.cma
	$(Q)cp META $(BUILDDIR)

# byte with statically linked libs
static: _tags META
	@echo Build bytecode lib with static linking
	$(Q)test -d $(BUILDDIR) || mkdir -p $(BUILDDIR)
	$(Q)$(OCAMLBUILD) $(QUIET) $(C_OPTS) -build-dir $(BUILDDIR) -tag static -I common lib-pprzlink.cma
	$(Q)cp META $(BUILDDIR)

native: _tags META
	@echo Build native lib
	$(Q)test -d $(BUILDDIR) || mkdir -p $(BUILDDIR)
	$(Q)$(OCAMLBUILD) $(QUIET) $(C_OPTS) -build-dir $(BUILDDIR) -I common lib-pprzlink.cmxa
	$(Q)cp META $(BUILDDIR)

install: clean_lib byte native META
	@echo INSTALL at location $(DESTDIR)
	$(Q)test -d $(DESTDIR) || mkdir -p $(DESTDIR)
	$(Q)ocamlfind remove $(INSTALL_FLAGS) pprzlink $(Q_OUT)
	$(Q)ocamlfind install $(INSTALL_FLAGS) pprzlink META $(INSTALL_FILES) $(Q_OUT)

UNAME = $(shell uname -s)
ifeq ("$(UNAME)","Darwin")
	MKTEMP = gmktemp
else
	MKTEMP = mktemp
endif

clean :
	$(Q)rm -f *.cm* *.out *.opt .depend *.a *.o *.so
	$(Q)ocamlbuild -clean -classic-display -build-dir $(BUILDDIR)

# test if an other version of the lib is installed and remove if needed
clean_lib:
	@echo Check for already installed pprzlink ocaml lib
ifneq ("$(wildcard $(DESTDIR)/pprzlink/META)","")
	@echo ' -> found lib version $(shell grep version $(DESTDIR)/pprzlink/META | cut -d \" -f2) while installing $(PPRZLINK_LIB_VERSION)'
ifneq ($(shell grep version $(DESTDIR)/pprzlink/META | cut -d \" -f2), $(PPRZLINK_LIB_VERSION))
	@echo ' -> removing files'
	$(Q)ocamlbuild -clean -classic-display -build-dir $(BUILDDIR)
	$(Q)ocamlfind remove $(INSTALL_FLAGS) pprzlink $(Q_OUT)
endif
endif

.PHONY: all byte static native install install_local clean clean_lib
