#ifndef FUNCIONESPAQUETES_H_
#define FUNCIONESPAQUETES_H_

#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>

int contestar2ProcAProc(tPackHeader h, int val1, int val2, int sock);
int contestarProcAProc(tPackHeader head, int val, int sock);

/* Solo la usa Kernel para recibir de Memoria los frames y el size de estos,
 * los almacenara en *frames y *frame_size.
 * Se corresponde reciprocamente con la funcion contestarMemoriaKernel()
 * !! Asumimos que ya se recibio el Header !!
 */

/* Parecida a la que utiliza Kernel. La diferencia a considerar es que recibe
 * un paquete completo con HEADER, lo cual es un comportamiento muy particular.
 */
int recibirInfo2ProcAProc(int sock, tPackHeader h_esp, int *val1, int *val2);

/* Recibe un header (un handshake) y recvGenericamente un valor entero que llega.
 * Asigna el valor entero a la direccion de memoria de `var'.
 */
int recibirInfoProcSimple(int sock, tPackHeader h_esp, int *var);

char *serialize2ProcAProc(tHShake2ProcAProc *h_shake, int *pack_size);

char *serializeProcAProc(tHShakeProcAProc *h_shake, int *pack_size);

/* Funcion generica de recepcion. Toda serializacion deberia dar un paquete
 * que responda al formato |HEAD(8)|PAYLOAD_SIZE(int)|PAYLOAD(char*)|
 * Luego cada funcion de deserializacion se debe encargar de interpretar el PAYLOAD
 *  !!!!!! Esta funcion comprende que ya se recibio el HEAD de 8 bytes !!!!!!
 */
char *recvGeneric(int sock_in);
char *recvGenericWFlags(int sock_in, int flags);

/* Para mandar solamente un Header. Util para informar fallos */
char *serializeHeader(tPackHeader head, int *pack_size);

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
char *serializeVal(tPackVal *pval, int *pack_size);

tPackPID *deserializeVal(char *val_serial);

/* Serializa un tPackPIDPag dado.
 * Retorna el buffer serializado;
 * retorna NULL si falla
 */
char *serializePIDPaginas(tPackPidPag *ppidpag, int *pack_size);

tPackPidPag *deserializePIDPaginas(char *pidpag_serial);

/****** Definiciones de [De]Serializaciones CPU ******/

char *serializeAbrir(tPackAbrir * abrir, int *pack_size);
tPackAbrir * deserializeAbrir(char *abrir_serial);

char *serializeMoverCursor(t_descriptor_archivo descriptor_archivo, t_valor_variable posicion, int *pack_size);
tPackCursor *deserializeMoverCursor(char *cursor_serial);

char *serializeEscribir(t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio, int *pack_size);
tPackRW *deserializeEscribir(char *escr_serial);

char *serializeRW(tPackHeader head, tPackRW *read_write, int *pack_size);
tPackRW * deserializeRW(char * rw_serial);

char *serializeLeer(tPackLeer *read, int *pack_size);
tPackLeer *deserializeLeer(char *leer_serial);

char *serializeEscribirFS2(tPackHeader head, t_direccion_archivo path, t_puntero cursor, int size, void *info, int *pack_size);

char *serializeLeerFS2(tPackHeader head, t_direccion_archivo path, t_puntero cursor, int size, int *pack_size);
tPackRecvRW *deserializeLeerFS2(char *leer_serial);

char *serializeValorYVariable(tPackHeader head, t_valor_variable valor, t_nombre_compartida variable, int *pack_size);
tPackValComp *deserializeValorYVariable(char *valor_serial);


/* Retorna el peso en bytes de todas las listas y variables sumadas del stack
 */
int sumarPesosStack(t_list *stack);

char * serializeFileDescriptor(tPackFS * fileSystem,int *pack_size);
tPackFS * deserializeFileDescriptor(char * aux_serial);
tPackAbrir * deserializeAbrir(char *abrir_serial);

//serializeLeerFS, guarda el paquete que hay que mandarle al fs para leer
char *serializeLeerFS(tPackHeader head, t_direccion_archivo path, void * info,t_valor_variable tamanio, t_puntero cursor ,int * pack_size);
tPackRecibirRW * deserializeLeerFS(char * recibir_serial);

void informarResultado(int sock, tPackHeader head);


#endif /* FUNCIONESPAQUETES_H_ */
