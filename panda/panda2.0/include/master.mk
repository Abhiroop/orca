# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# This makefile should be included by the master makefile that creates
# the Panda library. That makefile should define:
#
# PANDA		The directory of the Panda distribution
# MODULES	All modules that should be included in the library. The
#		modules are specified as subdirectories of $(PANDA)/src.
#		The system module should be mentioned first.
# C*		compiler flags as CC, CPPFLAGS, and CFLAGS
# A*		ar command and flags AR, ARFLAGS
#
# Finally, the makefile should include this makefile with:
# include $(PANDA)/include/master.mk

SHELL		:= /bin/sh

# Library directory = current directory
PANDA_LIB	:= $(shell pwd)

export PANDA PANDA_LIB CC CPPFLAGS CFLAGS AS ASFLAGS AR ARFLAGS RANLIB

# Object file and directory definitions 
object_dirs	:= $(addprefix $(PANDA_LIB)/objects/, $(MODULES))

.PHONY: all
all:		$(MODULES)

# Create a copy of the tree of all specified modules
$(object_dirs):	
		-mkdir -p $(object_dirs)
		-chmod -R g+wx $(PANDA_LIB)/objects

# Create all modules in the corresponding object directory with the makefile
# in the source directory.
.PHONY: $(MODULES)
$(MODULES):	$(object_dirs)
		@echo CFLAGS: $(CFLAGS)
		$(MAKE) -C $(PANDA_LIB)/objects/$@ \
		        -f $(PANDA)/src/$@/Makefile

.PHONY: system
system:		$(word 1, $(MODULES))


# Create a tags file for all source files in the modules
src_dirs        := $(addprefix $(PANDA)/src/, $(MODULES)) $(PANDA)/include
find_sources    = $(wildcard $(dir)/*.c) $(wildcard $(dir)/*.h)
src_files         = $(foreach dir, $(src_dirs), $(find_sources))
 
TAGS:		$(src_files)
		etags $(src_files)

# Cleanup all objects, the Panda library, and the tags file
clean:
		-rm -fr libpanda.a objects TAGS

