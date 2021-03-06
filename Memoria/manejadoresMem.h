#ifndef MANEJADORESMEM_H_
#define MANEJADORESMEM_H_

#include <stdbool.h>

#include "structsMem.h"
#include "memoriaConfigurators.h"

extern char *MEM_FIS;

#ifndef SIZEOF_HMD
#define SIZEOF_HMD 5
#endif

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

/* Realiza una busqueda en la Tabla de Paginas Invertida.
 * Mediante la funcion de hash encuentra el frame que corresponde a la pagina de un proceso
 * Retorna el frame necesitado (valor 0 o positivo)
 * Retorna valor negativo en caso de fallos.
 */
int buscarEnMemoria(int pid, int page);

/* Dado un PID y una pagina, retorna el frame aproximado que corresponde
 */
int frameHash(int pid, int page);

/* Esta es la funcion que en el TP viene a ser "Solicitar Bytes de una Pagina"
 */
char *leerBytes(int pid, int frame, int offset, int size);

char *getMemContent(int frame, int offset);

/* muestra el pid y pagina de cada entrada de la Tabla de Paginas Invertida,
 * y muestra tambien info de los procesos activos
 */
void dumpMemStructs(void);

/* muestra el contenido de las paginas de un proceso indicado,
 * o de todos los procesos en Memoria Fisica
 */
void dumpMemContent(int pid);

int *obtenerPIDsKernel(int *len);

/* obtenida de codigo publicado en git */
void DumpHex(const void* data, size_t size);

#endif
