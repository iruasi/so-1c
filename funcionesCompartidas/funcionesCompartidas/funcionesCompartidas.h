#ifndef FUNCIONESCOMPARTIDAS_H_
#define FUNCIONESCOMPARTIDAS_H_

#include <sys/select.h>
#include <sys/types.h>
#include <stdbool.h>

/* Verifica que dos valores enteros sean equivalentes. Si lo son, retorna true.
 * Si no coinciden, emite el mensaje `errmsg' en stderr junto con los valores obtenidos, y retorna false.
 */
bool assertEq(int expected, int actual, const char* errmsg);

/* Medida de seguridad. No solo hace free(pointer) sino que reasigna el pointer a NULL,
 * de esta manera, si se usare accidentalmente a futuro, es mas seguro que no toque nada critico
 */
void freeAndNULL(void **pointer);

/* Retorna el tamanio de un archivo
 */
unsigned long fsize(FILE* f);

/* Recibe una estructura que almacena informacion del propio host;
 * La inicializa con valores utiles, pasados por parametro
 */
void setupHints(struct addrinfo *hints, int address_family, int socket_type, int flags);

/* dado un socket e identificador de proceso, le envia un paquete basico de HandShake
 */
int handshakeCon(int sock_dest, int id_sender);


/* Dados un ip y puerto de destino, se crea, conecta y retorna socket apto para comunicacion
 * La deberia utilizar unicamente Iniciar_Programa, por cada nuevo hilo para un script que se crea
 * En caso de fallo retorna valores negativos.
 */
int establecerConexion(char *ip_destino, char *port_destino);

/* Crea un socket y lo bindea() a un puerto particular,
 * luego retorna este socket, apto para listen()
 * En caso de fallo retorna valores negativos.
 */
int makeListenSock(char *port_listen);

/* acepta una conexion entrante, y crea un socket para comunicaciones regulares;
 * En caso de fallo retorna valores negativos.
 */
int makeCommSock(int socket_in);

/* Limpia un buffer usado por cada proceso para emitir varios mensajes
 */
void clearBuffer(char *buffer, int bufferSize);

/* Atiende una conexion entrante, la agrega al set de relevancia, y vuelve a escuhar mas conexiones;
 * retorna el nuevo socket producido
 */
int handleNewListened(int sock_listen, fd_set *setFD);

/* borra un socket del set indicado y lo cierra
 */
void clearAndClose(int *fd, fd_set *setFD);

/* Util para recibir cualquier struct sin campos variables
 */
int cantidadTotalDeBytesRecibidos(int fdServidor, char * buffer, int tamanioBytes);

#endif /* FUNCIONESCOMPARTIDAS_H_ */
