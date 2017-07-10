#ifndef FUNCIONESPAQUETES_H_
#define FUNCIONESPAQUETES_H_

#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>

/* Solo las usa Memoria, para dar a Kernel y CPU informacion relevante de si misma
 * Retorna la cantidad de bytes enviados, o el error (valor negativo)
 */
int contestarMemoriaKernel(int size_marco, int cant_marcos, int socket_kernel);

int contestarProcAProc(tPackHeader head, int val, int sock);

/* Solo la usa Kernel para recibir de Memoria los frames y el size de estos,
 * los almacenara en *frames y *frame_size.
 * Se corresponde reciprocamente con la funcion contestarMemoriaKernel()
 * !! Asumimos que ya se recibio el Header !!
 */
int recibirInfoKerMem(int sock_memoria, int *frames, int *frame_size);

/* Parecida a la que utiliza Kernel. La diferencia a considerar es que recibe
 * un paquete completo con HEADER, lo cual es un comportamiento muy particular.
 */
int recibirInfoCPUMem(int sock_mem, int *frame_size);

int recibirInfoCPUKernel(int sock_kern, int *q_sleep);

char *serializeMemAKer(tHShakeMemAKer *h_shake, int *pack_size);

char *serializeProcAProc(tHShakeMemACPU *h_shake, int *pack_size);

/* Funcion generica de recepcion. Toda serializacion deberia dar un paquete
 * que responda al formato |HEAD(8)|PAYLOAD_SIZE(int)|PAYLOAD(char*)|
 * Luego cada funcion de deserializacion se debe encargar de interpretar el PAYLOAD
 *  !!!!!! Esta funcion comprende que ya se recibio el HEAD de 8 bytes !!!!!!
 */
char *recvGeneric(int sock_in);

/* Serializa un buffer de bytes para que respete el protocolo de HEADER
 * int *pack_size se usa para almacenar el size del paquete serializado, asi se lo puede send'ear
 */
char *serializeBytes(tPackHeader head, char* buffer, int buffer_size, int *pack_size);

/* Deserializa un buffer en un Paquete de Bytes
 */
tPackBytes *deserializeBytes(char *bytes_serial);


char *serializePCB(tPCB *pcb, tPackHeader head, int *pack_size);

/* Funcion auxiliar para crear un buffer con el stack serializado;
 * luego podremos memcpy'arlo al pcb_serializado

 * La forma de este buffer es: |TAMANIO_STACK|STACK_SERIALIZADO|
 */
char *serializarStack(tPCB *pcb, int pesoStack, int *pack_size);


/* Consideramos que ya hicimos recv()
 * Recibimos un buffer con el PCB y lo deserializamos a la struct apropiada.
 * (Hacer free a lo que malloquea)
 */
tPCB *deserializarPCB(char *pcb_serial);

void deserializarStack(tPCB *pcb, char *pcb_serial, int *offset);

/* Serializa un Pedido de Bytes para la Memoria
 */
char *serializeByteRequest(tPackByteReq *pbr, int *pack_size);

/* Util para deserializar tanto el ByteRequest como el InstrRequest
 */
tPackByteReq *deserializeByteRequest(char *byterq_serial);

/* serializa un paquete de peticion de almacenamiento.
 */
char *serializeByteAlmacenamiento(tPackByteAlmac *pbal, int* pack_size);

/* Deserializa un buffer en un paquete de Almacenamiento de Bytes a Memoria.
 */
tPackByteAlmac *deserializeByteAlmacenamiento(char * pbal_serial);

/* Serializa un tPackPID dado.
 * Retorna el buffer serializado;
 * retorna NULL si falla
 */
char *serializePID(tPackPID *ppid, int *pack_size);

tPackPID *deserializePID(char *pid_serial);

/* Serializa un tPackPIDPag dado.
 * Retorna el buffer serializado;
 * retorna NULL si falla
 */
char *serializePIDPaginas(tPackPidPag *ppidpag, int *pack_size);

tPackPidPag *deserializePIDPaginas(char *pidpag_serial);

/****** Definiciones de [De]Serializaciones CPU ******/

char *serializeAbrir(t_direccion_archivo direccion, t_banderas flags, int *pack_size);

char *serializeMoverCursor(t_descriptor_archivo descriptor_archivo, t_valor_variable posicion, int *pack_size);

char *serializeEscribir(t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio, int *pack_size);

tPackEscribir *deserializeEscribir(char *escr_serial);

char *serializeLeer(t_descriptor_archivo descriptor_archivo, t_puntero informacion, t_valor_variable tamanio, int *pack_size);

char *serializeValorYVariable(tPackHeader head, t_valor_variable valor, t_nombre_compartida variable, int *pack_size);

tPackValComp *deserializeValorYVariable(char *valor_serial);

/* Retorna el peso en bytes de todas las listas y variables sumadas del stack
 */
int sumarPesosStack(t_list *stack);

#endif /* FUNCIONESPAQUETES_H_ */
