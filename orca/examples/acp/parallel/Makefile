AOUT=acp

default:
	oc_unixproc -LIN -CHK -O2 -o $(AOUT) acp.imp

amoeba:
	oc_bcast-rpc -LIN -CHK -O2 -o $(AOUT) acp.imp

panda:
	oc_panda -o $(AOUT) acp.imp

profile:
	oc_unixproc -pg acp.imp

clean:
	oc_clean
        rm -f acp a.out
		
