all: TaskData.eps Worker.eps parix.eps fft.eps 

TaskData.eps: TaskData.txt
	if text2ps -p 8 < TaskData.txt > TaskData.ps; then exit 0; else exit 0; fi
	ps2epsi TaskData.ps TaskData.eps
	rm TaskData.ps 

Worker.eps: Worker.txt
	if text2ps -p 8 < Worker.txt > Worker.ps; then exit 0; else exit 0; fi
	ps2epsi Worker.ps Worker.eps
	rm Worker.ps 

parix.eps:	parix8-10.perf parix8-12.perf parix8-8.perf parix8-11.perf \
		parix8-13.perf parix8-9.perf parix.bat
		gnuplot parix.bat

fft.eps:	fft.fig
		fig2dev -L ps fft.fig > fft.eps
