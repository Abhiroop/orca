# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# $Id: Makefile,v 1.11 1997/06/05 11:12:47 ceriel Exp $

ORCA_HOME	= /usr/proj/orca
AMOEBA_HOME	= /usr/proj/amwork
PANDA_PROJ	= /usr/proj/panda

install:
	-mkdir $(ORCA_HOME)/bin
	for i in oc_bcast-rpc oc_panda oc_clean oc_sinproc oc_unixproc ancpp-T800; \
	do \
		sed -e "s+^OC_HOME.*+OC_HOME=$(ORCA_HOME)+" \
		    -e "s+^ORCA_HOME.*+ORCA_HOME = $(ORCA_HOME)+" \
		    -e "s+^AMOEBA_HOME.*+AMOEBA_HOME=$(AMOEBA_HOME)+" \
		    -e "s+^PANDA_PROJ.*+PANDA_PROJ=$(PANDA_PROJ)+" \
		    -e "s+^set OC_HOME.*+set OC_HOME = $(ORCA_HOME)+" \
		    -e "s+^set ORCA_HOME.*+set ORCA_HOME = $(ORCA_HOME)+" \
			< $$i > $(ORCA_HOME)/bin/$$i ; \
		chmod +x $(ORCA_HOME)/bin/$$i ; \
	done
	/bin/cp watchit $(ORCA_HOME)/bin
	chmod +x $(ORCA_HOME)/bin/watchit
	-mkdir $(ORCA_HOME)/man
	-mkdir $(ORCA_HOME)/man/man1
	/bin/cp oc_bcast-rpc.1 oc_panda.1 oc_sinproc.1 oc_unixproc.1 $(ORCA_HOME)/man/man1
	/bin/cp makedepend $(ORCA_HOME)/bin/makedepend

prun_install:
	sed -e "s+^OC_HOME.*+OC_HOME=$(ORCA_HOME)+" \
	    -e "s+^ORCA_HOME.*+ORCA_HOME = $(ORCA_HOME)+" \
	    -e "s+^AMOEBA_HOME.*+AMOEBA_HOME=$(AMOEBA_HOME)+" \
	    -e "s+^PANDA_PROJ.*+PANDA_PROJ=$(PANDA_PROJ)+" \
	    -e "s+^set OC_HOME.*+set OC_HOME = $(ORCA_HOME)+" \
	    -e "s+^set ORCA_HOME.*+set ORCA_HOME = $(ORCA_HOME)+" \
		< prun > $(ORCA_HOME)/bin/prun ;
	chmod +x $(ORCA_HOME)/bin/prun
	/bin/cp prun.1 $(ORCA_HOME)/man/man1
