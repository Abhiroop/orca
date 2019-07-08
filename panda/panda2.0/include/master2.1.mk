# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# This makefile should be included by the master makefile that creates
# the Panda library. That makefile should define:
#
# PANDA		The directory of the Panda distribution
# MODULES	All modules that should be included in the library. The
#		modules are specified as subdirectories of $(PANDA)/src.
# SYSTEM	The name of the system module, a subdirectory of 
# 		$(PANDA)/src/system.
# C*		compiler flags as CC, CPPFLAGS, and CFLAGS
# AR*		ar command and flags AR, ARFLAGS
# AS*		as command and flags AS, ASFLAGS
#
# Finally, the makefile should include this makefile with:
# include $(PANDA)/include/master.mk

SHELL		:= /bin/sh

# Library directory = current directory
PANDA_LIB	:= $(shell pwd)

export PANDA PANDA_LIB CC CPPFLAGS CFLAGS AS ASFLAGS AR ARFLAGS RANLIB SYSTEM

# Object file and directory definitions 
object_dirs	:= $(addprefix $(PANDA_LIB)/objects/, $(MODULES)) \
		   $(PANDA_LIB)/objects/system

.PHONY: default
default:	system $(MODULES)

# Create a copy of the tree of all specified modules
$(object_dirs):	
		-mkdir -p $(object_dirs)
		-chmod -R g+wx $(PANDA_LIB)/objects

# Create all modules in the corresponding object directory with the makefile
# in the source directory.
.PHONY: $(MODULES)
$(MODULES):	$(object_dirs)
		$(MAKE) -C $(PANDA_LIB)/objects/$@ \
		        -f $(PANDA)/src/$@/Makefile

.PHONY: system
system:		$(PANDA_LIB)/objects/system
		$(MAKE) -C $(PANDA_LIB)/objects/system \
		        -f $(PANDA)/src/system/$(SYSTEM)/Makefile



# Create a tags file for all source files in the modules and system
tag_modules	:= $(addsuffix .tags, $(MODULES))
TAGFILE		:= $(PANDA_LIB)/TAGS
export TAGFILE

.PHONY: TAGS
TAGS:		
		> TAGS
		$(MAKE) system.tags $(tag_modules)

.PHONY: system.tags
system.tags:	
		$(MAKE) -C $(PANDA_LIB)/objects/system \
			-f $(PANDA)/src/system/$(SYSTEM)/Makefile TAGS


%.tags:	
		$(MAKE) -C $(PANDA_LIB)/objects/$* \
			-f $(PANDA)/src/$*/Makefile TAGS

# Cleanup all objects, the Panda library, and the tags file
clean:
		-rm -fr libpanda.a objects TAGS

