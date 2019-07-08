OUT		= report
OUT.PS          = $(OUT:%=%.ps)

MAINTEXFILE	= main
MAINTEXFILE.TEX = $(MAINTEXFILE:%=%.tex)
MAINTEXFILE.DVI = $(MAINTEXFILE:%=%.dvi)


TEXFILES	+= $(MAINTEXFILE)
TEXFILES	+= intro
TEXFILES	+= problem
TEXFILES	+= seqalg
TEXFILES	+= parprog
TEXFILES	+= paralg
TEXFILES	+= optimiz
TEXFILES	+= perform
TEXFILES	+= concl
TEXFILES.TEX    = $(TEXFILES:%=%.tex)

FIGFILES	+= bitmap_a
FIGFILES	+= bitmap_b
FIGFILES	+= bitmap_c
FIGFILES	+= bitmap_d
FIGFILES	+= bitmap_e
FIGFILES	+= clipAndMerge
FIGFILES	+= dataParallel
FIGFILES	+= dataParObjects
FIGFILES	+= extendingBorders_a
FIGFILES	+= extendingBorders_b
FIGFILES	+= duplicateAndFilter
FIGFILES	+= patchwised
FIGFILES	+= patchObjects
FIGFILES	+= pipelined
FIGFILES	+= polygonExample
FIGFILES	+= worstCase
FIGFILES.FIG    = $(FIGFILES:%=fig/%.fig)
FIGFILES.EPS    = $(FIGFILES:%=%.eps)

MAG_bitmap_a		+= 0.45
MAG_bitmap_b		+= 0.45
MAG_bitmap_c		+= 0.45
MAG_bitmap_d		+= 0.45
MAG_bitmap_e		+= 0.45
MAG_clipAndMerge	+= 0.35
MAG_dataParallel	+= 0.30
MAG_dataParObjects	+= 0.43
MAG_duplicateAndFilter	+= 0.35
MAG_extendingBorders_a	+= 0.50
MAG_extendingBorders_b	+= 0.50
MAG_patchwised		+= 0.30
MAG_patchObjects	+= 0.43
MAG_pipelined		+= 0.35
MAG_polygonExample	+= 0.30
MAG_worstCase		+= 0.30


$(OUT.PS): \
		$(TEXFILES.TEX) \
		$(FIGFILES.EPS)
	latex $(MAINTEXFILE.TEX);
	bibtex $(MAINTEXFILE);
	latex $(MAINTEXFILE.TEX);
	latex $(MAINTEXFILE.TEX);
	dvips $(MAINTEXFILE.DVI) > $(OUT.PS);

short: \
		$(TEXFILES.TEX) \
		$(FIGFILES.EPS)
	latex $(MAINTEXFILE.TEX);
	dvips $(MAINTEXFILE.DVI) > $(OUT.PS);

again:
	latex $(MAINTEXFILE.TEX);
	dvips $(MAINTEXFILE.DVI) > $(OUT.PS);

$(FIGFILES.EPS): \
		$$(@:%.eps=fig/%.fig) \
		makefile
	fig2dev -L pstex -m $($(@:%.eps=MAG_%)) $(@:%.eps=fig/%.fig) $@

front.ps: \
		front.tex
	latex front.tex
	dvips front.dvi > front.ps

## The cleaners:

less:
	rm -f $(MAINTEXFILE.DVI) $(MAINTEXFILE).log $(MAINTEXFILE).aux
	rm -f $(MAINTEXFILE).bbl $(MAINTEXFILE).blg
	rm -f front.dvi front.log front.aux front.bbl front.blg
	rm -f $(FIGFILES.EPS)

clean:
	rm -f $(MAINTEXFILE.DVI) $(MAINTEXFILE).log $(MAINTEXFILE).aux
	rm -f $(MAINTEXFILE).bbl $(MAINTEXFILE).blg
	rm -f $(FIGFILES.EPS)
	rm -f front.dvi front.log front.aux front.bbl front.blg
	rm -f $(OUT.PS) front.ps
