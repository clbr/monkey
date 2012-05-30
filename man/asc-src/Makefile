.PHONY: all clean

SRC = $(wildcard *.txt)
DST = $(SRC:.txt=.o)

all: $(DST)

%.o : %.txt
	a2x -f manpage $<
	touch $@
	cp *.[0-9] ..

clean:
	rm -f *.o *.[0-9]
