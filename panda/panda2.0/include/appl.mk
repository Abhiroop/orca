# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# This makefile should be included by the makefile that generates a
# Panda application. That makefile should define:
#
# PANDA		The Panda root directory
# ARCH		The architecture for which the application is generated
# SRC_DIR	The directory that contains the application sources
# C*		Optional compiler flags
# LIBS		
# LDFLAGS	Optional linker flags
# VERSION	Optional stage in the path from $(PANDA) to the library
#		directory, like "latest_greatest" or "test"
#
# Finally, the makefile should include this makefile with:
# include $(PANDA)/include/appl.mk

# Add compiler flags
CPPFLAGS	+= -I$(PANDA)/include -I$(SRC_DIR) 
CFLAGS		+= 
LDFLAGS		+= -L$(PANDA)/$(VERSION)/lib/$(ARCH)
LIBS		+= -lpanda -lm

# Find source files in the source directory
vpath %.c	$(SRC_DIR)

# Find the library in the architecture lib dir
vpath %.a	$(PANDA)/$(VERSION)/lib/$(ARCH)

# Generate a dependency file for each source file. The dependency file
# itself also depends on the generated dependencies.
%.d:    %.c
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $< | sed '\''s/$*.o/& $@/g'\'' > $@'
 
# Include all dependency makefiles (.d files)
include $(patsubst %.c, %.d, $(notdir $(wildcard $(SRC_DIR)/*.c)))

# Include the architecture specific setup
include $(PANDA)/$(VERSION)/lib/$(ARCH)/setup.mk


