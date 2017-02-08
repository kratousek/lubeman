TOPDIR  := $(shell cd .; pwd)
include $(TOPDIR)/Rules.make

APP = simple

all: $(APP)

$(APP): main.o comutils.o engine.o parser.o tcppes.o	
	$(CC) main.o comutils.o engine.o parser.o tcppes.o -o $(APP) $(CFLAGS)

main.o: main.c
	$(CC) main.c -c $(CXFLAGS) 

comutils.o: comutils.c	
	$(CC) comutils.c -c $(CXFLAGS) 

engine.o: engine.c
	$(CC) engine.c -c $(CXFLAGS)

parser.o: parser.c
	$(CC) parser.c -c $(CXFLAGS)

tcppes.o: tcppes.c
	$(CC) tcppes.c -c $(CXFLAGS)

clean:
	rm -f *.o ; rm $(APP)
