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
#
OCAML_VERSION := $(shell ocamlc -version)
OCAML_MAJOR := $(shell echo $(OCAML_VERSION) | cut -f1 -d.)
ifeq ($(shell test $(OCAML_MAJOR) -lt 4; echo $$?),0)
# need to use an old buggy ocaml(build) version: dir MUST be relative
OCAML_LIBDIR ?= ../../build/ocaml
else
# using a recent ocaml(build) version without THAT particular bug,
# but dir MUST NOT contain ..
# so use some magic here to get a normalized absolute path if PPRZLINK_DIR is not set
PPRZLINK_DIR ?= $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
OCAML_LIBDIR ?= $(PPRZLINK_DIR)/build/ocaml
endif

export OCAMLPATH=$(OCAML_LIBDIR)

BUILDDIR ?= _build
INSTALL_FLAGS ?=
DESTDIR ?=
ifneq ($(DESTDIR),)
INSTALL_FLAGS += -destdir $(DESTDIR)
endif

INSTALL_FILES = $(shell ls $(BUILDDIR)/*.so $(BUILDDIR)/*.a $(BUILDDIR)/*.mli $(BUILDDIR)/*.cm*)

GEN_ML := $(shell ls *.ml)
GEN_BYTE = $(GEN_ML:.ml=.byte)
GEN_NATIVE = $(GEN_ML:.ml=.native)
GENS = $(basename $(GEN_ML))

all: byte

#
# build without creating links, then make symlinks ourselves so they still have .out ending
#
byte:
	@echo ${OCAMLPATH}
	@echo "Build generators (bytecode)"
	$(Q)$(OCAMLBUILD) -no-links -lflags "-dllpath-pkg pprzlink" $(GEN_BYTE)

native:
	@echo "Build generators (native)"
	$(Q)$(OCAMLBUILD) -no-links $(GEN_NATIVE)

clean:
	$(Q)rm -f *.cm* *.out *~ .depend
	$(Q)$(OCAMLBUILD) -clean

.PHONY: all byte native clean

