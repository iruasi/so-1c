#ifndef _AUXILIARESMEMORIA_H_
#define _AUXILIARESMEMORIA_H_

int reservarPaginas(int pid, int pageCount);
int pageQuantity(int pid);
bool frameLibre(int frame, int off);
bool pid_match(int pid, int frame, int off);

/* Se usa para trasladarse sobre la tabla de paginas invertida
 */
void nextFrameValue(int *fr, int *off, int step_size);

/* Dado un numero de frame representativo de la MEM_FIS, modifica fr_inv y off_inv tal que `apunten' al frame
 * correspondiente en la tabla de paginas invertida...
 */
void gotoFrameInvertida(int frame_repr, int *fr_inv, int *off_inv);

#endif // _AUXILIARESMEMORIA_H_
