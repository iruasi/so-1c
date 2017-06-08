#ifndef MANEJADORESMEM_H_
#define MANEJADORESMEM_H_

#include <stdbool.h>

#include "structsMem.h"
#include "memoriaConfigurators.h"

extern char *MEM_FIS;

#ifndef SIZEOF_HMD
#define SIZEOF_HMD 5
#endif

#define ULTIMO_HMD 0x02 // valor hexa de 1 byte, se distingue de entre true y false

void abortar(int pid);

/* Libera todas las estructuras del proceso Memoria,
 * desde la Memoria Fisica hasta los componentes de la Cache
 */
void liberarEstructurasMemoria(void);

/* Crea una cantidad quant de frames de tamanio size.
 * Se usa para la configuracion del espacio de memoria inicial
 * Ademas escribe la tabla de paginas invertida en sus primeros bytes.
 */
int setupMemoria(void);

/* Crea en la Memoria Fisica las entradas de la Tabla de Paginas Invertida.
 * Les asigna un PID y PAGINA especificos segun sean Entradas Invertidas o frames de Memoria Fisica
 */
void populateInvertidas(void);

/* Realiza una busqueda secuencial en la Tabla de Paginas Invertida. TODO: Deberia usar una funcion de hashing
 * mediante la funcion de hash encuentra el frame que corresponde a la pagina de un proceso
 */
int buscarEnMemoria(int pid, int page);

/* Crea en espacio Heap un HMD para un size requerido de reserva. Actualmente trabaja sobre MEM_FIS de una...
 * Retorna un puntero a un espacio de MEM_FIS donde se podran escribir bytes
 * Retorna NULL si no fue posible reservar el espacio pedido
 */
char *reservarBytes(int pid, int heap_page, int sizeReserva);

/* Nos dice, dado un HMD, si su espacio de memoria es reservable por una cantidad size de bytes
 * Si es el ultimo HMD, retorna un valor #definido
 */
uint8_t esReservable(int size, tHeapMeta *heapMetaData);

/* Esta funcion deberia usarse solamente para crear el HMD al final de una reserva en heap,
 * o para crear el primer HMD al setear la MEMORIA_FISICA...
 * Crea un nuevo HMD de tamanio size en una direccion de memoria, y por defecto esta libre
 */
tHeapMeta *crearNuevoHMD(uint8_t *dir_mem, int size);

/* esta la utiliza `almacenarBytes', una vez que ya conoce en que frame debe almacenar el buffer
 */
int escribirBytes(int pid, int frame, int offset, int size, void* buffer); // todo: revisar implementacion

/* Esta es la funcion que en el TP viene a ser "Solicitar Bytes de una Pagina"
 */
char *leerBytes(int pid, int frame, int offset, int size);

/* deshace la fragmentacion interna del HEAP dado en heap_dir
 */
void defragmentarHeap(uint8_t *heap_dir);


int getMemFrame(int pid, int page);

char *getMemContent(int frame, int offset);

/* muestra el pid y pagina de cada entrada de la Tabla de Paginas Invertida,
 * y muestra tambien info de los procesos activos
 */
void dumpMemStructs(void);

/* muestra el contenido de las paginas de un proceso indicado,
 * o de todos los procesos en Memoria Fisica
 */
void dumpMemContent(int pid);

#endif
