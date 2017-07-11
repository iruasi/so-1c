#ifndef DEFSKERNEL_H_
#define DEFSKERNEL_H_

#ifndef PID_IDLE
#define PID_IDLE -1
#endif

#ifdef SIZEOF_HMD
#undef SIZEOF_HDM
#define SIZEOF_HMD 5
#endif

#ifdef HEAD_SIZE
#undef HEAD_SIZE
#define HEAD_SIZE 8
#endif

#ifndef BACKLOG
#define BACKLOG 20
#endif

#define MAXPID_DIG 6 // maxima cantidad de digitos que puede tener un PID

/* MAX(A, B) es un macro que devuelve el mayor entre dos argumentos,
 * lo usamos para actualizar el maximo socket existente, a medida que se crean otros nuevos
 */
#ifndef MAX_AB
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

#endif /* DEFSKERNEL_H_ */
