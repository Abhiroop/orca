set terminal postscript eps 12
set output 'parix.eps'
set xlabel "Number of Processors per Cluster"
set ylabel "Response Time (ms)"
plot \
'parix8-8.perf' t "256 elements" with linespoints 1 1, \
'parix8-9.perf' t "512 elements" with linespoints 2 1, \
'parix8-10.perf' t "1024 elements" with linespoints 1 2, \
'parix8-11.perf' t "2048 elements" with linespoints 2 2, \
'parix8-12.perf' t "4096 elements" with linespoints 1 3, \
'parix8-13.perf' t "8192 elements" with linespoints 2 3
