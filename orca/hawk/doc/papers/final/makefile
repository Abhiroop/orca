FILES =		paper.tex \
		paper.sty \
		runlatex \
		biblio.bib \
		publications.bib \
		distribution.pstex \
		distribution.pstex_t \
		eps/overview.eps \
		eps/overlap.eps \
		eps/e2p.eps \
		plots/fft.eps \
		plots/fft.spd.eps \
		plots/ntr.eps \
		plots/ntr.spd.eps \
		plots/sor.eps \
		plots/sor.spd.eps \
		plots/jacobi.eps \
		plots/jacobi.spd.eps

all:	paper.dvi

paper.dvi:	$(FILES)
	./runlatex paper

paper.ps:	paper.dvi
	dvips -Poce paper.dvi > paper.ps

distribution.pstex: fig/distribution.fig
	fig2dev -L pstex fig/distribution.fig distribution.pstex

distribution.pstex_t: fig/distribution.fig
	fig2dev -L pstex_t -p distribution.pstex fig/distribution.fig distribution.pstex_t

eps/overview.eps: fig/overview.fig
	fig2dev -L ps fig/overview.fig eps/overview.eps

eps/overlap.eps: fig/overlap.fig
	fig2dev -L ps fig/overlap.fig eps/overlap.eps

eps/e2p.eps: fig/e2p.fig
	fig2dev -L ps fig/e2p.fig eps/e2p.eps

clean:
	rm -f paper.blg paper.bbl paper.aux paper.log paper.dvi paper.ps
