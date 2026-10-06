#include "Servidor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h> // Necesario para inet_ntoa y sockaddr_in
#include <errno.h> 

#include "registro.h"


//Declaracion de funciones
void *tratar_peticion(void *arg);
ssize_t readLine(int fd, void *buffer, size_t n);
void obtener_ip(int sc, char *ip_destino); 
void registrar_cliente(int sc);
void conectar_cliente(int sc);
void unregistrar_cliente(int sc);
void enviar_mensajes(int sc);
void desconectar_cliente(int sc);
void users(int sc);
void enviar_mensajes_pendientes(char nombre_usuario[BUFFER_SIZE]);
void sendAttach(int sc);
void enviar_log_rpc(const char* usuario, const char* op);

int main(int argc, char *argv[]) {

    //Comprobamos que me han pasado el puerto 
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return EXIT_FAILURE;
    }
    //Comprobamos si me han pasado esto $ ./server -p <port>
    if (strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Uso: %s -p <puerto>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int puerto = atoi(argv[2]);
    //Este es el numero limite de piertos de ordenador 
    if (puerto <= 0 || puerto > 65535) {
        fprintf(stderr, "Puerto inválido. Debe ser un número entre 1 y 65535.\n");
        return EXIT_FAILURE;
    }
    //Sacamos la direccion IP, para el print
    char ip[INET6_ADDRSTRLEN];
    strcpy(ip, "0.0.0.0");

    printf("s> init server %s:%d\n", ip, puerto);
    printf("s> ");
    fflush(stdout); // Para forzar que se imprima en pantalla
    //Declaraciones necesartias para sockets
    struct sockaddr_in server_addr, client_addr;
    socklen_t size;
    
    //Creamos el socket , TCP que es elq ue viene por defecto 
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error al crear el socket");
        return EXIT_FAILURE;
    }
    //Para evitar errores con el socket configuramos la opcion SOI_REUSEADDR
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Error al configurar el socket");
        close(server_socket);
        return EXIT_FAILURE;
    }

    //Configuramos la direccion del servidor
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Acepta conexiones en cualquier
    server_addr.sin_port = htons(puerto);
    //bind porque es el servidor y quiero que tenga un puerto fijo 
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error al enlazar el socket");
        close(server_socket);
        return EXIT_FAILURE;
    }
    //Listen, abrirmos el socket para escuchar las conexiones entrantes
    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("Error al hacer listen");
        close(server_socket);
        return EXIT_FAILURE;
    }
    //Configuracion de hilos 
    int sc; //socket cliente
    pthread_t hilo; 
    pthread_attr_t attr_hilo;

    pthread_attr_init(&attr_hilo);
    pthread_attr_setdetachstate(&attr_hilo, PTHREAD_CREATE_DETACHED);
    while (1) {
        size = sizeof(client_addr);
        //Esto se queda bloqueado hasta que llegue una peticion, cuando llega se crea un socket nuevo para antyender la peticion
        //Esperamos a los mensajes que vienen de sd y cuando lleguen me dan un socket nuevo para esa peticion 
        sc = accept(server_socket, (struct sockaddr *)&client_addr, &size);
        if (sc == -1) {
            perror("Error al aceptar la conexion");
            close(server_socket);
            return EXIT_FAILURE;
        }  
        //Inter_ntoa: Para pasarlo de binario a IP normal 
        //ntohs: Para pasarlo de binario a numero de puerto normal


        //Pasamos el descriptor del socket mediane memoria dinamica, asi cuando volvamos a ejecutar el bucle noi tenemos problema
        int *ptr_sc = malloc(sizeof(int));
        //Tratamiendo de errores. 
        if (ptr_sc == NULL) {
            perror("Error al asignar memoria para el descriptor del socket");
            close(sc);
            continue; // No cerramos el servidor 
        }
        *ptr_sc = sc;
        if (pthread_create(&hilo, &attr_hilo, tratar_peticion, (void *)ptr_sc) != 0) {
            perror("Error al crear el hilo");
            close(sc);
            free(ptr_sc);
            continue; // No cerramos el servidor 
        }
     

    }
    close(server_socket);
    return EXIT_SUCCESS;

}

void *tratar_peticion(void *arg) {
    int sc=*((int *)arg);
    free(arg); // Liberamos la memoria que habiamos reservado para el descriptor del socket
    //leemos la primera palabra que es la operacion 
    char operacion[BUFFER_SIZE];
    ssize_t bytes_leidos = readLine(sc, operacion, sizeof(operacion));
    if (bytes_leidos <= 0) {
        perror("Error al leer la operacion");
        close(sc);
        return NULL;    
    }
    //Dependiendo de la operacion, llamamos a una funcion u otra
    if(strcmp(operacion, "REGISTER") == 0){
        registrar_cliente(sc);
    }
    else if(strcmp(operacion, "CONNECT") == 0){
        conectar_cliente(sc);
    }
    else if(strcmp(operacion, "UNREGISTER") == 0){
        unregistrar_cliente(sc);
    }
    else if(strcmp(operacion, "SEND") == 0){
        enviar_mensajes(sc);
    }
    else if(strcmp(operacion, "DISCONNECT") == 0){
        desconectar_cliente(sc);
    }
    else if (strcmp(operacion, "USERS") == 0){
        users(sc);
    }
    else if(strcmp(operacion, "SENDATTACH") == 0){
        sendAttach(sc);
    }
    else{

        char respuesta = 2; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return NULL; //No es necesartio cerrar el hio, con esto sirve
    }
    return NULL;
}

//funcion copiada tal cual de aula global 
ssize_t readLine(int fd, void *buffer, size_t n)
{
    ssize_t numRead; /* num of bytes fetched by last read() */
    size_t totRead;  /* total bytes read so far */
    char *buf;
    char ch;

    if (n <= 0 || buffer == NULL) { 
        errno = EINVAL;
        return -1;
    }

    buf = buffer;
    totRead = 0;

    for (;;) {
        numRead = recv(fd, &ch, 1, 0);

        if (numRead == -1) {
            if (errno == EINTR) /* interrupted -> restart read() */
                continue;
            else
                return -1; /* some other error */
        } else if (numRead == 0) { /* EOF */
            if (totRead == 0) /* no bytes read; return 0 */
                return 0;
            else
                break;
        } else { /* numRead must be 1 if we get here*/
            if (ch == '\n')
                break;
            if (ch == '\0') 
                break;
            if (totRead < n - 1) { /* discard > (n-1) bytes */
                totRead++;
                *buf++ = ch; 
            }
        } 
    }
    *buf = '\0';
    return totRead;
}

// Funcion para sacar una IP a traves de un sc 
void obtener_ip(int sc, char *ip_destino) {
    struct sockaddr_in client_addr;
    socklen_t size = sizeof(client_addr);
    
    if (getpeername(sc, (struct sockaddr *)&client_addr, &size) == -1) {
        perror("Error al obtener la direccion IP del cliente");
        strcpy(ip_destino, "0.0.0.0"); // Valor por defecto seguro si hay error
        return;
    }
    
    // Copiamos la IP directamente a la memoria privada de nuestro hilo para que nadie la pise
    strcpy(ip_destino, inet_ntoa(client_addr.sin_addr));
}

//Esta funcion sera llamada en connect y en send para ver si podemos enviar los mensajes al usuaroo 
void enviar_mensajes_pendientes(char nombre_usuario[BUFFER_SIZE]){
    //No es necesartio si el usuario existe ya que hemos hecho comprobaciones previas 
    //Comprobamos si el usuario esta conectado 
    for (int i=0;i<num_usuarios_registrados;i++){
        if(usuarios_registrados[i].conectado == true && strcmp(usuarios_registrados[i].nombre, nombre_usuario) == 0){
            
            //Sacamos la IP del usuario 
            char ip_destino[BUFFER_SIZE];
            strcpy(ip_destino, usuarios_registrados[i].ip); 
            
            int num_mensajes = usuarios_registrados[i].num_mensajes_pendientes; 
            
            // Dentro de tu bucle for (int j=0; j<num_mensajes; j++)
            // NOTA: Movemos la conexión del socket AQUÍ DENTRO para enviar cada mensaje 
            // en una conexión independiente, cumpliendo con el protocolo oficial.
            for (int j = 0; j < num_mensajes; j++) {
                
                //abrimso el socket de usuario de recibir mensajes
                int socket_destino = socket(AF_INET, SOCK_STREAM, 0);
                if (socket_destino == -1) {
                    perror("Error al crear el socket de destino");
                    continue; // Pasamos al siguiente mensaje si este falla
                }
                
                struct sockaddr_in server_addr;
                memset(&server_addr, 0, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                server_addr.sin_addr.s_addr = inet_addr(ip_destino);
                server_addr.sin_port = htons(usuarios_registrados[i].puerto);
                
                //Convertimos la IP en formato binario 
                if (inet_pton(AF_INET, ip_destino, &server_addr.sin_addr) <= 0) {
                    perror("Error al convertir la IP en formato binario");
                    close(socket_destino);
                    continue; 
                }
                
                //Conectamos el socket de recibir mensajes con el de enviar mensajes 
                if (connect(socket_destino, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
                    perror("Error al conectar los sockets");
                    close(socket_destino);
                    continue;
                }
                printf("SEND MESSAGE %u FROM %s TO %s\n", 
                       usuarios_registrados[i].mensajes_pendientes[j].id_mensaje, 
                       usuarios_registrados[i].mensajes_pendientes[j].nombre_enviador, 
                       usuarios_registrados[i].nombre);
                printf("s> ");
                fflush(stdout);
       
                //ahora que ya lo tenemos abierto a la perfeccion lo que hacemos es mandar el mensake
                char buffer_aux[BUFFER_SIZE];

                // Comprobamos si el mensaje tiene un fichero adjunto leyendo su longitud
                if (strlen(usuarios_registrados[i].mensajes_pendientes[j].fichero) > 0) {
                    
                    // 1. Enviamos la operación de fichero adjunto
                    snprintf(buffer_aux, sizeof(buffer_aux), "SEND_MESSAGE_ATTACH");
                    send(socket_destino, buffer_aux, strlen(buffer_aux) + 1, 0);
                    
                    // 2. Enviamos el remitente (nombre_enviador)
                    snprintf(buffer_aux, sizeof(buffer_aux), "%s", usuarios_registrados[i].mensajes_pendientes[j].nombre_enviador);
                    send(socket_destino, buffer_aux, strlen(buffer_aux) + 1, 0);

                    // 3. Enviamos el ID (convertido a texto)
                    snprintf(buffer_aux, sizeof(buffer_aux), "%u", usuarios_registrados[i].mensajes_pendientes[j].id_mensaje);
                    send(socket_destino, buffer_aux, strlen(buffer_aux) + 1, 0);

                    // 4. Enviamos el contenido del mensaje
                    snprintf(buffer_aux, sizeof(buffer_aux), "%s", usuarios_registrados[i].mensajes_pendientes[j].mensaje);
                    send(socket_destino, buffer_aux, strlen(buffer_aux) + 1, 0);
                    
                    // 5. Enviamos el nombre del fichero
                    snprintf(buffer_aux, sizeof(buffer_aux), "%s", usuarios_registrados[i].mensajes_pendientes[j].fichero);
                    send(socket_destino, buffer_aux, strlen(buffer_aux) + 1, 0);

                } else {
                    
                    // 1. Enviamos la operación normal
                    snprintf(buffer_aux, sizeof(buffer_aux), "SEND_MESSAGE");
                    send(socket_destino, buffer_aux, strlen(buffer_aux) + 1, 0);
                    
                    // 2. Enviamos el remitente (nombre_enviador)
                    snprintf(buffer_aux, sizeof(buffer_aux), "%s", usuarios_registrados[i].mensajes_pendientes[j].nombre_enviador);
                    send(socket_destino, buffer_aux, strlen(buffer_aux) + 1, 0);

                    // 3. Enviamos el ID (convertido a texto)
                    snprintf(buffer_aux, sizeof(buffer_aux), "%u", usuarios_registrados[i].mensajes_pendientes[j].id_mensaje);
                    send(socket_destino, buffer_aux, strlen(buffer_aux) + 1, 0);

                    // 4. Enviamos el contenido del mensaje
                    snprintf(buffer_aux, sizeof(buffer_aux), "%s", usuarios_registrados[i].mensajes_pendientes[j].mensaje);
                    send(socket_destino, buffer_aux, strlen(buffer_aux) + 1, 0);
                }

                //Cerramos el socket de este mensaje específico
                close(socket_destino);

                // =================================================================
                // AÑADIR A PARTIR DE AQUÍ: ENVIAR ACK (CONFIRMACIÓN) AL REMITENTE
                // =================================================================
                char *nombre_rem = usuarios_registrados[i].mensajes_pendientes[j].nombre_enviador;
                unsigned int id_msg = usuarios_registrados[i].mensajes_pendientes[j].id_mensaje;
                char *fichero_adj = usuarios_registrados[i].mensajes_pendientes[j].fichero;

                // Buscamos al remitente en la lista de usuarios para ver si está conectado
                for (int k = 0; k < num_usuarios_registrados; k++) {
                    if (strcmp(usuarios_registrados[k].nombre, nombre_rem) == 0 && usuarios_registrados[k].conectado == true) {
                        
                        // Si está conectado, le abrimos un socket para avisarle
                        int socket_ack = socket(AF_INET, SOCK_STREAM, 0);
                        if (socket_ack != -1) {
                            struct sockaddr_in ack_addr;
                            memset(&ack_addr, 0, sizeof(ack_addr));
                            ack_addr.sin_family = AF_INET;
                            ack_addr.sin_addr.s_addr = inet_addr(usuarios_registrados[k].ip);
                            ack_addr.sin_port = htons(usuarios_registrados[k].puerto);

                            if (connect(socket_ack, (struct sockaddr *)&ack_addr, sizeof(ack_addr)) != -1) {
                                char buffer_ack[BUFFER_SIZE];

                                if (strlen(fichero_adj) > 0) {
                                    // Confirmación para mensaje con fichero (Parte 2)
                                    snprintf(buffer_ack, sizeof(buffer_ack), "SEND_MESS_ATTACH_ACK");
                                    send(socket_ack, buffer_ack, strlen(buffer_ack) + 1, 0);

                                    snprintf(buffer_ack, sizeof(buffer_ack), "%u", id_msg);
                                    send(socket_ack, buffer_ack, strlen(buffer_ack) + 1, 0);

                                    snprintf(buffer_ack, sizeof(buffer_ack), "%s", fichero_adj);
                                    send(socket_ack, buffer_ack, strlen(buffer_ack) + 1, 0);
                                } else {
                                    // Confirmación para mensaje normal (Parte 1)
                                    snprintf(buffer_ack, sizeof(buffer_ack), "SEND_MESS_ACK");
                                    send(socket_ack, buffer_ack, strlen(buffer_ack) + 1, 0);

                                    snprintf(buffer_ack, sizeof(buffer_ack), "%u", id_msg);
                                    send(socket_ack, buffer_ack, strlen(buffer_ack) + 1, 0);
                                }
                            }
                            close(socket_ack);
                        }
                        break; // Ya encontramos al remitente y le avisamos, salimos del bucle k
                    }
                }
            }

            // Al terminar el bucle, limpiamos el buzón
            usuarios_registrados[i].num_mensajes_pendientes = 0;
            return;
        }
    }
}

//Funciones propias del servidor 
void registrar_cliente(int sc){
    char nombre_cliente[BUFFER_SIZE];
    if(readLine(sc, nombre_cliente, sizeof(nombre_cliente)) <= 0){
        perror("Error al leer el nombre del cliente");
        char respuesta=2; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return ;
    }
    //Comprobamos que el nombre no este vacio
    if(strlen(nombre_cliente) == 0){
        char respuesta=2; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return ; 
    }

    enviar_log_rpc(nombre_cliente, "REGISTER");

    //Compoprbamos si el nombre esta repetido  
    pthread_mutex_lock(&mutex_usuarios);
    for (int i=0 ; i<num_usuarios_registrados;i++){ 
        if(strcmp(usuarios_registrados[i].nombre, nombre_cliente) == 0){
            pthread_mutex_unlock(&mutex_usuarios);
            char respuesta=1;
            printf("REGISTER %s FAIL\n", nombre_cliente);
            printf("s> ");
            fflush(stdout);
            send(sc, &respuesta, 1, 0);
            close(sc);
            return ;  
            
        }     
    }
    //ampliamos la memoria en 1 
    usuarios_registrados=realloc(usuarios_registrados, (num_usuarios_registrados +1)* sizeof(struct UsuarioRegistrado));
    if (usuarios_registrados == NULL) {
        perror("Error al asignar memoria");
        pthread_mutex_unlock(&mutex_usuarios);
        char respuesta = 2; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return;
    }
    //Como no esta reoetudi lo qu ehacemos es añadirlo rellandno los parametros 
    strcpy(usuarios_registrados[num_usuarios_registrados].nombre, nombre_cliente);
    usuarios_registrados[num_usuarios_registrados].conectado = false;
    //Sacamos la IP del usuario 
    //char ip[INET6_ADDRSTRLEN];
    obtener_ip(sc, usuarios_registrados[num_usuarios_registrados].ip);
    //Sacamos el puerto del usuario

    struct sockaddr_in addr_aux;
    socklen_t addr_len = sizeof(addr_aux);
    getsockname(sc, (struct sockaddr *)&addr_aux, &addr_len);
    usuarios_registrados[num_usuarios_registrados].puerto = ntohs(addr_aux.sin_port);

    memset(usuarios_registrados[num_usuarios_registrados].mensajes_pendientes, 0, sizeof(usuarios_registrados[num_usuarios_registrados].mensajes_pendientes));
    usuarios_registrados[num_usuarios_registrados].num_mensajes_pendientes = 0;

    num_usuarios_registrados++;
    pthread_mutex_unlock(&mutex_usuarios);
    char respuesta=0; 
    printf("REGISTER %s OK\n", nombre_cliente);
    printf("s> ");
    fflush(stdout);
    send(sc, &respuesta, 1, 0);
    close(sc);
    
}

void unregistrar_cliente(int sc){

    //leemos el nombre que nos pasa por parametro
    char nombre_cliente[BUFFER_SIZE];
    readLine(sc, nombre_cliente, sizeof(nombre_cliente));
    //Comprobamos que el nombre no este vacio
    if(strlen(nombre_cliente) == 0){
        char respuesta=2; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return ; 
    }

    enviar_log_rpc(nombre_cliente, "UNREGISTER");

    pthread_mutex_lock(&mutex_usuarios);

    //Comprpobamos que el nombre exista en la base de datos 
    for (int i=0;i<num_usuarios_registrados;i++){
        if(strcmp(usuarios_registrados[i].nombre, nombre_cliente) == 0){

            //Si lo encuentra lo borramos
            //Copiamos el elemento final al que vamos a borrar 
            usuarios_registrados[i] = usuarios_registrados[num_usuarios_registrados - 1];
            // 2. Reducimos el contador total
            num_usuarios_registrados--;
            //El elemento quie hemos movido no lo dejamos vacio en su antigua posicion 
            //pero en la parte de registarr lo limiamos todo 

            //como trabajamos con memoria dinamica 
            if (num_usuarios_registrados > 0) {
                usuarios_registrados= realloc(usuarios_registrados, num_usuarios_registrados * sizeof(struct UsuarioRegistrado));
                if (usuarios_registrados == NULL) {
                    perror("Error al reasignar memoria");
                    pthread_mutex_unlock(&mutex_usuarios);
                    char respuesta = 2; 
                    send(sc, &respuesta, 1, 0);
                    close(sc);
                    return;
                }
            }
            if (num_usuarios_registrados == 0) {
                free(usuarios_registrados);
                usuarios_registrados = NULL;
            }
        

            char respuesta=0; 
            printf("UNREGISTER %s OK\n", nombre_cliente);
            printf("s> ");
            fflush(stdout);
            send(sc, &respuesta, 1, 0);
            close(sc);
            pthread_mutex_unlock(&mutex_usuarios);
            return ;  
        }
    }
    //Si no existe en la base de datos 
    pthread_mutex_unlock(&mutex_usuarios);
    char respuesta=1; 
    printf("UNREGISTER %s FAIL\n", nombre_cliente);
    printf("s> ");
    fflush(stdout);
    send(sc, &respuesta, 1, 0);
    close(sc);
  
}

void conectar_cliente(int sc){
    //leemos el nombre que nos pasa por parametro
    char nombre_cliente[BUFFER_SIZE];
    readLine(sc, nombre_cliente, sizeof(nombre_cliente));
    enviar_log_rpc(nombre_cliente, "CONNECT");
    pthread_mutex_lock(&mutex_usuarios);
    //Comprobamos que el nombre no este vacio
    if(strlen(nombre_cliente) == 0){
        char respuesta=3;
        send(sc, &respuesta, 1, 0);
        close(sc);
        pthread_mutex_unlock(&mutex_usuarios); // Añadido unlock antes de salir
        return ; 
    }
    //Vamos a leer el puerto de escucha para los mensajes 
    char puerto[BUFFER_SIZE];
    readLine(sc, puerto, sizeof(puerto));
    int port = atoi(puerto);
    //comprobamso que el usuario no este conectado o si lo esta 
    for (int i=0;i<num_usuarios_registrados;i++){
        if(usuarios_registrados[i].conectado == true && strcmp(usuarios_registrados[i].nombre, nombre_cliente) == 0){
            char respuesta=2; 
            printf("CONNECT %s FAIL\n", nombre_cliente);
            printf("s> ");
            fflush(stdout);
            send(sc, &respuesta, 1, 0);
            close(sc);
            pthread_mutex_unlock(&mutex_usuarios);
            return ; 
        }
        if (usuarios_registrados[i].conectado == false && strcmp(usuarios_registrados[i].nombre, nombre_cliente) == 0){
            //Si lo encuentra lo marcamos como conectado 
            obtener_ip(sc, usuarios_registrados[i].ip);
            usuarios_registrados[i].puerto = port;  
            usuarios_registrados[i].conectado = true;

            // 1. Primero confirmamos la conexión
            char respuesta=0; 
            printf("CONNECT %s OK\n", nombre_cliente);
            printf("s> ");
            fflush(stdout);
            send(sc, &respuesta, 1, 0);

            // 2. AHORA enviamos los mensajes acumulados
            enviar_mensajes_pendientes(usuarios_registrados[i].nombre);

            close(sc);
            pthread_mutex_unlock(&mutex_usuarios);
            return ;  
        }
    }
    //Si hay llegado hasta aqui el cdoigo es que no existe en la base de datos 
    char respuesta=1; 
    printf("CONNECT %s FAIL\n", nombre_cliente);
    printf("s> ");
    fflush(stdout);
    send(sc, &respuesta, 1, 0);
    close(sc);
    pthread_mutex_unlock(&mutex_usuarios);
}

void desconectar_cliente(int sc){
    //leemos el nombre que nos pasa por parametros 
    char nombre_cliente[BUFFER_SIZE];
    if(readLine(sc, nombre_cliente, sizeof(nombre_cliente)) <= 0){
        char respuesta=3; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return ; 
    }
    //Comprobamos que el nombre no este vacio
    if(strlen(nombre_cliente) == 0){
        char respuesta=3; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return ; 
    }

    enviar_log_rpc(nombre_cliente, "DISCONNECT");

    //comprobamos que el usuario existe en la base de datos 
    pthread_mutex_lock(&mutex_usuarios);
    for (int i=0;i<num_usuarios_registrados;i++){
        
        if(strcmp(usuarios_registrados[i].nombre, nombre_cliente) == 0 && usuarios_registrados[i].conectado == true){
            //Si lo encuentra lo marcamos como desconectado 
            usuarios_registrados[i].conectado = false;
            char respuesta=0; 
            printf("DISCONNECT %s OK\n", nombre_cliente);
            printf("s> ");
            fflush(stdout);
            send(sc, &respuesta, 1, 0);
            close(sc);
            pthread_mutex_unlock(&mutex_usuarios);
            return ;  
        }
        if(strcmp(usuarios_registrados[i].nombre, nombre_cliente) == 0 && usuarios_registrados[i].conectado == false){
            //El usuario ya estaba desconectado 
            char respuesta=2;
            printf("DISCONNECT %s FAIL\n", nombre_cliente);
            printf("s> ");
            fflush(stdout);
            send(sc, &respuesta, 1, 0);
            close(sc);
            pthread_mutex_unlock(&mutex_usuarios);
            return ;
        }

    }
    //Si hemosllegado aqui es porque el usuario no esta registrado en la base de datos 
    char respuesta=1; 
    printf("DISCONNECT %s FAIL\n", nombre_cliente);
    printf("s> ");
    fflush(stdout);
    send(sc, &respuesta, 1, 0);
    close(sc);
    pthread_mutex_unlock(&mutex_usuarios);

}

void enviar_mensajes(int sc){
    char nombre_emisor[BUFFER_SIZE];
    char nombre_usuario[BUFFER_SIZE];
    char mensaje[BUFFER_SIZE];
    
    // 1. Leemos el nombre del emisor
    if(readLine(sc, nombre_emisor, sizeof(nombre_emisor)) <= 0){
        char respuesta=3; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return ; 
    }

    enviar_log_rpc(nombre_emisor, "SEND");

    // 2. Leemos el destinatario 
    if (readLine(sc, nombre_usuario, sizeof(nombre_usuario)) <= 0){
        char respuesta=2; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return ; 
    }
    // Comprobamos que el nombre no esté vacío
    if(strlen(nombre_usuario) == 0){
        char respuesta=3; 
        send(sc, &respuesta, 1, 0); 
        close(sc);
        return ; 
    }
    // 3. Leemos el mensaje 
    if(readLine(sc, mensaje, sizeof(mensaje)) <= 0){
        char respuesta=3; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return ; 
    }

    // Comprobamos si el usuario existe en la base de datos 
    pthread_mutex_lock(&mutex_usuarios);

    for (int i=0; i<num_usuarios_registrados; i++){
        if(strcmp(usuarios_registrados[i].nombre, nombre_usuario) == 0 ){ 
            
            // Verificamos si el receptor tiene espacio en su buzón
            if(usuarios_registrados[i].num_mensajes_pendientes < MAX_MENSAJES_PENDIENTES){
                
                indentificador_mensaje++; 
                unsigned int id_asignado = indentificador_mensaje;

                // Rellenamos el buzon del receptor con los datos del mensaje
                int pos = usuarios_registrados[i].num_mensajes_pendientes;
                strcpy(usuarios_registrados[i].mensajes_pendientes[pos].nombre_enviador, nombre_emisor);
                strcpy(usuarios_registrados[i].mensajes_pendientes[pos].mensaje, mensaje);
                usuarios_registrados[i].mensajes_pendientes[pos].id_mensaje = id_asignado;
                //Limpiamos la memoria del fichero
                strcpy(usuarios_registrados[i].mensajes_pendientes[pos].fichero, "");
                // Incrementamos el num de mensajes pendientes 
                usuarios_registrados[i].num_mensajes_pendientes++;
                if (usuarios_registrados[i].conectado == false) {
                    printf("MESSAGE %u FROM %s TO %s STORED\n", id_asignado, nombre_emisor, nombre_usuario);
                    printf("s> ");
                    fflush(stdout);
                }
                // Intentamos enviar los mensajes pendientes inmediatamente
                // Si el usuario está conectado, se le entregarán ahora mismo
                enviar_mensajes_pendientes(nombre_usuario);

                // Respondemos al emisor con éxito (0)
                char respuesta=0; 
                send(sc, &respuesta, 1, 0);
                
                // Enviamos el ID asignado al cliente emisor
                char id_str[32];
                sprintf(id_str, "%u", id_asignado);
                send(sc, id_str, strlen(id_str) + 1, 0); 
                
                close(sc);
                pthread_mutex_unlock(&mutex_usuarios);
                return ;
            }
            else{
                // Error: Buzón lleno
                char respuesta=2; 
                send(sc, &respuesta, 1, 0);
                close(sc);
                pthread_mutex_unlock(&mutex_usuarios);
                return ;
            }
        }
    } 

    // Si el usuario no existe en la base de datos 
    char respuesta=1; 
    send(sc, &respuesta, 1, 0);
    close(sc);
    pthread_mutex_unlock(&mutex_usuarios);
    return ; 
}

void users(int sc) {
    char nombre_solicitante[BUFFER_SIZE];
    
    // Leemos el nombre del usuario que hace la petición
    if (readLine(sc, nombre_solicitante, sizeof(nombre_solicitante)) <= 0) {
        char respuesta = 1;
        printf("CONNECTEDUSERS FAIL\n");
        printf("s> ");
        fflush(stdout);
        send(sc, &respuesta, 1, 0);
        close(sc);
        return;
    }

    enviar_log_rpc(nombre_solicitante, "USERS");

    pthread_mutex_lock(&mutex_usuarios);
    
    int solicitante_conectado = 0;
    for (int i = 0; i < num_usuarios_registrados; i++) {
        if (strcmp(usuarios_registrados[i].nombre, nombre_solicitante) == 0 && usuarios_registrados[i].conectado == true) {
            solicitante_conectado = 1;
            break;
        }
    }
    
    // Si no está conectado, devolvemos error 1 y abortamos
    if (solicitante_conectado == 0) {
        char respuesta = 1;
        send(sc, &respuesta, 1, 0);
        pthread_mutex_unlock(&mutex_usuarios);
        close(sc);
        return;
    }
    // voantamos  cuantos usuarios hay conectados 
    int conectados = 0;
    for (int i = 0; i < num_usuarios_registrados; i++) {
        if (usuarios_registrados[i].conectado) {
            conectados++;
        }
    }

    //Si henos legado aqui ga funcioando 
    char respuesta = 0;
    printf("CONNECTEDUSERS OK\n");
    printf("s> ");
    fflush(stdout);
    send(sc, &respuesta, 1, 0);

    // Enviamos el numero de conectados como cadena
    char num_str[16];
    sprintf(num_str, "%d", conectados);
    send(sc, num_str, strlen(num_str) + 1, 0);

    // Enviamos la lista de nombres de los usuarios conectados
    for (int i = 0; i < num_usuarios_registrados; i++) {
        if (usuarios_registrados[i].conectado) {
            //Como me puiden que se mande en una cadena cirecta haremos:
            // +50 para los caracteres de formato extra y evitar warnings de truncamiento (GCC) 
            char info_usuario[BUFFER_SIZE + 50];
        // Formateamos todo en una sola cadena como pide la practica: "usuario:IP:puerto"
            snprintf(info_usuario, sizeof(info_usuario), "%s:%s:%d", 
                 usuarios_registrados[i].nombre, 
                 usuarios_registrados[i].ip, 
                 usuarios_registrados[i].puerto);
        
        send(sc, info_usuario, strlen(info_usuario) + 1, 0);
        }
    }

    pthread_mutex_unlock(&mutex_usuarios);
    close(sc);
}
//Funcion de la parte 2
void sendAttach(int sc) {
    //leemos nombre del usuario 
    char nombre_usuario_remitente[BUFFER_SIZE];
    if(readLine(sc, nombre_usuario_remitente, sizeof(nombre_usuario_remitente)) <= 0) {
        char respuesta=1; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return  ; 
    }
    char nombre_usuario_destinatario[BUFFER_SIZE];
    if(readLine(sc, nombre_usuario_destinatario, sizeof(nombre_usuario_destinatario)) <= 0) {
        char respuesta=1; 
        send(sc, &respuesta, 1, 0);
        close(sc);
        return ;    
    }
    char mensajes_adjunto[BUFFER_SIZE];
    if(readLine(sc, mensajes_adjunto, sizeof(mensajes_adjunto)) <= 0) {
        char respuesta=1; 
        send(sc, &respuesta, 1, 0);         
        close(sc);
        return ;
    }
    char nombre_fichero[BUFFER_SIZE];
    if(readLine(sc, nombre_fichero, sizeof(nombre_fichero)) <=
        0) {
            char respuesta=1; 
            send(sc, &respuesta, 1, 0);         
            close(sc);
            return ;
        }

    char buffer_log[BUFFER_SIZE + 50];
    snprintf(buffer_log, sizeof(buffer_log), "SENDATTACH %s", nombre_fichero);
    enviar_log_rpc(nombre_usuario_remitente, buffer_log);

    //Hemos llegado aqui por lo tanto 
    // 1. Bloqueamos el mutex porque vamos a leer y escribir en la lista de usuarios
    pthread_mutex_lock(&mutex_usuarios);

    // 2. Buscamos al usuario destinatario en nuestra lista
    int pos_destinatario = -1;
    for (int i = 0; i < num_usuarios_registrados; i++) {
        if (strcmp(usuarios_registrados[i].nombre, nombre_usuario_destinatario) == 0) {
            pos_destinatario = i;
            break;
        }
    }

    // Si el usuario no existe, devolvemos error 1
    if (pos_destinatario == -1) {
        char respuesta = 1;
        send(sc, &respuesta, 1, 0);
        close(sc);
        pthread_mutex_unlock(&mutex_usuarios);
        return;
    }

    // 3. Verificamos si el receptor tiene espacio en su buzón
    if (usuarios_registrados[pos_destinatario].num_mensajes_pendientes >= MAX_MENSAJES_PENDIENTES) {
        char respuesta = 2; //Buzzon lleno
        send(sc, &respuesta, 1, 0);
        close(sc);
        pthread_mutex_unlock(&mutex_usuarios);
        return;
    }

    // 4. Generamos el ID del mensaje
    indentificador_mensaje++;
    int id_asignado = indentificador_mensaje;

    // 5. Rellenamos el buzón del receptor con TODOS los datos (incluido el fichero)
    int pos_mensaje = usuarios_registrados[pos_destinatario].num_mensajes_pendientes;
    
    strcpy(usuarios_registrados[pos_destinatario].mensajes_pendientes[pos_mensaje].nombre_enviador, nombre_usuario_remitente);
    strcpy(usuarios_registrados[pos_destinatario].mensajes_pendientes[pos_mensaje].mensaje, mensajes_adjunto);
    strcpy(usuarios_registrados[pos_destinatario].mensajes_pendientes[pos_mensaje].fichero, nombre_fichero); 
    usuarios_registrados[pos_destinatario].mensajes_pendientes[pos_mensaje].id_mensaje = id_asignado;
    
    // Indicamos que ahora tiene un mensaje nuevo esperando 
    usuarios_registrados[pos_destinatario].num_mensajes_pendientes++;
    if (usuarios_registrados[pos_destinatario].conectado == false) {
        printf("MESSAGE %d FROM %s TO %s STORED\n", id_asignado, nombre_usuario_remitente, nombre_usuario_destinatario);
        printf("s> ");
        fflush(stdout);
    }
    // 6. Respondemos al emisor con exito (0)
    char respuesta = 0;
    send(sc, &respuesta, 1, 0);

    // 7. Le enviamos el ID que le hemos asignado
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", id_asignado);
    send(sc, id_str, strlen(id_str) + 1, 0);

    // Cerramos el socket 
    close(sc);

    // Intentamos enviar los mensajes pendientes inmediatamente
    enviar_mensajes_pendientes(nombre_usuario_destinatario);

    //Liberamos el mutex 
    pthread_mutex_unlock(&mutex_usuarios);
}


void enviar_log_rpc(const char* usuario, const char* op) {
    // 1. Leemos la IP de la variable de entorno obligatoria
    char* ip_rpc = getenv("LOG_RPC_IP");
    if (ip_rpc == NULL) {
        return; 
    }

    // 2. Creamos el "mango" (handle) del cliente RPC
    CLIENT *clnt = clnt_create(ip_rpc, REGISTRO_PROG, REGISTRO_VERS, "tcp");
    if (clnt == NULL) return;

    // 3. Preparamos la estructura con los datos
    DatosOperacion datos;
    datos.nombre_usuario = (char*) usuario;
    datos.operacion = (char*) op;
    
    int result; // Para guardar el resultado de la llamada

    // 4. Llamada remota (estilo -N -M)
    enum clnt_stat status = imprimir_log_1(datos, &result, clnt);
    
    if (status != RPC_SUCCESS) {
        clnt_perror(clnt, "Fallo en la llamada RPC");
    }

    // 5. Limpiamos
    clnt_destroy(clnt);
}