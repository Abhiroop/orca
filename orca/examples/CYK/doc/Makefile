all:		scriptie.ps

distr:		scriptie.ps

scriptie.ps:	scriptie speedup.dyn speedup.stat fig2.1.ps fig3.3.ps \
			fig3.5.ps fig5.3.ps fig3.2.ps fig3.4.ps fig5.1.ps
		grap scriptie | pic | tbl | eqn \
			| psfig | troff -ms voorblad.trf - | devps >  scriptie.ps

voorblad.ps:	voorblad.trf
		troff -ms voorblad.trf | devps > voorblad.ps

fig%.ps:	fig%.fig
		fig2dev -L ps $(@:fig%.ps=fig%.fig) $@

print:		fig2.1.ps fig3.3.ps fig3.5.ps fig5.3.ps \
			fig3.2.ps fig3.4.ps fig5.1.ps
		grap scriptie | pic | tbl | eqn \
			| psfig | troff -ms voorblad.trf - | devps | lpr

clean:
		rm -f fig*.ps

allclean:
		rm -f *.ps
