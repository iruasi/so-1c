#ifndef DEFSKERNEL_H_
#define DEFSKERNEL_H_

#ifndef PID_IDLE
#define PID_IDLE -1
#endif

#ifndef SIZEOF_HMD
#define SIZEOF_HMD 5
#endif

#ifndef HEAD_SIZE
#define HEAD_SIZE 8
#endif

#ifndef BACKLOG
#define BACKLOG 20
#endif

/* MAX(A, B) es un macro que devuelve el mayor entre dos argumentos,
 * lo usamos para actualizar el maximo socket existente, a medida que se crean otros nuevos
 */
#ifndef MAX_AB
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

#endif /* DEFSKERNEL_H_ */
