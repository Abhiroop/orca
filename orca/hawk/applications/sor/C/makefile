BINSH = /home/saniya/bin/sh

am_sor::
	amake am_sor
	$(BINSH)/amoeba/tobn sor.c /home/po/apps/sor/sor.c
	$(BINSH)/amoeba/tobn grid.c /home/po/apps/sor/grid.c
	ainstall am_sor /home/po/apps/sor/am_sor

am_sor_d::
	amake -D__MAL_DEBUG am_sor_d
	$(BINSH)/amoeba/tobn sor.c /home/po/apps/sor/sor.c
	$(BINSH)/amoeba/tobn grid.c /home/po/apps/sor/grid.c
	ainstall am_sor_d /home/po/apps/sor/am_sor_d

fm_sor::
	amake fm_sor
	$(BINSH)/amoeba/tobn sor.c /home/myrinet/sor.c
	$(BINSH)/amoeba/tobn grid.c /home/myrinet/grid.c
	ainstall fm_sor /home/myrinet/fm_sor

