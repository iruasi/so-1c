#ifndef CAPAMEMORIA_H_
#define CAPAMEMORIA_H_

#include <stdbool.h>

#include <parser/parser.h>

#ifndef SIZEOF_HMD
#define SIZEOF_HMD 5
#endif

#ifndef VALID_ALLOC
#define VALID_ALLOC(S) (((S) > 0 && (S) <= MAX_ALLOC_SIZE)? true : false)
#endif

#ifndef ES_ULTIMO_HMD
#define ES_ULTIMO_HMD(H, D) (((D) - ((H)->size + SIZEOF_HMD) <= 0)? true : false)
#endif

typedef struct{
	int size;
	bool isFree;
} tHeapMeta;

typedef struct {
	int page;
	int max_size;
} tHeapProc;

typedef struct {
	int cant_heaps,
		cant_alloc,
		bytes_allocd,
		cant_frees,
		bytes_freed;
} infoHeap;

/* ejecutar despues de tener frame_size definido
 */
void setupGlobales_capaMemoria(void);

/* Nos dice, dado un HMD, si su espacio de memoria es reservable por un size
 */
bool esReservable(int size, tHeapMeta *hmd);

/* Crea un nuevo HMD de tamanio `size' en la direccion `dir_mem'.
 */
void crearNuevoHMD(tHeapMeta *dir_mem, int size);

/* Dada una pagina de heap y un tamanio de bytes a reservar,
 * intenta reservar en un bloque suficientemente grande.
 * Retorna un puntero a la direccion del bloque escribible si va bien.
 * Retorna NULL si no encuentra espacio reservable en esa pagina.
 */
t_puntero reservarBytes(char* heap_page, int sizeReserva);

// manejo de heap 2.0

/* Reserva un bloque de datos en Heap interactuando con memoria y administrando
 * el pedido de paginas nuevas segun se requiera.
 * Retorna el puntero absoluto. Si falla retorna 0;
 */
t_puntero reservar(int pid, int size);
t_puntero intentarReservaUnica(int pid, int pag);

/* Trata de liberar el bloque de memoria apuntado por ptr.
 * En caso de fallo, se informa a la CPU.
 */
int liberar(int pid, t_puntero ptr);
void consolidar(char *heap);

int escribirEnMemoria(int pid, int pag, char *heap);

t_puntero reservarEnHeap(int pid, int size);

/* Busca la pagina solicitada en Memoria y la trae completa;
 * es un wrapper de Solicitud de Bytes.
 * Retorna el contenido de la pagina en un buffer. o NULL.
 */
char *obtenerHeapDeMemoria(int pid, int pag);

int crearNuevoHeap(int pid);

void agregarHeapAPID(int pid, int pag);

void liberarHeapEnKernel(int pid);

tHeapMeta *nextBlock(tHeapMeta *hmd, int *dist);
tHeapMeta *nextFreeBlock(tHeapMeta *hmd, int *dist);
int getMaxFreeBlock(char *heap);

/* Revisa en el diccionario si un PID tiene alguna pagina de Heap
 */
bool tieneHeap(int pid);

bool paginaPerteneceAPID(char *spid, int pag, tHeapProc **hp);
bool punteroApuntaABloque(char *heap, t_puntero ptr);
bool punteroApuntaABloqueValido(char *heap, t_puntero ptr);

#endif /* CAPAMEMORIA_H_ */
