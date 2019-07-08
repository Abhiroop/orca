# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: Makefile,v 1.11 1998/11/23 12:57:02 ceriel Exp $

include ../make_paths

install:
	-mkdir -p $(ORCA_TARGET)/bin
	for i in oc_bcast-rpc oc_panda oc_clean oc_sinproc oc_unixproc ancpp-T800 trc_pp orcshot; \
	do \
		sed -e "s+^OC_HOME.*+OC_HOME=$(ORCA_TARGET)+" \
		    -e "s+^ORCA_HOME.*+ORCA_HOME = $(ORCA_TARGET)+" \
		    -e "s+^AMOEBA_HOME.*+AMOEBA_HOME=$(AMOEBA_HOME)+" \
		    -e "s+^PANDA_PROJ.*+PANDA_PROJ=$(PANDA_PROJ)+" \
		    -e "s+^set OC_HOME.*+set OC_HOME = $(ORCA_TARGET)+" \
		    -e "s+^set ORCA_HOME.*+set ORCA_HOME = $(ORCA_TARGET)+" \
		    -e "s+^set AMOEBA_HOME.*+set AMOEBA_HOME = $(AMOEBA_HOME)+" \
		    -e "s+^set PANDA_PROJ.*+set PANDA_PROJ = $(PANDA_PROJ)+" \
			< $(ORCA_HOME)/src/scripts/$$i > $(ORCA_TARGET)/bin/$$i ; \
		chmod -f +x $(ORCA_TARGET)/bin/$$i ; \
	done
	-mkdir -p $(ORCA_TARGET)/man/man1
	cd $(ORCA_HOME)/src/scripts; /bin/cp oc_clean.1 oc_bcast-rpc.1 oc_panda.1 oc_sinproc.1 oc_unixproc.1 $(ORCA_TARGET)/man/man1
	/bin/cp $(ORCA_HOME)/src/scripts/makedepend $(ORCA_TARGET)/bin/makedepend

prun_install:
	sed -e "s+^OC_HOME.*+OC_HOME=$(ORCA_HOME)+" \
	    -e "s+^ORCA_HOME.*+ORCA_HOME = $(ORCA_HOME)+" \
	    -e "s+^AMOEBA_HOME.*+AMOEBA_HOME=$(AMOEBA_HOME)+" \
	    -e "s+^PANDA_PROJ.*+PANDA_PROJ=$(PANDA_PROJ)+" \
	    -e "s+^set OC_HOME.*+set OC_HOME = $(ORCA_HOME)+" \
	    -e "s+^set ORCA_HOME.*+set ORCA_HOME = $(ORCA_HOME)+" \
	        < prun > $(ORCA_TARGET)/bin/prun ;
	chmod +x $(ORCA_TARGET)/bin/prun
	/bin/cp $(ORCA_HOME)/src/scripts/watchit $(ORCA_TARGET)/bin
	chmod -f  +x $(ORCA_TARGET)/bin/watchit
	/bin/cp prun.1 $(ORCA_HOME)/man/man1
