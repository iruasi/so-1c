#ifndef MANEJADORESMEM_H_
#define MANEJADORESMEM_H_

#include <stdbool.h>

#include "structsMem.h"
#include "memoriaConfigurators.h"

extern tMemoria *memoria;
extern tCacheEntrada *CACHE;
extern char *MEM_FIS;

#define SIZEOF_HMD 5
#define ULTIMO_HMD 0x02 // valor hexa de 1 byte, se distingue de entre true y false

void abortar(int pid);

/* Crea una cantidad quant de frames de tamanio size.
 * Se usa para la configuracion del espacio de memoria inicial
 * Ademas escribe la tabla de paginas invertida en sus primeros bytes.
 */
int setupMemoria();
void populateInvertidas();

/* 'Crea' una cantidad quant de frames de tamanio size.
 * Se usa para la configuracion del espacio de memoria inicial
 */
int setupCache(int quantity);

/* recorre secuencialmente CACHE hasta dar con una entrada que tenga el pid y page buscados
 */
uint8_t *buscarEnCache(uint32_t pid, uint32_t page);

/* obtiene la dir de una entrada reemplazable en cache y la pisa
 */
void insertarEnCache(tCacheEntrada entry);

/* nos dice si tenemos un cache hit o miss
 */
bool entradaCoincide(tCacheEntrada entrada, uint32_t pid, uint32_t page);

/* mediante la funcion de hash encuentra el frame que corresponde a la pagina de un proceso
 */
int buscarEnMemoria(uint32_t pid, uint32_t page);

/* crea una entrada de tipo tMemEntrada, util para la cache
 */
tCacheEntrada crearEntrada(uint32_t pid, uint32_t page, uint32_t frame);


/* Crea en espacio Heap un HMD para un size requerido de reserva. Actualmente trabaja sobre MEM_FIS de una...
 * Retorna un puntero a un espacio de MEM_FIS donde se podran escribir bytes
 * Retorna NULL si no fue posible reservar el espacio pedido
 */
char *reservarBytes(int sizeReserva);

/* Nos dice, dado un HMD, si su espacio de memoria es reservable por una cantidad size de bytes
 * Si es el ultimo HMD, retorna un valor #definido
 */
uint8_t esReservable(int size, tHeapMeta *heapMetaData);

/* Esta funcion deberia usarse solamente para crear el HMD al final de una reserva en heap,
 * o para crear el primer HMD al setear la MEMORIA_FISICA...
 * Crea un nuevo HMD de tamanio size en una direccion de memoria, y por defecto esta libre
 */
tHeapMeta *crearNuevoHMD(uint8_t *dir_mem, int size);

/* Busca el frame en cache o en la memoria principal
 */
uint8_t *obtenerFrame(uint32_t pid, uint32_t page);

/* dado un pid y pagina, retorna el contenido en CACHE;
 */
uint8_t *buscarEnCache(uint32_t pid, uint32_t page);

/* inserta una nueva entrada en CACHE, utiliza el algoritmo LRU
 */
void insertarEnCache(tCacheEntrada entry);


/* esta la utiliza `almacenarBytes', una vez que ya conoce en que frame debe almacenar el buffer
 */
void escribirBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size, void* buffer); // todo: escribir implementacion


/* Esta es la funcion que en el TP viene a ser "Solicitar Bytes de una Pagina"
 */
uint8_t *leerBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size);

/* deshace la fragmentacion interna del HEAP dado en heap_dir
 */
void defragmentarHeap(uint8_t *heap_dir);

#endif
