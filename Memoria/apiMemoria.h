#ifndef APIMEMORIA_H_
#define APIMEMORIA_H_

#include "structsMem.h"

#define PID_FREE -2 // pid disponible
#define PID_INV  -1 // pid tabla invertida

// OPERACIONES DE LA MEMORIA

/* Cant de milisegundos de rta de Memoria
 *
 */
void retardo(void);

/* limpia toda la CACHE
 */
void flush(tCacheEntrada *cache, uint32_t quant);

/* Para la Memoria o un PID, da nocion de su tamanio
 */
void size(void);

/* Dumpea lo que hay en MEM_FIS
 * despues tendria que ampliarse para dumpear el stack..
 * TODO: A futuro debe dumpear en disco y en pantalla -> CACHE, MEM_FIS, Tabla de Pags y Procesos Activos
 */
void dump(void *memory_location);


// API DE LA MEMORIA

/* Crea los segmentos del programa, segun pueda
 */
void *inicializarPrograma(int pid, int pageCount);

/* Dado un PID y una cantidad de paginas, intenta crear las estructuras de administracion necesarias
 * para un programa.
 * De momento retorna un puntero al lugar de memoria que puede afectar o NULL
 * TODO: deberia retornar un int o nada; precisamos la tabla de paginas invertida
 */
uint8_t *inicializarProgramaBeta(int pid, int page_count, int sourceSize, void *srcCode);

/* es parte del API de Memoria; Realiza el pedido de escritura de CPU
 * Esta es la funcion que en el TP viene a ser "Almacenar Bytes en una Pagina"
 */
uint32_t almacenarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size, uint8_t* buffer);

/* Se ejecuta al recibir el del CPU
 */
uint8_t *solicitarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size);

/* AMPLIA la cantidad de paginas ya existnetes para el proceso
 */
uint8_t *asignarPaginas(int pid, int page_count);


#endif // APIMEMORIA_H_
