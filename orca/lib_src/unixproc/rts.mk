# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

# rts - dependent stuff

# rts:
RTS		= unixproc

# targets to build:
LIB_TARGETS	= IO dataman misc std system $(SYS_TARGETS)

# includes:
INCL		= $(ORCA_TARGET)/$(VERSION)/include
INCLUDES	= -I$(INCL) \
		  -I$(INCL)/system/$(RTS) \
		  -I$(ORCA_TARGET)/$(VERSION)/std \
		  $(SYS_INCLUDES)
