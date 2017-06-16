#ifndef MANEJADORESCACHE_H_
#define MANEJADORESCACHE_H_

/* 'Crea' una cantidad quant de frames de tamanio size.
 * Se usa para la configuracion del espacio de memoria inicial
 */
int setupCache(void);

/* Procedimiento para que cada Linea a CACHE apunte donde le corresponde
 */
void setupCacheLines(void);

/* dado un pid y pagina, retorna el contenido en CACHE;
 */
char *getCacheContent(int pid, int page);

/* Disminuye en 1 un contador de cada entrada para la CACHE,
 * salvo el que esta indicado por el indice `accesed'
 * Lo utiliza el LRU para desalojar segun el contador mas bajo.
 */
void cachePenalizer(int accessed);

/* Si no alcanzo el limite, inserta una pagina nueva en CACHE.
 * Utiliza el algoritmo LRU para elegir una victima, de ser necesario
 */
int insertarEnCache(int pid, int page, char *cont);

/* Esta funcion parece que no se utiliza
 */
tCacheEntrada *allocateCacheEntry(void);

/* Busca la primera entrada libre o en su defecto la entrada menos utilizada (LRU)
 * Retorna el puntero a la Linea de CACHE a ser reemplazada.
 */
tCacheEntrada *getCacheVictim(int *min_index);

/* Retorna la cantidad de paginas que un PID dado tiene en CACHE
 */
int pageCachedQuantity(int pid);

/* muestra el pid y pagina de cada linea de CACHE
 */
void dumpCache(void);

#endif /* MANEJADORESCACHE_H_ */
