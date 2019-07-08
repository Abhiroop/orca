# rts - dependent stuff

# rts:
RTS		= panda

# targets to build:
LIB_TARGETS	= IO dataman misc std system $(SYS_TARGETS)

# includes:
INCL		= $(ORCA_TARGET)/$(VERSION)/include
INCLUDES	= -I$(INCL) \
		  -I$(INCL)/system/$(RTS) \
		  -I$(ORCA_TARGET)/$(VERSION)/std \
		  -I$(PANDA_PROJ)/$(VERSION)/include \
		  $(SYS_INCLUDES)
