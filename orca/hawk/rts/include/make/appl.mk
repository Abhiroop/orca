# This makefile can be included by the makefile that builds an RTS application.
# That makefile should define:
#
# RTS		The RTS root directory
# PANDA		The Panda root directory
# RTS_CONF	The RTS configuration to use
# ARCH          The architecture for which the application is generated
# VERSION       Optional stage in the path from $(PANDA) to the library
#               directory, like "latest_greatest" or "test"
# SRC_DIR	The directory that contains the application sources
# C*            Optional compiler flags
# LIBS          
# LDFLAGS       Optional linker flags
#
# Finally, the makefile should include this makefile with:
# include $(RTS)/include/make/appl.mk

# Add compiler flags
CPPFLAGS        += \
		-I$(PANDA)/include \
		-I$(RTS)/include \
		-I$(RTS)/include/communication \
		-I$(RTS)/include/synchronization \
		-I$(RTS)/include/collection \
		-I$(RTS)/include/util \
		-I$(RTS)/include/po
CFLAGS          += 
LDFLAGS         += \
		-L$(PANDA)/$(VERSION)/lib/$(ARCH) \
		-L$(RTS)/conf/$(RTS_CONF)
LIBS            += -lrts -lpanda -lm
 
# Find source files in the source directory
vpath %.c       $(SRC_DIR)
 
# Find the library in the architecture lib dir
vpath %.a       $(PANDA)/$(VERSION)/lib/$(ARCH)
vpath %.a       $(RTS)/conf/$(RTS_CONF)
 
# Generate a dependency file for each source file. The dependency file
# itself also depends on the generated dependencies.
%.d:    %.c
	$(SHELL) -ec '$(DEP) $(CPPFLAGS) $< |sed '\''s/$*.o/& $@/g'\'' > $@'
 
# Include all dependency makefiles (.d files)
include $(patsubst %.c, %.d, $(notdir $(wildcard $(SRC_DIR)/*.c)))
 
# Include the architecture specific setup
include $(PANDA)/$(VERSION)/lib/$(ARCH)/setup.mk


.PHONY: clean
clean:  
		rm -f *.o core

.PHONY: cleanall
cleanall:
		$(MAKE) clean
		rm -f *.d

