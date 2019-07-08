BINDIR = /home/saniya/bin/sh/amoeba
APPDIR = /home/po/apps/ntr
OCDIR = ./.oc_driver

ntr::
	/net/orca/hawk/bin/oc_panda -g -amoeba ntr.imp -o ntr
	ainstall ntr $(APPDIR)/ntr
	$(BINDIR)/tobn $(OCDIR)/global.c $(APPDIR)/global.c
	$(BINDIR)/tobn $(OCDIR)/ntr.c $(APPDIR)/ntr.c
	$(BINDIR)/tobn $(OCDIR)/MiscUtil.c $(APPDIR)/MiscUtil.c
	$(BINDIR)/tobn $(OCDIR)/InprObject.c $(APPDIR)/InprObject.c

