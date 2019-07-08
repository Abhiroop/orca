# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# This makefile should be included by the master makefile that creates
# the Panda library. That makefile should define:
#
# RTS		The directory of the RTS distribution
# PANDA		The directory of the Panda distribution
# MODULES	All modules that should be included in the library. The
#		modules are specified as subdirectories of $(RTS).
# C*		compiler flags as CC, CPPFLAGS, and CFLAGS
# A*		ar command and flags AR, ARFLAGS
# DEP		dependency generator
#
# Finally, the makefile should include this makefile with:
# include $(RTS)/include/make/master.mk

SHELL		:= /bin/sh

# Library directory = current directory
RTS_LIB		:= $(shell pwd)

export RTS RTS_LIB PANDA CC CPPFLAGS CFLAGS AS ASFLAGS AR ARFLAGS RANLIB
export DEP

# Object file and directory definitions 
object_dirs	:= $(addprefix $(RTS_LIB)/objects/, $(MODULES))

.PHONY: all
all:		$(MODULES)

# Create a copy of the tree of all specified modules
$(object_dirs):	
		-mkdir -p $(object_dirs)
		-chmod -R g+wx $(RTS_LIB)/objects

# Create all modules in the corresponding object directory with the makefile
# in the source directory.
.PHONY: $(MODULES)
$(MODULES):	$(object_dirs)
		@$(MAKE) -C $(RTS_LIB)/objects/$@ \
		        -f $(RTS)/$@/GNUmakefile

# Build a TAGS file of all sources
src_dirs	:= $(addprefix $(RTS)/, $(MODULES))
find_sources 	= $(wildcard $(dir)/*.c)
c_files    	= $(foreach dir, $(src_dirs), $(find_sources))

inc_dirs	:= $(addprefix $(RTS)/include/, $(MODULES)) $(RTS)/include
find_include	= $(wildcard $(dir)/*.h)
h_files		= $(foreach dir, $(inc_dirs), $(find_include))

TAGS:		$(c_files) $(h_files)
		@etags $(c_files) $(h_files)


# Cleanup all objects, the Panda library, and the tags file
clean:
		-rm -fr librts.a objects TAGS

