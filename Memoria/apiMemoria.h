#ifndef APIMEMORIA_H_
#define APIMEMORIA_H_

#include "structsMem.h"

#ifndef PID_MEM // es para la distinguir la Memoria de un PID cualquiera
#define PID_MEM -2
#endif

#ifndef PID_INV // es para la distinguir la Memoria de un PID cualquiera
#define PID_INV -2
#endif

#ifndef FREE_PAGE
#define FREE_PAGE -1
#endif

// OPERACIONES DE LA MEMORIA

/* Cantidad de milisegundos de latencia de accesos a Memoria
 */
void retardo(int ms);

/* limpia toda la CACHE
 */
void flush(void);

/* Dumpea lo que hay en MEM_FIS
 * despues tendria que ampliarse para dumpear el stack..
 * TODO: A futuro debe dumpear en disco y en pantalla -> CACHE, MEM_FIS, Tabla de Pags y Procesos Activos
 */
void dump(int pid);

/* Para un PID o PID_MEMORIA, da nocion de su tamanio
 * Si se lo llama con un pid de proceso (mayor que 0), afecta a proc_size
 * Si se lo llama con PID_MEM, solo afecta las ultimas tres variables
 */
void size(int pid, int *proc_size, int *mem_frs, int *mem_ocup, int *mem_free);


// API DE LA MEMORIA

/* Crea las paginas de codigo y stack de un programa
 */
int inicializarPrograma(int pid, int pageCount);

/* es parte del API de Memoria; Realiza el pedido de escritura de CPU
 * Esta es la funcion que en el TP viene a ser "Almacenar Bytes en una Pagina"
 */
int almacenarBytes(int pid, int page, int offset, int size, char *buffer);

/* Se ejecuta al recibir el del CPU
 */
char *solicitarBytes(int pid, int page, int offset, int size);

/* (Esta funcion la puede pedir Kernel a Memoria directamente)
 * Para un pid y cantidad de paginas, asigna las paginas subsiguientes
 * a las que ya tenga en la Tabla de Paginas Invertida.
 * Retorna 0 si finaliza correctamente.
 */
int asignarPaginas(int pid, int pageCount);

void liberarPagina(int pid, int page);

#endif // APIMEMORIA_H_
