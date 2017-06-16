#ifndef FUNCIONESPAQUETES_H_
#define FUNCIONESPAQUETES_H_

#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>

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


/* Serializa un buffer de bytes para que respete el protocolo de HEADER
 * int *pack_size se usa para almacenar el size del paquete serializado, asi se lo puede send'ear
 */
char *serializeBytes(tProceso proc, tMensaje msj, char* buffer, int buffer_size, int *pack_size);

/* Deserializa un buffer en un Paquete de Bytes
 */
tPackBytes *deserializeBytes(int sock_in);


char *serializePCB(tPCB *pcb, tPackHeader head, int *pack_size);

/* Funcion auxiliar para crear un buffer con el stack serializado;
 * luego podremos memcpy'arlo al pcb_serializado
 */
char *serializarStack(tPCB *pcb, int pesoStack, int *pack_size);


/* Consideramos que ya hicimos recv()
 * Recibimos un buffer con el PCB y lo deserializamos a la struct apropiada.
 * (Hacer free a lo que malloquea)
 */
tPCB *deserializarPCB(char *pcb_serial);

void deserializarStack(tPCB *pcb, char *pcb_serial, int *offset);

/* para el momento que ejecuta esta funcion, ya se recibio el HEADER de 8 bytes,
 * por lo tanto hay que recibir el resto del paquete...
 */
char *recvPCB(int sock_in);


/* Serializa un pcb para poder hacer una Solicitud de Bytes a Memoria
 */
char *serializeByteRequest(tPCB *pcb, int size_instr, int *pack_size);

/* Deserializa un buffer (recibiendo desde sock_in) en un paquete de Solicitud de Bytes a Memoria.
 */
tPackByteReq *deserializeByteRequest(int sock_in);

/* Deserializa un buffer (recibiendo desde sock_in) en un paquete de Almacenamiento de Bytes a Memoria.
 */
tPackByteAlmac *deserializeByteAlmacenamiento(int sock_in);

/* recibimos codigo fuente del socket de entrada
 * devolvemos un puntero a memoria que lo contiene
 * !! Asumimos que ya se recibio el Header !!
 */
tPackSrcCode *recvSourceCode(int sock_in);

/* Dado un socket de recepcion, recibe y deserializa codigo fuente
 */
tPackSrcCode *deserializeSrcCode(int sock_in);

/* Recibe de un socket el codigo fuente y lo serializa,
 * copia ademas los contenidos de algun header pasado por parametro
 * retorna un espacio de memoria utilizable para reenviar el codigo fuente.
 * !! Suponemos que ya se recibio antes un tPackHeader u 8 bytes !!
 */
void *serializeSrcCodeFromRecv(int sock_in, tPackHeader head, int *serialized_pack_size);

/* Serializa un tPackPID dado.
 * Retorna el buffer serializado;
 * retorna NULL si falla
 */
char *serializePID(tPackPID *ppid);

/* Serializa un tPackPIDPag dado.
 * Retorna el buffer serializado;
 * retorna NULL si falla
 */
char *serializePIDPaginas(tPackPidPag *ppidpag);

tPackPidPag *deserializePIDPaginas(char *pidpag_serial);

/* Retorna el peso en bytes de todas las listas y variables sumadas del stack
 */
int sumarPesosStack(t_list *stack);

#endif /* FUNCIONESPAQUETES_H_ */
