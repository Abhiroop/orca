INSTALL_DIR	= /usr/local/VU/panda

install:	$(INSTALL_DIR)/bin/panc

$(INSTALL_DIR)/bin/panc: $(INSTALL_DIR)/bin	panc
		sed "s+PANDA_DIR=.*+PANDA_DIR=$(INSTALL_DIR)+" < panc > $@
		chmod +x $@

$(INSTALL_DIR)/bin:
		mkdir -p $@
