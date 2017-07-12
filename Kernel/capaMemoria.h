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
#define ES_ULTIMO_HMD(H, D) (((D) - ((H)->size + SIZEOF_HMD) < 0)? true : false)
#endif

typedef struct{
	int size;
	bool isFree;
} tHeapMeta;

typedef struct {
	int page;
	int max_size;
} tHeapProc;

void setupHeapStructs(void);

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

/* Reserva en Memoria una pagina para Heap;
 * Retorna 0 en salida exitosa.
 * Retorna negativo en caso contrario.
 */
int reservarPagHeap(int pid, int size_reserva);

/* Se utiliza cuando Kernel recibe la respuesta de Memoria de ASIGN_SUCCS.
 * Si la pagina que se asigno es de Heap, le escribe el Heap_Metadata a la misma.
 * Si la pagina era de Codigo/Stack, no produce efectos.
 * Retorna el numero de la pagina afectada, 0 si era de Codigo/Stack.
 * Retorna negativo si falla la funcion.
 */
int inicializarHMD(int pid, int pagenum);

void liberarMemoria(void);

/* Para un PID, actualiza su cantidad e indices de paginas de heap
 *
 */
void actualizarHProcsConPagina(int pid, int heap_page);


// manejo de heap 2.0

/* Reserva un bloque de datos en Heap interactuando con memoria y administrando
 * el pedido de paginas nuevas segun se requiera.
 * Retorna el puntero absoluto. Si falla retorna 0;
 */
t_puntero reservar(int pid, int size);
t_puntero intentarReservaUnica(int pid, int pag);

int escribirEnMemoria(int pid, int pag, char *heap);

t_puntero reservarEnHeap(int pid, int size);

/* Busca la pagina solicitada en Memoria y la trae completa;
 * es un wrapper de Solicitud de Bytes.
 * Retorna el contenido de la pagina en un buffer.
 */
char *obtenerHeapDeMemoria(int pid, int pag);

int crearNuevoHeap(int pid);

int pagMasReciente(int pid);

void enviarPrimerHMD(int pid, int pag);

void agregarHeapAPID(int pid, int pag);

tHeapMeta *nextBlock(tHeapMeta *hmd, int *dist);
tHeapMeta *nextFreeBlock(tHeapMeta *hmd, int *dist);
int getMaxFreeBlock(char *heap);

/* Revisa en el diccionario si un PID tiene alguna pagina de Heap
 */
bool tieneHeap(int pid);

#endif /* CAPAMEMORIA_H_ */
