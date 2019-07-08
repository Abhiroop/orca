CC=gcc
LINT=/usr/local/ack/bin/lint
TAGS=etags

CFLAGS=		-g -I$(PANDA_PROJ)/include $(V) $(DEFINES)
LINTFLAGS=-ahx -ansi
TAGSFLAGS=-t


LDLIBS=	-L$(PANDA_PROJ)/rpcwork_lib \
	-lpanda

DEPEND= $(PANDA_PROJ)/rpcwork_lib/libpanda.a\
	Makefile

DEFINES	= \
		-UDEBUG \
		-UDEBUG_3 \
		-DDEBUG_MALLOC=2 \
		-DMSG_COUNT \
		-DVERBOSE_RCVE \
		-DVERBOSE_SEND \
		-UNO_MSG_CLEAR


default:	test_1

lib::
		cd $(PANDA_PROJ)/rpcwork_lib; make

test_1:		main.o $(DEPEND)
		$(CC) $(CFLAGS) -o $@ main.o $(LDLIBS)

lint:
	$(LINT) $(LINTFLAGS) $(CFLAGS) main.c

depend:	
	makedepend $(CFLAGS) main.c





# DO NOT DELETE THIS LINE -- make depend depends on it.

main.o: /usr/include/stdio.h /usr/proj/panda/include/panda/panda.h
main.o: /usr/proj/panda/include/sys/sys.h
main.o: /usr/proj/panda/include/sys/current/threads.h
main.o: /usr/proj/panda/include/sys/sparc/pthreads/pthread.h
main.o: /usr/proj/panda/include/sys/sparc/pthreads/unistd.h
main.o: /usr/proj/panda/include/sys/sparc/pthreads/signal.h
main.o: /usr/proj/panda/include/sys/sparc/pthreads/stdtypes.h
main.o: /usr/proj/panda/include/sys/sparc/pthreads/limits.h
main.o: /usr/include/sys/time.h /usr/include/sys/time.h
main.o: /usr/proj/panda/include/sys/sparc/pthreads/errno.h
main.o: /usr/proj/panda/include/sys/current/sync.h
main.o: /usr/proj/panda/include/sys/sparc/time.h
main.o: /usr/proj/panda/include/sys/current/message.h
main.o: /usr/proj/panda/include/sys/current/pset.h
main.o: /usr/proj/panda/include/sys/current/comm.h
main.o: /usr/proj/panda/include/sys/sparc/message.h
main.o: /usr/proj/panda/include/sys/sparc/pset.h
main.o: /usr/proj/panda/include/sys/current/misc.h
main.o: /usr/proj/panda/include/sys/current/error.h /usr/include/assert.h
main.o: /usr/proj/panda/include/sys/current/malloc.h
main.o: /usr/proj/panda/include/sys/current/pool.h
main.o: /usr/proj/panda/include/sys/sparc/sync.h
main.o: /usr/proj/panda/include/sys/sparc/pool.h
main.o: /usr/proj/panda/include/sys/current/system.h
main.o: /usr/proj/panda/include/sys/current/global.h
main.o: /usr/proj/panda/include/sys/sparc/msg_pool.h
main.o: /usr/proj/panda/include/sys/sparc/ecount.h
main.o: /usr/proj/panda/include/sys/current/glocal.h
main.o: /usr/proj/panda/include/sys/current/time.h
main.o: /usr/proj/panda/include/sys/current/msg_pool.h
main.o: /usr/proj/panda/include/util/util.h
main.o: /usr/proj/panda/include/util/hash.h
main.o: /usr/proj/panda/include/util/util_init.h
main.o: /usr/proj/panda/include/util/semaphore.h
main.o: /usr/proj/panda/include/util/time.h
main.o: /usr/proj/panda/include/util/typed_msg.h
main.o: /usr/proj/panda/include/rpc/rpc.h /usr/proj/panda/include/rpc/comm.h
main.o: /usr/proj/panda/include/rpc/rpc_init.h
main.o: /usr/proj/panda/include/group/group.h
main.o: /usr/proj/panda/include/group/pan_basics.h
main.o: /usr/proj/panda/include/group/pan_grp_endian.h
main.o: /usr/proj/panda/include/group/pan_time.h
main.o: /usr/proj/panda/include/group/pan_memlist.h
main.o: /usr/proj/panda/include/group/pan_msg_buf.h
main.o: /usr/proj/panda/include/group/pan_grp_header.h
main.o: /usr/proj/panda/include/group/pan_grp_nameserver.h
main.o: /usr/proj/panda/include/group/pan_grp.h
main.o: /usr/proj/panda/include/service/service.h
main.o: /usr/proj/panda/include/service/serv.h
main.o: /usr/proj/panda/include/service/service_init.h
main.o: /usr/proj/panda/include/panda/panda_init.h
