# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# This makefile should be included by the makefile for a module. That 
# makefile should define:
#
# FILES		ALL files needed for this system module
# C*		Additional compiler flags (use +=) CFLAGS, CPPFLAGS
#
# Finally, the makefile should include this makefile with:
# include $(PANDA)/include/system.mk
#
# The following variables are passed from the main Makefile:
#
# PANDA		The root directory of the Panda sources
# PANDA_LIB	The directory of the library in which the object files
#		have to be placed

export FILES CFLAGS CPPFLAGS 

LIB		:= $(PANDA_LIB)/libpanda.a

C_SRC		:= $(notdir $(filter %.c, $(FILES)))
ASM_SRC		:= $(notdir $(filter %.S, $(FILES)))
OBJ		:= $(C_SRC:%.c=%.o) $(ASM_SRC:%.S=%.o)

MAKEFILE	:= $(PANDA)/src/system/$(SYSTEM)/Makefile

do:		
		$(MAKE) -f $(MAKEFILE) copy
		$(MAKE) DEPEND=1 $(LIB)

# Add these object files to the library
$(LIB):		$(OBJ)
		-chmod g+w $(OBJ)
		$(AR) $(ARFLAGS) $(LIB) $(OBJ)
ifdef RANLIB
		$(RANLIB) $(LIB)
endif
		-chmod g+w $(LIB)

# Copy all source files into one directory
org_files	:= $(addprefix $(PANDA)/src/system/, $(FILES))

.PHONY: copy
copy:
		@$(PANDA)/bin/update.pl $(MAKEFILE) $(org_files)

# Generate a dependency file for each source file. The dependency file
# itself also depends on the generated dependencies.
%.d:	%.c
	$(SHELL) -ec '$(CC) -M $(CPPFLAGS) $< | sed '\''s/$*.o/& $@/g'\'' > $@'
	-chmod g+w $@

# Add tags to file TAGFILE
files		:= $(addprefix $(PANDA_LIB)/objects/system/, $(notdir $(FILES)))

.PHONY: TAGS
TAGS:		
		etags -a -o $(TAGFILE) $(files)


# Only include dependency files when building library
ifeq ($(DEPEND), 1)
include $(patsubst %.c, %.d, $(C_SRC))
endif
