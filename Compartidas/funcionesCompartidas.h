#ifndef FUNCIONESCOMPARTIDAS_H_
#define FUNCIONESCOMPARTIDAS_H_

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


#endif /* FUNCIONESCOMPARTIDAS_H_ */
