#ifndef FUNCIONESPAQUETES_H_
#define FUNCIONESPAQUETES_H_


/* dado un socket e identificador de proceso, le envia un paquete basico de HandShake
 */
int handshakeCon(int sock_dest, int id_sender);


/* recibimos codigo fuente del socket de entrada
 * devolvemos un puntero a memoria que lo contiene
 */
tPackSrcCode *recvSourceCode(int sock_in);

#endif /* FUNCIONESPAQUETES_H_ */
