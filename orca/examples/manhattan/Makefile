SOURCE_FILES = TourObject.imp TourObject.spf \
	TspFunctions.imp TspFunctions.spf \
	PresentationModule.imp PresentationModule.spf \
	TourModule.spf TourModule.imp \
	DistributionObject.imp DistributionObject.spf \
	Main.imp Main.spf \
	MonitorObject.imp MonitorObject.spf \
	TourCollectionObject.imp TourCollectionObject.spf \
	LogModule.spf LogModuleSrc.c \
	HashFunction.spf hash.c \
	Strings.spf orcastrings.c \
	HashTableObject.imp HashTableObject.spf \
	GenericList.imp GenericList.spf \
	TspTypes.spf CollectionTypes.spf \
	Makefile \
	TspOpt.imp TspOpt.spf
C-FILES = LogModuleSrc.c orcastrings.c hash.c

FLAGS = --O -CHK -O -v
ORCAMAIN = Main.imp

default: unix_proc



amoeba:  /tmp/mankrank.out
	ainstall /tmp/mankrank.out /home/mankrank
	touch amoeba

/tmp/mankrank.out:	$(SOURCE_FILES)
	oc_panda -amoeba -v -g -o /tmp/mankrank.out $(ORCAMAIN) $(C-FILES)

unix_proc: $(SOURCE_FILES)
	oc_unixproc $(FLAGS) -g -o $(ORCAMAIN:.imp=.unix)  $(ORCAMAIN) $(C-FILES)

debug: $(SOURCE_FILES)
	myoc_unixproc $(FLAGS) -o $(ORCAMAIN:.imp=.unix)  $(ORCAMAIN) $(C-FILES)

panda:	$(SOURCE_FILES)
	oc_panda $(FLAGS) -o $(ORCAMAIN:.imp=.panda)  $(ORCAMAIN) $(C-FILES)

clean:	
	oc_clean
