#ifndef _AUXILIARESMEMORIA_H_
#define _AUXILIARESMEMORIA_H_

/* Para un pid y cantidad de paginas, asigna las paginas subsiguientes
 * a las que ya tenga en la Tabla de Paginas Invertida.
 * Retorna el numero de la primera pagina nueva reservada, si finaliza correctamente.
 */
int reservarPaginas(int pid, int pageCount);

/* En la Tabla de Paginas Invertida, busca la cantidad de paginas tiene asociado
 * el PID pasado por parametro.
 * Retorna negativo si se ingresa un PID invalido.
 * Sino retorna la cantidad de paginas encontrada (pueden ser 0 o muchas).
 */
int pageQuantity(int pid);

/* Retorna true si, dados un frame y offset, el PID coincide con el de la Tabla de Paginas Invertida
 * Retorna false en caso contrario
 */
bool pid_match(int pid, int frame, int off);

/* Retorna true si, dados un frame y offset, el PID de la Tabla de Paginas Invertida indica estar libre
 * Retorna false en caso contrario
 */
bool frameLibre(int frame, int off);


/* Sobreescribe cualquier entrada en CACHE que pertenezca al pid,
 * se reemplaza con pid_free y free_page.
 */
void limpiarDeCache(int pid);

/* Sobreescribe cualquier entrada en la Tabla de Invertidas que pertenezca al pid,
 * se reemplaza con pid_free y free_page.
 */
void limpiarDeInvertidas(int pid);

/* Se usa para trasladarse de frame a frame sobre la tabla de paginas invertida
 */
void nextFrameValue(int *fr, int *off, int step_size);

/* Dado un numero de frame representativo de la MEM_FIS, modifica fr_inv y off_inv tal que `apunten' al frame
 * correspondiente en la tabla de paginas invertida...
 */
void gotoFrameInvertida(int frame_repr, int *fr_inv, int *off_inv);

void consolaMemoria(void);


#endif // _AUXILIARESMEMORIA_H_
