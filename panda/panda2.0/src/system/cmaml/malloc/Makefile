CC     = gcc
CFLAGS = -O

# the dickmalloc module
CSRC =	mal.c log.c phys.c global.c
COBJ =	mal.o log.o phys.o global.o

OBJ = . #default


SRC	= malloc.c
OBJECTS = $(OBJ)/malloc.o


default: $(OBJECTS)

$(OBJ)/malloc.o: malloc.c
	$(CC) $(CFLAGS) -c -o $@ malloc.c

size_type.h:	getsize
		getsize > size_type.h

getsize:	getsize.c
		$(CC) -o getsize getsize.c

malloc.c:	READ_ME param.h impl.h check.h log.h phys.h \
		mal.c log.c phys.c check.c Makefile add_file
	cat READ_ME >malloc.c
	echo "#ifndef STANDARD_MALLOC" >>malloc.c
	add_file size_type.h >>malloc.c
	add_file param.h >>malloc.c
	add_file impl.h >>malloc.c
	add_file check.h >>malloc.c
	add_file log.h >>malloc.c
	add_file phys.h >>malloc.c
	add_file mal.c >>malloc.c
	add_file log.c >>malloc.c
	add_file phys.c >>malloc.c
	add_file check.c >>malloc.c
	echo "#endif /* !STANDARD_MALLOC*/" >>malloc.c
	mv malloc.c ../malloc.c

lint:
	lint -DASSERT -DCHECKSUM -DPRINT_COUNT $(SRC)

lxref:	$(OBJ)
	@lxref $(OBJ)

cchk:
	cchk $(SRC)

Files:	Makefile
	ls READ_ME ChangeLog Makefile *.h $(SRC) >Files

print:	Files
	@print `cat Files`

clean:
	rm -f *.o core mal.out mon.out malloc.c tst \
		malloc dickmalloc unixmalloc
		
#------------------------------------------------------------
check.o: check.h impl.h log.h param.h phys.h size_type.h
global.o: impl.h param.h size_type.h
log.o: check.h impl.h log.h param.h size_type.h
mal.o: check.h impl.h log.h param.h phys.h size_type.h
phys.o: check.h impl.h param.h phys.h size_type.h





