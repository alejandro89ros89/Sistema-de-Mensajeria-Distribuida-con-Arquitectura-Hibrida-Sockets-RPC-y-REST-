#include <pthread.h>
#include <stdbool.h> // Para poder usar el tipo bool

//Contantes limintes 
//#define MAX_CLIENTS 10
//inicialmente diseñamos el programa con arrays estaticas, pero en clase se nos comento que tenia que ser dinamico. 
#define BUFFER_SIZE 256
#define MAX_MENSAJES_PENDIENTES 10
#define MAX_IP_SIZE 16

//Estructuras extras 
struct Mensaje_pendiente{
    unsigned int id_mensaje;
    char nombre_enviador[BUFFER_SIZE];
    char mensaje[BUFFER_SIZE];
    //Añadimos el capo para mandar el nombre del archivo 
    char fichero[BUFFER_SIZE];
};

struct UsuarioRegistrado{
    char nombre[BUFFER_SIZE];
    bool conectado;
    char ip[MAX_IP_SIZE];
    int puerto;
    struct Mensaje_pendiente mensajes_pendientes[MAX_MENSAJES_PENDIENTES];
    int num_mensajes_pendientes;

};


//Variables globales 
struct UsuarioRegistrado*  usuarios_registrados=NULL; //inicializamos una lista dinamica inicialmente vacia
int num_usuarios_registrados = 0;
int indentificador_mensaje=0; //oara saver cyak es el id_mensaje poir el que vamos

//Mutex para proteger los accesos, lo inicializamos con PTHREAD_MUTEX_INITIALIZER 
//para evitar problemas con ejecuciones de multihilo   
pthread_mutex_t mutex_usuarios = PTHREAD_MUTEX_INITIALIZER;
