/* Este archivo solamente contiene definiciones que se pueden utilizar para identificar
 * los distintos tipos de errores que pueden suceder.
 */

#ifndef TIPOSERRORES_H_
#define TIPOSERRORES_H_

typedef enum {
  RESOURCE_INSUF=       -1
, NOSUCH_FILE=          -2
, NOREAD_PERM=          -3
, NOWRITE_PERM=         -4
, MEM_EXCEPTION=        -5
, CONS_DISCONNECT=      -6
, CONS_FIN_PROG=        -7
, MEM_OVERALLOC=        -8
, MEM_TOP_PAGES=        -9

// nuestros fallso
, NOCREAT_PERM=        -10

, UNDEFINED_ERR=       -20
, FALLO_GRAL=          -21
, FALLO_CONFIGURACION= -22
, FALLO_RECV=          -23
, FALLO_SEND=          -24
, FALLO_CONEXION=      -25
, FALLO_SELECT=        -26
, CONEX_INVAL=         -27
, FALLO_MATAR=         -28
, FALLO_HILO_JOIN=     -29
, FALLO_DESERIALIZAC=  -30
, FALLO_SERIALIZAC=    -31
, DESCONEXION_CPU=      -32

, FALLO_INSTR=         -40
, VAR_NOT_FOUND=       -41
, GLOBAL_NOT_FOUND=    -42
, PUNTERO_INVALIDO=    -43

, MEMORIA_LLENA=       -50
, FRAME_NOT_FOUND=     -51
, PID_INVALIDO=        -52
, FALLO_ESCRITURA=     -53
, MAX_CACHE_ENTRIES=   -54
, SIZE_INVALID=        -55
, FALLO_HEAP=          -56
, FALLO_ALMAC=         -57
, FALLO_ASIGN=         -58
, FALLO_SOLIC=         -59

,INVALIDAR_RESPUESTA=  -60

, ABORTO_PCB=          -95
, ABORTO_KERNEL=       -96
, ABORTO_CPU=          -97
, ABORTO_FILESYSTEM=   -98
, ABORTO_MEMORIA=      -99
} tErrores;

#endif /* TIPOSERRORES_H_ */

