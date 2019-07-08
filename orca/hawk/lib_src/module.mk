# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: module.mk,v 1.3 1997/06/05 16:22:18 ceriel Exp $

# Standard programs
AR		= ar
ARFLAGS		= r
CHMOD		= chmod
CHMODFLAGS	= ug+rw
RANLIB		= ranlib
TAGS		= etags
RM		= /bin/rm
EX		= ex
MAKEDEPEND	= makedepend

CC		= gcc

CPPFLAGS	= $(INCLUDES) \
		  $(DEFINES)

CFLAGS		= $(EXT_FLAGS)

# set FORCE to ALWAYS if you want to recompile everything
FORCE		=

INSTALL		= .			# Must choose some default...

CONFMAKEFILE	= $(INSTALL)/Makefile.$(MODULE)

TARGETLIB =	lib$(MODULE).a
TARGET		= $(INSTALL)/$(TARGETLIB)
LIB_TARGET	= $(TARGET)

# Note: special rules required for assembly source files.

PWD_SRC		= $(SRC:%.c=$(PWD)/%.c) $(SSRC:%.S=$(PWD)/%.S)

OBJ_DIR		= objects
OBJECTS		= $(SRC:%.c=${OBJ_DIR}/%.o) $(SSRC:%.S=${OBJ_DIR}/%.o)

O_FILES		= $(SRC:%.c=%.o) $(SSRC:%.S=%.o)

LIB_OBJECTS	= $(SRC:%.c=$(LIB_TARGET)(%.o)) $(SSRC:%.S=$(LIB_TARGET)(%.o))

# two possible ways of compiling: either with .o files in a sub-directory
# or with rules for making directly into the lib...a. The latter is more
# space-efficient, but also slower.
# Choose "target" and "depend" if you want the sub-directory version,
# "lib_target" and "lib_depend" if you want the direct version.

default:	target

ALWAYS::

target:		${OBJ_DIR} ${TARGET}

${OBJ_DIR}/%.o:	${PWD}/%.c $(FORCE)
		${COMPILE.c} ${PWD}/$*.c
		@/bin/mv $*.o ${OBJ_DIR}/$*.o
		@-${CHMOD} ${CHMODFLAGS} ${OBJ_DIR}/$*.o

${TARGET}: 	${OBJECTS}
		@-${AR} ${ARFLAGS} ${TARGET} ${OBJECTS}
		@-${RANLIB} ${TARGET}
		@-${CHMOD} ${CHMODFLAGS} ${TARGET}

${OBJ_DIR}::
		@if test \! -d $@ ; \
		then \
			mkdir $@ ; \
			${CHMOD} ug+rwx $@ ; \
		fi

depend:
		echo "# This is a generated Makefile; Modify at your own risk" > $(CONFMAKEFILE)
		echo "" >> $(CONFMAKEFILE)
		echo "include $(ORCA_TARGET)/$(VERSION)/make_paths" >> $(CONFMAKEFILE)
		echo "RTS_SRC = " $(RTS_SRC) >> $(CONFMAKEFILE)
		echo "" >> $(CONFMAKEFILE)
		@/bin/cat ${PWD}/template.mk >> ${CONFMAKEFILE}
		${MAKEDEPEND}	-f${CONFMAKEFILE} \
				${CPPFLAGS} \
				${PWD_SRC}
		@-(echo '%s/^.*\/\([A-Za-z_][A-Za-z_0-9]*\.o:\)/$${OBJ_DIR}\/\1'; \
		   echo '%s/^[A-Za-z_][A-Za-z_0-9]*\.o:/$${OBJ_DIR}\/&'; \
		   echo wq) | ${EX} - ${CONFMAKEFILE}
		@-${CHMOD} ${CHMODFLAGS} ${CONFMAKEFILE}
		@-${RM} -f ${CONFMAKEFILE}.bak

clean:		
		-$(RM) -f $(OBJECTS) $(O_FILES) *.bak

# Make rules for making directly into the lib...a

$(LIB_TARGET)(%.o) : $(PWD)/%.c $(FORCE)
		$(COMPILE.c) $(PWD)/$*.c
		@-$(AR) $(ARFLAGS) $(LIB_TARGET) $%
		@-$(RM) -f $%

lib_target:	$(LIB_OBJECTS)
		@-$(RANLIB) $(LIB_TARGET)
		@-$(CHMOD) $(CHMODFLAGS) $(LIB_TARGET)

lib_depend:		
		echo "# This is a generated Makefile; Modify at your own risk" > $(CONFMAKEFILE)
		echo "" >> $(CONFMAKEFILE)
		echo "include $(ORCA_TARGET)/$(VERSION)/make_paths" >> $(CONFMAKEFILE)
		echo "RTS_SRC = " $(RTS_SRC) >> $(CONFMAKEFILE)
		echo "" >> $(CONFMAKEFILE)
		/bin/cat $(PWD)/template.mk >> $(CONFMAKEFILE)
		${MAKEDEPEND}	-f${CONFMAKEFILE} \
				${CPPFLAGS} \
				${PWD_SRC}
		@-(echo '%s/^[A-Za-z_][A-Za-z_0-9]*\.o/$$(TARGET)(&)'; \
		   echo wq) | ${EX} - ${CONFMAKEFILE}
		@-$(CHMOD) $(CHMODFLAGS) $(CONFMAKEFILE)
		@-${RM} ${CONFMAKEFILE}.bak

lib_clean:	clean
		-$(AR) d $(LIB_TARGET) $(O_FILES)

.PRECIOUS:	$(TARGET)

tags:	
		$(TAGS) -t $(SRC)
