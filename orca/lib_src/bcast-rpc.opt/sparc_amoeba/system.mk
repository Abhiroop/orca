# rts- and architecture-specific options

# include paths
include $(BACK)../../make_paths

# which machine and programs?
AMCONF          = $(AMOEBA_HOME)/conf/amoeba

AMOEBA_GNU      = $(AMCONF)/sparc.gnu-2
TOOLSET         = $(AMOEBA_GNU)/toolset
AMOEBA_BIN      = $(AMOEBA_HOME)/bin.$(AM_BINSUF)/gnu

AMCFLAGS        = -mam_sparc -G $(AMOEBA_BIN)
AMCDEFINES      = -Dsparc -DAMOEBA -nodeps -mv8 -fno-builtin

MACH		= sparc_amoeba
CC		= $(TOOLSET)/do_gcc $(AMCFLAGS) $(AMCDEFINES)
AR		= $(TOOLSET)/do_ar $(AMCFLAGS)
ARFLAGS		= crs
RANLIB		= :

SYS_INCLUDES	= \
		-I$(AMOEBA_HOME)/src/h \
		-I$(AMOEBA_HOME)/src/h/posix \
		-I$(AMOEBA_HOME)/src/h/toolset/gnu-2

DEFINES		= $(SPEC_DEFINES) -DOPTIMIZED -DPREEMPTIVE
EXT_FLAGS	= $(NAMELIST:Y=-g) \
		  $(PROFILING:Y=-pg) \
		  $(OPTIM:Y=-O2) \
		  $(EXTRAOPTIM:Y=-finline-functions -fomit-frame-pointer)

# include rts-specific stuff
include $(BACK)../rts.mk

# include master make template
include $(ORCA_TARGET)/$(VERSION)/library.mk
