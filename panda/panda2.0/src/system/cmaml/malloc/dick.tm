run.all dickmalloc
1848	72	152	2072	818	dickmalloc.o

time dickmalloc -n200 -t
        4.0 real         3.3 user         0.6 sys  
       13.0 real         3.5 user         0.8 sys  
time dickmalloc -n200					0.3
        4.0 real         3.5 user         0.7 sys  
        8.0 real         3.6 user         0.9 sys  
time dickmalloc -n2000 -t
       37.0 real        35.5 user         1.0 sys  
       59.0 real        38.1 user         3.5 sys  
time dickmalloc -n2000					3.9
       41.0 real        38.7 user         1.7 sys  
       62.0 real        39.5 user         4.0 sys  
time dickmalloc -n20000 -t
      418.0 real       407.9 user         4.4 sys  
      745.0 real       430.6 user        34.0 sys  
time dickmalloc -n20000					37.2
      455.0 real       435.7 user        13.8 sys  
      686.0 real       451.8 user        48.1 sys  

time dickmalloc -n200
n = 200: 20480 bytes occupied
n = 200: 20480 bytes occupied
time dickmalloc -n2000
n = 2000: 167936 bytes occupied
n = 2000: 172032 bytes occupied
time dickmalloc -n20000
n = 20000: 1679360 bytes occupied
n = 20000: 1789952 bytes occupied
