#ifndef _AUXILIARESMEMORIA_H_
#define _AUXILIARESMEMORIA_H_

int reservarCodYStackInv(int pid, int pageCount);
int pageQuantity(int pid);
bool frameLibre(int frame, int off);
bool pid_match(int pid, int frame, int off);

/* Se usa para trasladarse sobre la tabla de paginas invertida
 */
void nextFrameValue(int *fr, int *off, int step_size);

#endif // _AUXILIARESMEMORIA_H_
