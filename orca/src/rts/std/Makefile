# (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Orca distribution.

ORCA_HOME	= /usr/proj/orca

COMP		= oc_2c OC_HOME=$(ORCA_HOME) -v

TARGETS = \
	GenericBin.gc \
	IntObject.c \
	RowCollect.c \
	GenericJob.gc \
	PollSequen.c \
	Random.c \
	barrier.c \
	distributions.c \
	Finish.h \
	Time.h \
	args.h \
	conversions.h \
	InOut.h \
	unix.h \
	trace.h \
	trace_gen.gc \
	Math.h \
	RtsStats.h \
	VTypes.h \
	Xd.h \
	Vogle.c

DBS = \
	.Finish.db \
	.InOut.db \
	.Time.db \
	.unix.db \
	.GenericBin.db \
	.IntObject.db \
	.RowCollect.db \
	.GenericJob.db \
	.distribtions.db \
	.Random.db \
	.barrier.db \
	.PollSequen.db \
	.trace.db \
	.trace_gen.db \
	.Math.db \
	.RtsStats.db \
	.VTypes.db \
	.Vogle.db \
	.Xd.db \
	.conversions.db \
	.args.db

INCLS = \
	GenericBin.gh \
	IntObject.h \
	RowCollect.h \
	Random.h \
	GenericJob.gh \
	barrier.h \
	distributions.h \
	PollSequen.h \
	trace_gen.gh \
	trace.h \
	Math.h \
	args.h \
	conversions.h \
	VTypes.h \
	Vogle.h \
	Xd.h

all:	$(TARGETS)
	/bin/rm -rf .oc_driver
	-mkdir -p $(ORCA_HOME)/man/man3
	eqn vogle.3 > $(ORCA_HOME)/man/man3/vogle.3

ALWAYS::

GenericBin.gc:	ALWAYS
	$(COMP) GenericBin.imp

trace_gen.gc:	ALWAYS
	/bin/rm -f trace_gen.gc
	$(COMP) trace_gen.spf
	/bin/cp trace_gen.src trace_gen.gc

IntObject.c:	ALWAYS
	$(COMP) IntObject.imp

RowCollect.c:	ALWAYS
	$(COMP) RowCollect.imp

GenericJob.gc:	ALWAYS
	$(COMP) GenericJob.imp

PollSequen.c:	ALWAYS
	$(COMP) PollSequen.imp

Random.c:	ALWAYS
	$(COMP) Random.imp

distributions.c:	ALWAYS
	$(COMP) distributions.imp

barrier.c:	ALWAYS
	$(COMP) barrier.imp

Vogle.c:	ALWAYS
	$(COMP) Vogle.imp

Finish.h:	ALWAYS
	$(COMP) Finish.spf
	/bin/rm -f Finish.c

InOut.h:	ALWAYS
	$(COMP) -i InOut.spf
	/bin/rm -f InOut.c

Time.h:		ALWAYS
	$(COMP) -i Time.spf
	/bin/rm -f Time.c

args.h:		ALWAYS
	$(COMP) args.spf
	/bin/rm -f args.c

conversions.h:	ALWAYS
	$(COMP) conversions.spf
	/bin/rm -f conversions.c

unix.h:		ALWAYS
	$(COMP) unix.spf
	/bin/rm -f unix.c

trace.h:	ALWAYS
	$(COMP) trace.spf
	/bin/rm -f trace.c

Math.h:		ALWAYS
	/bin/rm -f Math.h
	$(COMP) Math.spf
	cp Math_C.h Math.h
	/bin/rm -f Math.c

RtsStats.h:		ALWAYS
	/bin/rm -f RtsStats.h
	$(COMP) RtsStats.spf
	cp stats_c.h RtsStats.h
	/bin/rm -f RtsStats.c

Xd.h:		ALWAYS
	$(COMP) Xd.spf
	/bin/rm -f Xd.c

VTypes.h:	ALWAYS
	$(COMP) VTypes.spf
	/bin/rm -f VTypes.c

clean:
	/bin/rm -rf $(TARGETS) $(DBS) $(INCLS) .oc_driver
