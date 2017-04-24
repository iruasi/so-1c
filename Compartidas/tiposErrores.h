/* Este archivo solamente contiene definiciones que se pueden utilizar para identificar
 * los distintos tipos de errores que pueden suceder.
 */

#ifndef TIPOSERRORES_H_
#define TIPOSERRORES_H_

#define RES_INSUF      -1
#define NOSUCH_FILE    -2
#define NOREAD_PERM    -3
#define NOWRITE_PERM   -4
#define MEM_EXCEPTION  -5
#define KERN_CONS_EXIT -6
#define CONS_PROG_EXIT -7
#define MEM_OVERALLOC  -8
#define MEM_TOP_PAGES  -9
#define UNDEFINED_ERR  -20


#define FALLO_GRAL          -21
//#define FALLO_CONFIGURACION -22
#define FALLO_RECV          -23
#define FALLO_CONEXION      -24
#define FALLO_SELECT        -26
#define ABORTO_FILESYSTEM   -98
#define ABORTO_MEMORIA      -99


#endif /* TIPOSERRORES_H_ */

