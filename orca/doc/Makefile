# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

TARGETS = refman.ps userman.ps interface.ps

all:		$(TARGETS)

distr:		$(TARGETS)

%.ps:		%.dvi
		dvips $< > $@

refman.dvi:	refman.tex refman.bib
		latex refman.tex
		bibtex refman
		latex refman.tex
		latex refman.tex

%.dvi:		%.tex
		latex $<
		latex $<

refman.html:	refman.tex refman.dvi
		latex2html -info "" -split 4 -show_section_numbers refman.tex
		( cd refman ; \
		  for f in *.html ; \
		    do mv $$f interm ; \
		    sed -f ../script.sed < interm > $$f ; \
		  done )

userman.html:	userman.tex userman.dvi
		latex2html -info "" -split 4 -show_section_numbers userman.tex
		( cd userman ; \
		  for f in *.html ; \
		    do mv $$f interm ; \
		    sed -f ../script.sed < interm > $$f ; \
		  done )

clean:
		rm -rf *.aux *.dvi *.log *.bbl *.blg */TMP

allclean:	clean
		rm -f *.ps userman refman
