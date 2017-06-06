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

/* todo: descripcion
 */
void cachePenalizer(int accessed);

/* todo: descripcion
 */
int insertarEnCache(int pid, int page, char *cont);

/* todo: description
 */
tCacheEntrada *allocateCacheEntry(void);

/* todo: descripcion
 */
tCacheEntrada *getCacheVictim(int *min_index);

/* todo: descripcion
 */
int pageCachedQuantity(int pid);

/* muestra el pid y pagina de cada linea de CACHE
 */
void dumpCache(void);

#endif /* MANEJADORESCACHE_H_ */
