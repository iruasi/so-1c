#ifndef MANEJADORESMEM_H_
#define MANEJADORESMEM_H_

#include <stdbool.h>

#include "structsMem.h"
#include "memoriaConfigurators.h"

extern tMemoria *memoria;
extern tMemEntrada *CACHE;
extern uint8_t *MEM_FIS;

/* Crea una cantidad quant de frames de tamanio size.
 * Se usa para la configuracion del espacio de memoria inicial
 */
uint8_t *setupMemoria(int quantity, int size);

/* 'Crea' una cantidad quant de frames de tamanio size.
 * Se usa para la configuracion del espacio de memoria inicial
 */
tMemEntrada *setupCache(int quantity);

/* limpia toda la cache
 */
void wipeCache(tMemEntrada *cache, uint32_t quant);

/* recorre secuencialmente CACHE hasta dar con una entrada que tenga el pid y page buscados
 */
uint8_t *buscarEnCache(uint32_t pid, uint32_t page);

/* obtiene la dir de una entrada reemplazable en cache y la pisa
 */
void insertarEnCache(tMemEntrada entry);

/* nos dice si tenemos un cache hit o miss
 */
bool entradaCoincide(tMemEntrada entrada, uint32_t pid, uint32_t page);

/* mediante la funcion de hash encuentra el frame de la pagina de un proceso
 */
uint8_t *buscarEnMemoria(uint32_t pid, uint32_t page);

/* crea una entrada de tipo tMemEntrada, util para la cache
 */
tMemEntrada crearEntrada(uint32_t pid, uint32_t page, uint32_t frame);

/* Dado un PID y una cantidad de paginas, intenta crear las estructuras de administracion necesarias
 * para un programa.
 * De momento retorna un puntero al lugar de memoria que puede afectar o NULL
 * TODO: deberia retornar un int o nada; precisamos la tabla de paginas invertida
 */
uint8_t *inicializarPrograma(int pid, int page_count, int sourceSize, void *srcCode);

uint8_t *asignarPaginas(int pid, int page_count);

/* Crea un HMD para un size requerido de reserva. Actualmente trabaja sobre MEM_FIS
 * Retorna un puntero a un espacio de MEM_FIS donde se podran escribir bytes
 * Retorna NULL si no fue posible reservar el espacio pedido
 */
uint8_t *reservarBytes(int sizeReserva);

/* Nos dice, dado un HMD, si su espacio de memoria es reservable por una cantidad size de bytes
 * Si es el ultimo HMD, retorna un valor #definido
 */
uint8_t esReservable(int size, tHeapMeta *heapMetaData);

/* Esta funcion deberia usarse solamente para crear el HMD al final de una reserva en heap.
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
void insertarEnCache(tMemEntrada entry);

/* es parte del API de Memoria; Realiza el pedido de escritura de CPU
 * Esta es la funcion que en el TP viene a ser "Almacenar Bytes en una Pagina"
 */
uint32_t almacenarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size, uint8_t* buffer);

/* esta la utiliza `almacenarBytes', una vez que ya conoce en que frame debe almacenar el buffer
 */
void escribirBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size, void* buffer); // todo: escribir implementacion

/* Se ejecuta al recibir el del CPU
 */
uint8_t *solicitarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size);

/* Esta es la funcion que en el TP viene a ser "Solicitar Bytes de una Pagina"
 */
uint8_t *leerBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size);

/* deshace la fragmentacion interna del HEAP dado en heap_dir
 */
void defragmentarHeap(uint8_t *heap_dir);

#endif
