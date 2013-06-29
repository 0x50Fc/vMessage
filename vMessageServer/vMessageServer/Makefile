
CORE = ../../c/core
EVENT = ../../c/event
STREAM = ../../c/stream
SERVER = ../../c/server

LIBRARY = -L/usr/lib -lpthread -lz
INCLUDE = -I$(CORE) -I$(EVENT) -I$(STREAM) -I$(SERVER)
CFLAGS = -D__LINUX__ -DHLOG_CONSOLE



EXECNAME = vMessageServer
OUTPUT = bin
TARGET = $(OUTPUT)/$(EXECNAME)

CORE_C      = $(CORE)/hinifile.c $(CORE)/hserial_list.c $(CORE)/hext_obj.c $(CORE)/hbase64.c $(CORE)/hfile.c \
                 $(CORE)/hbuffer.c $(CORE)/htypes.c $(CORE)/md5.c $(CORE)/hbytes.c $(CORE)/hstr.c
EVENT_C     = $(EVENT)/hevent.c
STREAM_C    = $(STREAM)/hstream.c
SERVER_C    = $(SERVER)/hserver.c

MSG_C       = MSG.c MSGServerProcess.c MSGAuth.c MSGDatabase.c main.c

$(TARGET):
	cc $(CFLAGS) $(INCLUDE) $(CORE_C) $(EVENT_C) $(STREAM_C) $(SERVER_C) $(MSG_C) -o $(TARGET) $(LIBRARY)
 
clean:
	rm -rf $(OBJOUTPUT)/*.o $(TARGET) 

install:
	cp $(TARGET) /usr/bin/$(EXECNAME)

uninstall:
	rm -f /usr/bin/$(EXECNAME)


