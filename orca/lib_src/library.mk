# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: library.mk,v 1.6 1997/06/05 16:21:50 ceriel Exp $

# rts - and system - independent stuff. This file is to be included
# in the rts - and system - specific Makefile.

PWD		= $(ORCA_TARGET)/$(VERSION)/$(RTS)/$(MACH)/$(SPECIAL)
RTS_SRC		= $(ORCA_HOME)/src/rts

TARGETLIB	= lib$(RTS).a

.PRECIOUS:      lib$(RTS).a

default:        $(LIB_TARGETS)

ALWAYS ::

%:		Makefile.% ALWAYS
		@echo make $@
		@$(MAKE) $(MFLAGS) \
			-f Makefile.$@ \
			ORCA_HOME=$(ORCA_HOME) \
			CFLAGS="$(EXT_FLAGS)" \
			CPPFLAGS="$(INCLUDES) $(DEFINES)" \
			INSTALL="$(PWD)" \
			RANLIB="$(RANLIB)" \
			FORCE="$(FORCE)" \
			TARGETLIB="$(TARGETLIB)" \
			VERSION="$(VERSION)" \
			CC="$(CC)" \
			AR="$(AR)" \
			ARFLAGS="$(ARFLAGS)"
		@echo $@ done

Makefile.system :	$(RTS_SRC)/system/$(RTS)/template.mk
		$(MAKE) $(MFLAGS) \
			-f $(RTS_SRC)/system/$(RTS)/template.mk \
			ORCA_HOME=$(ORCA_HOME) \
			ORCA_TARGET=$(ORCA_TARGET) \
			RTS_SRC=$(RTS_SRC) \
			CPPFLAGS="$(INCLUDES) $(DEFINES)" \
			VERSION="$(VERSION)" \
			CONFMAKEFILE="$(PWD)/$@" \
			depend

Makefile.std :	$(RTS_SRC)/std/template.mk
		$(MAKE) $(MFLAGS) \
			-f $(RTS_SRC)/$(@:Makefile.%=%)/template.mk \
			ORCA_HOME=$(ORCA_HOME) \
			ORCA_TARGET=$(ORCA_TARGET) \
			RTS_SRC=$(ORCA_TARGET)/$(VERSION) \
			CPPFLAGS="$(INCLUDES) $(DEFINES)" \
			VERSION="$(VERSION)" \
			CONFMAKEFILE="$(PWD)/$@" \
			depend

Makefile.% :	$(RTS_SRC)/%/template.mk
		$(MAKE) $(MFLAGS) \
			-f $(RTS_SRC)/$(@:Makefile.%=%)/template.mk \
			ORCA_HOME=$(ORCA_HOME) \
			ORCA_TARGET=$(ORCA_TARGET) \
			RTS_SRC=$(RTS_SRC) \
			CPPFLAGS="$(INCLUDES) $(DEFINES)" \
			VERSION="$(VERSION)" \
			CONFMAKEFILE="$(PWD)/$@" \
			depend

# only for unixproc on SunOS4:
Makefile.threads :	$(RTS_SRC)/system/$(RTS)/threads/src/template.mk
		$(MAKE) $(MFLAGS) \
			-f $(RTS_SRC)/system/$(RTS)/threads/src/template.mk \
			ORCA_HOME=$(ORCA_HOME) \
			ORCA_TARGET=$(ORCA_TARGET) \
			RTS_SRC=$(RTS_SRC) \
			CPPFLAGS="$(INCLUDES) $(DEFINES)" \
			VERSION="$(VERSION)" \
			CONFMAKEFILE="$(PWD)/$@" \
			depend

depend:		$(LIB_TARGETS:%=Makefile.%)

clean:
		for mod in $(LIB_TARGETS) ;\
		do \
		    if [ -f Makefile.$${mod} ] ;\
		    then \
			$(MAKE) -f Makefile.$${mod} \
				INSTALL="$(PWD)" \
				TARGETLIB="$(TARGETLIB)" \
				VERSION="$(VERSION)" \
				clean ;\
		    fi ;\
		done

lib_clean:
		for mod in $(LIB_TARGETS) ;\
		do \
		    if [ -f Makefile.$${mod} ] ;\
		    then \
			$(MAKE) -f Makefile.$${mod} \
				INSTALL="$(PWD)" \
				TARGETLIB="$(TARGETLIB)" \
				VERSION="$(VERSION)" \
				lib_clean ;\
		    fi ;\
		done

allclean:
		for mod in $(LIB_TARGETS) ;\
		do \
			/bin/rm -f Makefile.$${mod} ;\
		done
		/bin/rm -rf objects *.o lib$(RTS).a
