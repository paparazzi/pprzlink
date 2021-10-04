# Hey Emacs, this is a -*- makefile -*-
#
# Copyright (C) 2003-2014 The Paparazzi Team
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

OCAMLBUILD=ocamlbuild -use-ocamlfind

#
# NOTE: due to a bug in older ocamlbuild versions the build-dir can NOT be absolute
# see http://caml.inria.fr/mantis/view.php?id=5503
# using a recent ocaml(build) version without THAT particular bug,
# but dir MUST NOT contain ..
# so use some magic here to get a normalized absolute path if PPRZLINK_DIR is not set
PPRZLINK_DIR ?= $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
OCAML_LIBDIR ?= $(PPRZLINK_DIR)/build/ocaml
BUILDDIR ?= $(PPRZLINK_DIR)/build/ocaml/gen

# FIXME test if pprzlink is installed in the system or try local build
export OCAMLPATH=$(OCAML_LIBDIR)

INSTALL_FLAGS ?=
DESTDIR ?= $(PPRZLINK_DIR)/bin
ifneq ($(DESTDIR),)
INSTALL_FLAGS += -destdir $(DESTDIR)
endif

INSTALL_FILES = $(shell find $(BUILDDIR) -name '*.byte') $(shell find $(BUILDDIR) -name '*.native')

GEN_ML := $(shell ls *.ml)
GEN_BYTE = $(GEN_ML:.ml=.byte)
GEN_NATIVE = $(GEN_ML:.ml=.native)
GENS = $(basename $(GEN_ML))

all: byte

byte:
	@echo "Build generators (bytecode)"
	$(Q)$(OCAMLBUILD) -build-dir $(BUILDDIR) -no-links -lflags "-dllpath-pkg pprzlink" $(GEN_BYTE)

native:
	@echo "Build generators (native)"
	$(Q)$(OCAMLBUILD) -build-dir $(BUILDDIR) -no-links $(GEN_NATIVE)

install: byte
	@echo "Installing generators in $(DESTDIR)"
	$(Q)mkdir -p $(DESTDIR)
	$(Q)cp $(INSTALL_FILES) $(DESTDIR)

clean:
	$(Q)rm -f *.cm* *.out *~ .depend *.pyc
	$(Q)$(OCAMLBUILD) -clean -classic-display -build-dir $(BUILDDIR)

.PHONY: all byte native clean

