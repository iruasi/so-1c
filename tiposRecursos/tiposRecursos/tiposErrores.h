/* Este archivo solamente contiene definiciones que se pueden utilizar para identificar
 * los distintos tipos de errores que pueden suceder.
 */

#ifndef TIPOSERRORES_H_
#define TIPOSERRORES_H_

typedef enum {
  RES_INSUF=            -1
, NOSUCH_FILE=          -2
, NOREAD_PERM=          -3
, NOWRITE_PERM=         -4
, MEM_EXCEPTION=        -5
, KERN_CONS_EXIT=       -6
, CONS_PROG_EXIT=       -7
, MEM_OVERALLOC=        -8
, MEM_TOP_PAGES=        -9

// nuestros fallso
, UNDEFINED_ERR=       -20
, FALLO_GRAL=          -21
, FALLO_CONFIGURACION= -22
, FALLO_RECV=          -23
, FALLO_SEND=          -24
, FALLO_CONEXION=      -25
, FALLO_SELECT=        -26
, CONEX_INVAL=         -40
, FALLO_MATAR=         -41

, MEMORIA_LLENA=       -50
, FRAME_NOT_FOUND=     -51
, PID_INVALIDO=        -52
, FALLO_ESCRITURA=     -53
, MAX_CACHE_ENTRIES=   -54

, ABORTO_FILESYSTEM=   -98
, ABORTO_MEMORIA=      -99
} tErrores;

#endif /* TIPOSERRORES_H_ */

