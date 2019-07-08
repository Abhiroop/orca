PANDA		= /usr/proj/panda/panda2.0

default:	panda.ps

panda.ps:	panda.dvi
		dvips panda.dvi >panda.ps

panda.dvi:	panda.tex pan_sys.tex pan_mp.tex pan_rpc.tex \
		pan_group.tex saw.tex pan_group_debug.tex
		latex panda.tex

pan_sys.tex:	$(PANDA)/include/pan_sys.h
		c2latex $(PANDA)/include/pan_sys.h >pan_sys.tex

pan_mp.tex:	$(PANDA)/include/pan_mp.h
		c2latex $(PANDA)/include/pan_mp.h >pan_mp.tex

pan_rpc.tex:	$(PANDA)/include/pan_rpc.h
		c2latex $(PANDA)/include/pan_rpc.h >pan_rpc.tex

pan_group.tex:	$(PANDA)/include/pan_group.h
		c2latex $(PANDA)/include/pan_group.h >pan_group.tex

saw.tex:	$(PANDA)/tst/src/system/saw/saw.c
		c2latex $(PANDA)/tst/src/system/saw/saw.c >saw.tex

pan_group_debug.tex: $(PANDA)/include/debug/pan_group.h
		c2latex $(PANDA)/include/debug/pan_group.h >\
		pan_group_debug.tex

clean:	
		rm -f *.dvi *.log *.aux panda.ps pan_sys.tex \
		pan_mp.tex pan_rpc.tex pan_group.tex
