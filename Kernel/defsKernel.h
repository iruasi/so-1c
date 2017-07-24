#ifndef DEFSKERNEL_H_
#define DEFSKERNEL_H_

#include <sys/select.h>

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

#define MAXPID_DIG 6 // maxima cantidad de digitos que puede tener un PID

/* MAX(A, B) es un macro que devuelve el mayor entre dos argumentos,
 * lo usamos para actualizar el maximo socket existente, a medida que se crean otros nuevos
 */
#ifndef MAX_AB
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

#ifndef MUX_LOCK_M
#define MUX_LOCK(M) (pthread_mutex_lock(M))
#endif
#ifndef MUX_UNLOCK_M
#define MUX_UNLOCK(M) (pthread_mutex_unlock(M))
#endif

// El tamaño de un evento es igual al tamaño de la estructura de inotify
// mas el tamaño maximo de nombre de archivo que nosotros soportemos
// en este caso el tamaño de nombre maximo que vamos a manejar es de 24
// caracteres. Esto es porque la estructura inotify_event tiene un array
// sin dimension ( Ver C-Talks I - ANSI C ).
#define EVENT_SIZE  ( sizeof (struct inotify_event) + 24 )

// El tamaño del buffer es igual a la cantidad maxima de eventos simultaneos
// que quiero manejar por el tamaño de cada uno de los eventos. En este caso
// Puedo manejar hasta 1024 eventos simultaneos.
#define BUF_LEN     ( 128 * EVENT_SIZE )


typedef struct {
	int sock_lis_con,
		sock_lis_cpu,
		sock_inotify,
		sock_watch,
		fd_max;
	fd_set master;
} ker_socks;

/* Llama a los setups de cada archivo que inicialice variables globales,
 * semaforos y mutexes.
 * Es util para controlar un poco el scope de las variables globales.
 */
void setupVariablesPorSubsistema(void);

#endif /* DEFSKERNEL_H_ */
