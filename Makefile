CLIENT_APP = client
SERVER_APP = server
REPLICA_APP = replicas
PROGS = $(CLIENT_APP) $(SERVER_APP) $(REPLICA_APP)
CC=g++
DEPEND=client.o server.o replicas.o
CFLAGS = -lnsl -lpthread -w -D TEST -pthread #-lsocket 



all: client server replicas

#$(DEPEND): %.o: %.c
#	$(CC) -c -o $@ $< 
	#$(CC) -c -o @< @
client.o: client.cpp protocol.h 
	$(CC) $(CFLAGS) -c -o client.o  client.cpp

server.o: server.cpp protocol.h servers_protocol.h
	$(CC) $(CFLAGS) -c -o server.o  server.cpp

replicas.o: replicas.cpp protocol.h servers_protocol.h
	$(CC) $(CFLAGS) -c -o replicas.o  replicas.cpp

client: client.o 
	$(CC) $(CFLAGS) client.o -o  $(CLIENT_APP)

server: server.o 
	$(CC) $(CFLAGS) server.o -o  $(SERVER_APP)

$(REPLICA_APP): replicas.o 
	$(CC) $(CFLAGS) replicas.o -o  $(REPLICA_APP)

run:
	./$(SERVER_APP)


clean:
	rm -f *.o
	rm -f $(PROGS)
