# rts - dependent stuff

# rts:
RTS		= bcast-rpc.opt

# targets to build:
LIB_TARGETS	= IO dataman misc std system $(SYS_TARGETS)

# includes:
INCL		= $(ORCA_TARGET)/$(VERSION)/include
INCLUDES	= -I$(INCL) \
		  -I$(INCL)/system/$(RTS) \
		  -I$(ORCA_TARGET)/$(VERSION)/std \
		  $(SYS_INCLUDES)
