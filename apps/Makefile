EXEC = main
TGTDIR = ..

CFLAGS += -I ./ -I../eventsCtl -I../common -g 
#CFLAGS += -I ./ -I../eventsCtl -I../common -g -D HAVE_CACHE_DISK_STORE
#CFLAGS += -I ./ -I../eventsCtl -I../common -g -D DNSPARSE_BLOCK

LDFLAGS += -L ./  -L../eventsCtl -leventctl -L../common -lcomm -lrt

#LDFLAGS += -shared -L ./ \
	   -lpthread

include $(TGTDIR)/Makefile.inc
