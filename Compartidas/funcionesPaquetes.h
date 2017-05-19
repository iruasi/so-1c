#ifndef FUNCIONESPAQUETES_H_
#define FUNCIONESPAQUETES_H_


/* dado un socket e identificador de proceso, le envia un paquete basico de HandShake
 */
int handshakeCon(int sock_dest, int id_sender);

/* Solo la usa Memoria, para dar a Kernel informacion relevante de si misma
 * Retorna la cantidad de bytes enviados, deberian ser 16
 */
int contestarMemoriaKernel(int size_marco, int cant_marcos, int socket_kernel);

/* Solo la usa Kernel para recibir de Memoria los frames y el size de estos,
 * los almacenara en *frames y *frame_size.
 * Se corresponde reciprocamente con la funcion contestarMemoriaKernel()
 * !! Asumimos que ya se recibio el Header !!
 */
int recibirInfoMem(int sock_memoria, int *frames, int *frame_size);

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
