CC = gcc
# Añadimos -I. para que encuentre registro.h en la carpeta actual
CFLAGS = -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-cast-function-type -I./Includes -I/usr/include/tirpc -I. -g
LDFLAGS = -lpthread -ltirpc

SERVER = servidor
RPC_SERVER = registro_server

# Archivos fuente
SRCS_SERVER = Servidor/Servidor.c registro_clnt.c registro_xdr.c
SRCS_RPC = registro_server.c registro_svc.c registro_xdr.c

all: $(SERVER) $(RPC_SERVER)

$(SERVER): $(SRCS_SERVER)
	$(CC) $(CFLAGS) $(SRCS_SERVER) -o $(SERVER) $(LDFLAGS)

$(RPC_SERVER): $(SRCS_RPC)
	$(CC) $(CFLAGS) $(SRCS_RPC) -o $(RPC_SERVER) $(LDFLAGS)

clean:
	rm -f $(SERVER) $(RPC_SERVER) *.o