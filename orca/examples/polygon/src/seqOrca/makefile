## The different sequential versions

OUT		+= seqLSDOverlay
OUT		+= seqLSDOnceInOverlay
OUT		+= seqLSDOnceInNoOutOverlay

## The conditional compiling constants.

#CPP_ALL_FLAGS			+= -DDEBUG

#CPP_PANDA_TRACE_FLAGS		+= -DTIMING_TRACE
#CPP_PANDA_TRACE_FLAGS		+= -DTRACE_LEVEL=10000

FLAGSseqLSDOnceInNoOutOverlay	+= -DNO_OUTPUT
FLAGSseqLSDOnceInNoOutOverlay	+= -DONCE_INPUT

FLAGSseqLSDOnceInOverlay	+= -DONCE_INPUT


## Files that need to be run trough $(CPP)

PRECPP		:sh= ls P.* 2> /dev/null


## Files that are created after they have been run trough $(CPP)

POSTCPP		= $(PRECPP:P.%=%)


## If these files change then recompile.

ORCADEPEND	:sh= for i in *.spf *.imp; do     \
		         if [ -f "P.$i" ]; then   \
			     echo "" > /dev/null; \
		         else                     \
			     echo "$i";           \
		         fi;                      \
		     done
ORCADEPEND	+= makefile


## Preprocessor

CPP		= cpp


## Orca compilers

OC_UNIX		= oc_unixproc

OC_BCAST	= oc_bcast-rpc
#OC_BCAST	= /usr/proj/orca/latest_greatest/bin/oc_bcast-rpc

OC_PANDA	= oc_panda -v
#OC_PANDA	= /usr/proj/orca/latest_greatest/bin/oc_panda

OC_PANDA_TRACE	= oc_panda
#OC_PANDA_TRACE	= /home/raoul/bin/sh/oc_trace


## Orca options

OC_FLAGS	+= -CHK
OC_FLAGS	+= -LIN
OC_FLAGS	+= -O2
#OC_FLAGS	+= -qpt
OC_FLAGS	+= --O
#OC_FLAGS	+= -g
#OC_FLAGS	+= -pg
OC_FLAGS	+= -v

OC_CFLAGS	+= -finline-functions
OC_CFLAGS	+= -funroll-loops

OC_UNIX_FLAGS	+= $(OC_FLAGS)

OC_BCAST_FLAGS	+= -sparc
OC_BCAST_FLAGS	+= -opt
OC_BCAST_FLAGS	+= $(OC_FLAGS)

OC_PANDA_FLAGS	+= -amoeba
OC_PANDA_FLAGS	+= $(OC_FLAGS)

OC_PANDA_TRACE_FLAGS	+= -trc
OC_PANDA_TRACE_FLAGS	+= $(OC_PANDA_FLAGS)


## Makefile in which is dealt with preprocessing

PREPROCESSING_MAKEFILE = makefile


## Make overlay versions

SRC		= Main.imp

unix: $(OUT)
bcast: $(OUT:%=%.bcast)
panda: $(OUT:%=%.panda)
pandaTrace: $(OUT:%=%.panda.trace)

$(OUT):  $(ORCADEPEND)
	@ echo "#### $@ ####"
	@ $(MAKE) -f $(PREPROCESSING_MAKEFILE) PreClean
	@ $(MAKE) -f $(PREPROCESSING_MAKEFILE)        \
	       CPPFLAGS="$(CPP_ALL_FLAGS) $(FLAGS$@)" \
	       Preprocessing
	$(OC_UNIX) $(OC_UNIX_FLAGS) -o $@ $(SRC)

$(OUT:%=%.bcast):  $(ORCADEPEND)
	@ echo "#### $@ ####"
	@ $(MAKE) -f $(PREPROCESSING_MAKEFILE) PreClean
	@ $(MAKE) -f $(PREPROCESSING_MAKEFILE)                     \
	       CPPFLAGS="$(CPP_ALL_FLAGS) $(FLAGS$(@:%.bcast=%))" \
	       Preprocessing
	$(OC_BCAST) $(OC_BCAST_FLAGS) -o $@ $(SRC)
	ainstall $@ /home/$@
	# rm $@

$(OUT:%=%.panda):  $(ORCADEPEND)
	@ echo "#### $@ ####"
	@ $(MAKE) -f $(PREPROCESSING_MAKEFILE) PreClean
	@ $(MAKE) -f $(PREPROCESSING_MAKEFILE)               \
	  CPPFLAGS="$(CPP_ALL_FLAGS) $(FLAGS$(@:%.panda=%))" \
	       Preprocessing
	$(OC_PANDA) $(OC_PANDA_FLAGS) -o $@ $(SRC)
	ainstall $@ /home/$@
	rm $@

$(OUT:%=%.panda.trace):  $(ORCADEPEND)
	@ echo "#### $@ ####"
	@ $(MAKE) -f $(PREPROCESSING_MAKEFILE) PreClean
	@ $(MAKE) -f $(PREPROCESSING_MAKEFILE)               \
	  CPPFLAGS="$(CPP_ALL_FLAGS) $(CPP_PANDA_TRACE_FLAGS) $(FLAGS$(@:%.panda.trace=%))" \
	       Preprocessing
	export OC_CFLAGS
	echo popi $(OC_CFLAGS)
	$(OC_PANDA) $(OC_PANDA_TRACE_FLAGS) -o $@ $(SRC)
	ainstall $@ /home/$@
	# rm $@


## The cleaners

less: PreClean
	rm -f .*.db
	rm -f Overlay.ps
	rm -f $(OUT:%=%.bcast)
	rm -f $(OUT:%=%.panda)
	rm -f $(OUT:%=%.panda.trace)
	rm -fr .oc_driver

clean: PreClean
	rm -f $(OUT)
	rm -f .*.db
	rm -f Overlay.ps
	rm -f $(OUT:%=%.bcast)
	rm -f $(OUT:%=%.panda)
	rm -f $(OUT:%=%.panda.trace)
	rm -fr .oc_driver


print:
	pr \
	   Main.spf P.Main.imp \
	   P.BoxModule.spf P.BoxModule.imp \
	   BoxListModule.spf BoxListModule.imp \
	   OwnMath.spf OwnMath.imp  \
	| lpr -J"Overlay"


vgrind:
	vgrind -loc -d ../.vgrindefs -h Overlay \
	       Main.spf P.Main.imp \
	       P.BoxModule.spf P.BoxModule.imp \
	       BoxListModule.spf BoxListModule.imp \
	       OwnMath.spf OwnMath.imp  \
	| devps > Overlay.ps


############################################
## Preprocessing part
##

## Do all the necessairy preprocessing

Preprocessing: $(POSTCPP)

##  Orca doesn't do preprocessing, so if we want to we must do this ourselves.
##  To ensure that the line numbering in the resulting files stays consistent
##  with unprocessed ones the $(CPP) output is also fed to a simple awk script
##  which uses the $(CPP) line number directives to insert blank lines where
##  $(CPP) has cut something away. To be able to handle a limited form of
##  include files as well the script may also cut something away.

$(POSTCPP): P.$$@
	@ echo "$(CPP) $(CPPFLAGS) P.$@ => $@"
	@ $(CPP) $(CPPFLAGS) P.$@                                  \
	  | awk '             BEGIN{ nr = 1 }                      \
	         /^# [0-9]+ "P.$@"/{ for( ; nr<$$2 ; nr++ ) print""\
				   ; skip = 0 ; next }             \
		       /^# [0-9] +/{ skip = 1 ; next }             \
				   { if( skip ) next }             \
				   { print ; nr++    }' >$@

PreClean:
	rm -f $(POSTCPP)
