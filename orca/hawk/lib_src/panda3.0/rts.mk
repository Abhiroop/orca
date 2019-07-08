# rts - dependent stuff

# rts:
RTS		= panda3.0

# targets to build:
LIB_TARGETS	= IO dataman misc std system $(SYS_TARGETS)

# includes:
INCL		= $(ORCA_TARGET)/$(VERSION)/include
INCLUDES	= -I$(INCL) \
		  -I$(INCL)/system/$(RTS) \
		  -I$(ORCA_TARGET)/$(VERSION)/std \
		  -I$(PANDA_PROJ)/panda3.0/include \
		  -I$(PANDA_PROJ)/panda3.0/lib/$(OS_PANDA) \
		  -I$(ORCA_TARGET)/rts/include \
		  -I$(ORCA_TARGET)/rts/include/collection \
		  -I$(ORCA_TARGET)/rts/include/communication \
		  -I$(ORCA_TARGET)/rts/include/synchronization \
		  -I$(ORCA_TARGET)/rts/include/po \
		  -I$(ORCA_TARGET)/rts/include/util \
		  $(SYS_INCLUDES)
