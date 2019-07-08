AOUT=acp

default:
	oc_unixproc -LIN -CHK -O2 -o $(AOUT) -finline-functions -funroll-loops acp.imp

amoeba:
	oc_bcast-rpc -LIN -CHK -o $(AOUT) acp.imp

profile:
	oc_unixproc -pg -LIN -CHK acp.imp

clean:
	oc_clean
	rm -f a.out acp
