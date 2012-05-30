.PHONY: all clean

SRC = $(wildcard *.txt)
DST = $(SRC:.txt=.o)

all: $(DST)
	cp *.[0-9] ..

%.o : %.txt
	a2x -f manpage $<
	touch $@

clean:
	rm -f *.o *.[0-9]
