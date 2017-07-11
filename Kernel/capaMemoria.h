#ifndef CAPAMEMORIA_H_
#define CAPAMEMORIA_H_

#include <stdbool.h>

#include <parser/parser.h>

#ifndef SIZEOF_HMD
#define SIZEOF_HMD 5
#endif

#ifndef ULTIMO_HMD
#define ULTIMO_HMD 0x02 // valor hexa de 1 byte, se distingue de entre true y false
#endif

typedef struct{
	int size;
	bool isFree;
} tHeapMeta;

typedef struct {
	int pid;
	int static_pages;
	int pag_heap_cant;
} tHeapProc;

void setupHeapStructs(void);

/* Nos dice, dado un HMD, si su espacio de memoria es reservable por una cantidad size de bytes
 * Retorna true si es reservable.
 * Si es el ultimo HMD, retorna el #define ULTIMO_HMD.
 * Aparte de esos casos, retorna false si no es reservable.
 */
uint8_t esReservable(int size, tHeapMeta *hmd);

/* Crea un nuevo HMD de tamanio `size' en la direccion `dir_mem'.
 */
void crearNuevoHMD(char *dir_mem, int size);

/* Dada una pagina de heap y un tamanio de bytes a reservar,
 * intenta reservar en un bloque suficientemente grande.
 * Retorna un puntero a la direccion del bloque escribible si va bien.
 * Retorna NULL si no encuentra espacio reservable en esa pagina.
 */
t_puntero reservarBytes(char* heap_page, int sizeReserva); // todo: cambios

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


t_puntero reservar(int pid, int size);

int escribirEnMemoria(int pid, int pag, char *heap);

t_puntero reservarEnHeap(int pid, int size, int *pag);

/* Busca la pagina solicitada en Memoria y la trae completa;
 * es un wrapper de Solicitud de Bytes.
 * Retorna el contenido de la pagina en un buffer.
 */
char *obtenerHeapDeMemoria(int pid, int pag);

int crearNuevoHeap(int pid);

int pagMasReciente(int pid);

void enviarPrimerHMD(int pid, int pag);

void agregarHeapAPID(int pid, int pag);

/* Revisa en el diccionario si un PID tiene alguna pagina de Heap
 */
bool tieneHeap(int pid);

#endif /* CAPAMEMORIA_H_ */
