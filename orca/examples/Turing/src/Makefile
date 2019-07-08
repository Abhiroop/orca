FLAGS = -O -v
						
default: unix_proc

amoeba:  
	oc_panda -amoeba $(FLAGS) -o Turing.amoeba Turing.imp

unix_proc:
	oc_unixproc $(FLAGS) -o Turing.unix Turing.imp 

panda:
	oc_panda $(FLAGS) -o Turing.panda Turing.imp

clean:
	oc_clean
