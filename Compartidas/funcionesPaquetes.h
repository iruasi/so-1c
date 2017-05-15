#ifndef FUNCIONESPAQUETES_H_
#define FUNCIONESPAQUETES_H_


/* dado un socket e identificador de proceso, le envia un paquete basico de HandShake
 */
int handshakeCon(int sock_dest, int id_sender);


/* recibimos codigo fuente del socket de entrada
 * devolvemos un puntero a memoria que lo contiene
 * !! Asumimos que ya se recibio el Header !!
 */
tPackSrcCode *recvSourceCode(int sock_in);

/* Recibe de un socket el codigo fuente y lo serializa,
 * copia ademas los contenidos de algun header pasado por parametro
 * retorna un espacio de memoria utilizable para reenviar el codigo fuente.
 * !! Suponemos que ya se recibio antes un tPackHeader u 8 bytes !!
 */
void *serializeSrcCodeFromRecv(int sock_in, tPackHeader head, int *serialized_pack_size);

#endif /* FUNCIONESPAQUETES_H_ */
