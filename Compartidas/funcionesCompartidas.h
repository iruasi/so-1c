#ifndef FUNCIONESCOMPARTIDAS_H_
#define FUNCIONESCOMPARTIDAS_H_

#include <sys/select.h>
#include <sys/types.h>
/* Medida de seguridad. No solo hace free(pointer) sino que reasigna el pointer a NULL
 */
void freeAndNULL(void *pointer);

/* Retorna el tamanio de un archivo
 */
unsigned long fsize(FILE* f);

/* Recibe una estructura que almacena informacion del propio host;
 * La inicializa con valores utiles, pasados por parametro
 */
void setupHints(struct addrinfo *, int, int, int);

/* Dados un ip y puerto de destino, se crea, conecta y retorna socket apto para comunicacion
 * La deberia utilizar unicamente Iniciar_Programa, por cada nuevo hilo para un script que se crea
 */
int establecerConexion(char *, char *);

/* crea un socket y lo bindea() a un puerto particular,
 * luego retorna este socket, apto para listen()
 */
int makeListenSock(char *);

/* acepta una conexion entrante, y crea un socket para comunicaciones regulares;
 */
int makeCommSock(int);

/* Limpia un buffer usado por cada proceso para emitir varios mensajes
 */
void clearBuffer(char *, int );

/* Atiende una conexion entrante, la agrega al set de relevancia, y vuelve a escuhar mas conexiones;
 * retorna el nuevo socket producido
 */
int handleNewListened(int sock_listen, fd_set *setFD);

/* borra un socket del set indicado y lo cierra
 */
void clearAndClose(int *fd, fd_set *setFD);


#endif /* FUNCIONESCOMPARTIDAS_H_ */
