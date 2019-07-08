# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Panda distribution.

# This makefile should be included by the makefile for a module. That 
# makefile should define:
#
# SRC		All source file that belong to the module
# MODULE	The module source directory
# C*		Additional compiler flags (use +=) CFLAGS, CPPFLAGS
#
# Finally, the makefile should include this makefile with:
# include $(RTS)/include/make/module.mk
#
# The following variables are passed from the main Makefile:
#
# RTS		The root directory of the RTS distribution
# RTS_LIB	The directory of the library in which the object files
#		have to be placed

LIB		:= $(RTS_LIB)/librts.a

# Find the source files in the corresponding module source directory, or
# a generic source directory (optional)
vpath %.S	$(MODULE)
vpath %.s	$(MODULE)
vpath %.c	$(MODULE)

C_SRC		:= $(filter %.c, $(SRC))
ASM_SRC		:= $(filter %.S, $(SRC))
OBJ		:= $(C_SRC:%.c=%.o) $(ASM_SRC:%.S=%.o)

$(LIB):		$(OBJ)
		@-chmod g+w $(OBJ)
		$(AR) $(ARFLAGS) $(LIB) $(OBJ)
ifdef RANLIB
		$(RANLIB) $(LIB)
endif
		@-chmod g+rw $(LIB)

# Generate a dependency file for each source file. The dependency file
# itself also depends on the generated dependencies.
%.d:	%.c
	@$(DEP) $(CPPFLAGS) $< | sed 's/$*.o/& $@/g' > $@
	@-chmod g+w $@
	@echo Updated: $@

# RFHH add rule for .S and .s compilation

%.o: %.s
		$(COMPILE.s) $(CPPFLAGS) $<

%.o: %.S
		$(COMPILE.s) $(CPPFLAGS) $<


# Include all dependency makefiles (.d files)
include $(patsubst %.c, %.d, $(C_SRC))
