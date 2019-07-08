CC		= gcc

CFLAGS		+= -ansi
CFLAGS		+= -O2
CFLAGS		+= -s
#CFLAGS		+= -g

LDFLAGS		+= -lm

OUT		= boxMaker

LIBS		+= Box.o 
LIBS		+= Bitmap.o 
LIBS		+= main.o


$(OUT): $(LIBS)
	$(CC) $(CFLAGS)  $(LIBS)  $(LDFLAGS)  -o $(OUT) 


Bitmap		= Bitmap.h
Boolean		= Boolean.h
Box		= Box.h


main.o:  makefile  $(Bitmap)  $(Boolean)  $(Box)  main.c

Box.o:  makefile  $(Box)

Bitmap.o:  makefile  $(Bitmap) $(Boolean)


less:
	rm -f  $(LIBS)  $(OUT).ps


clean:
	rm -f  $(OUT)  $(LIBS)  $(OUT).ps


print:
	pr main.c Bitmap.h Bitmap.c Boolean.h Box.h Box.c | lpr -J"$(OUT)"


vgrind:
	vgrind -lc main.c Bitmap.h Bitmap.c Boolean.h Box.h Box.c \
	| devps > $(OUT).ps

