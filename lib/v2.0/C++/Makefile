# Hey Emacs, this is a -*- makefile -*-
#
# Copyright (C) 2019 Fabien Garcia <fabien.garcia@enac.fr>
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

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(dir $(mkfile_path))

SRC_DIR=$(DESTDIR)/share/src
INCLUDE_DIR=$(DESTDIR)/include
LIB_DIR=$(DESTDIR)/lib
OBJ_DIR=$(PWD)/build/C++

PPRZLINK_HEADERS=$(wildcard pprzlink/*.h)
PPRZLINK_SRC=$(wildcard pprzlink/*.cpp)

PPRZLINK_OBJS=$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(PPRZLINK_SRC))

IVYC++_HEADERS=ivy-c++/IvyApplication.h ivy-c++/Ivy.h ivy-c++/IvyCallback.h
IVYC++_SRCS=ivy-c++/IvyApplication.cpp ivy-c++/Ivy.cpp

IVYC++_OBJS=$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(IVYC++_SRCS))

INCLUDE_FLAG=-I$(current_dir)
CXXFLAGS= --std=c++17 -Wall -fPIC -flto $(INCLUDE_FLAG)

.PHONY: install copy-ivyqt prepare-build

$(OBJ_DIR)/%.o: %.cpp
	$(Q)echo Compiling $<
	$(Q)$(CXX) -c $(CXXFLAGS) $< -o $@

prepare-build:
	$(Q)test -d $(OBJ_DIR)/pprzlink || mkdir -p $(OBJ_DIR)/pprzlink
	$(Q)test -d $(OBJ_DIR)/ivy-c++ || mkdir -p $(OBJ_DIR)/ivy-c++

libpprzlink++: prepare-build $(IVYC++_OBJS) $(PPRZLINK_OBJS) $(IVYC++_HEADERS) $(PPRZLINK_HEADERS)
	@echo Building $@
	$(Q)$(CXX) -shared -o $(OBJ_DIR)/libpprzlink++.so $(IVYC++_OBJS) $(PPRZLINK_OBJS)
	$(Q)ar rcs $(OBJ_DIR)/libpprzlink++.a $(IVYC++_OBJS) $(PPRZLINK_OBJS)

install: 
	$(Q)test -d $(INCLUDE_DIR)/pprzlink || mkdir -p $(INCLUDE_DIR)/pprzlink
	$(Q)test -d $(INCLUDE_DIR)/ivy-c++ || mkdir -p $(INCLUDE_DIR)/ivy-c++
	$(Q)cp $(PPRZLINK_HEADERS) $(INCLUDE_DIR)/pprzlink
	$(Q)cp -R pprzlink/exceptions $(INCLUDE_DIR)/pprzlink
	$(Q)cp $(IVYC++_HEADERS) $(INCLUDE_DIR)/ivy-c++
	$(Q)test -d $(LIB_DIR) || mkdir -p $(LIB_DIR)
	$(Q)cp $(OBJ_DIR)/libpprzlink++.* $(LIB_DIR)

copy-ivyqt: 
	$(Q)test -d $(SRC_DIR) || mkdir -p $(SRC_DIR)
	$(Q)cp ivy-c++/ivyqtloop.h $(SRC_DIR)
	$(Q)cp ivy-c++/ivyqtloop.cpp $(SRC_DIR)
