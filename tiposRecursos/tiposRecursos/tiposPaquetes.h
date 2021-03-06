#ifndef TIPOSPAQUETES_H_
#define TIPOSPAQUETES_H_
#include <stdint.h>

#include <parser/parser.h>

#ifndef HEAD_SIZE
#define HEAD_SIZE 8 // size de la cabecera de cualquier packete (tipo de proceso y de mensaje)
#endif

typedef enum {KER= 1, CPU= 2, FS= 3, CON= 4, MEM= 5} tProceso;

typedef enum {
	HSHAKE      = 1,
	THREAD_INIT = 2, // para cada nuevo thread que se crea y entra a un manejador
	SRC_CODE    = 3,

	IMPRIMIR    = 4,

	KERINFO     = 11,

	// Mensajes para CPU send/recv...
	PCB_EXEC    = 21, // serian para Planificador
	PCB_PREEMPT = 22, // serian para Planificador
	PCB_SAVE    = PCB_PREEMPT, // serian para Memoria
	PCB_RESTORE = PCB_EXEC, // serian para Memoria
	INSTR       = 23,
	BYTES       = 24,
	SET_GLOBAL  = 25,
	GET_GLOBAL  = 26,
	PCB_BLOCK   = 27,
	NEW_QSLEEP  = 28,
	SIG1        = 29,
	S_WAIT      = 30,
	S_SIGNAL    = 31,
	LIBERAR     = 32,
	ABRIR       = 33,
	BORRAR      = 34,
	CERRAR      = 35,
	MOVERCURSOR = 36,
	ESCRIBIR    = 37,
	LEER        = 38,
	RESERVAR    = 39,
	ENTREGO_FD  = 40, //KERNEL - CPU

	// API Memoria & Mensaje que recibe/envia Memoria
	ASIGN_PAG   = 50,
	INI_PROG    = 51,
	FIN_PROG    = 52,
	SOLIC_BYTES = 53,
	ALMAC_BYTES = 54,
	MEMINFO     = 55,
	ASIGN_SUCCS = 56,
	DUMP_DISK   = 57,
	PID_LIST    = 58,

	// Mensajes que recibe/envia Consola
	PRINT       = 41,
	NEWPROG     = 42,
	PID         = 43,
	KILL_PID    = 44,

	//Agrego mensaje para tratar planificador todo: pensar si algunos son en realidad errores
	RECURSO_NO_DISPONIBLE = 60, //kernel - cpu y viceversa
	FIN_PROCESO           = 61,	//kernel - cpu y viceversa
	ABORTO_PROCESO        = 62, //kernel - cpu y viceversa

	VALIDAR_RESPUESTA = 80, //fs-kernel
	CREAR_ARCHIVO     = 81,//fs-kernel
	VALIDAR_ARCHIVO   = 82,
	ARCHIVO_CERRADO   = 90, //fs-kernel-cpu
	ARCHIVO_BORRADO   = 91,//fs-kernel-cpu
	CURSOR_MOVIDO     = 92,//fs-kernel-cpu
	ARCHIVO_ESCRITO   = 93,//fs-kernel-cpu
	ARCHIVO_LEIDO     = 94,//fs-kernel-cpu

	FIN         = 71
} tMensaje;

typedef struct {

	tProceso tipo_de_proceso;
	tMensaje tipo_de_mensaje;
} tPackHeader; // este tipo de struct no necesita serialazion

typedef struct {

	tPackHeader head;
	int pid;
	int pageCount;
} tPackPidPag; // este paquete se utiliza para enviar un pid y una cant de paginas

typedef struct {

	tPackHeader head;
	int val;
} tPackPID, tHShakeProcAProc, tPackVal,tPackExitCode;

typedef struct {
	tPackHeader head;
	int pid;
	int page;
	int offset;
	int size;
} tPackByteReq; // Solicitud de Bytes a Memoria

typedef struct {
	tPackHeader head;
	int pid;
	int page;
	int offset;
	int size;
	char *bytes;
} tPackByteAlmac; // Almacenamiento de Bytes a Memoria

typedef struct {

	tPackHeader head;
	int bytelen;
	char *bytes;
} tPackBytes, tPackNombreVar, tPackSrcCode;

typedef struct {

	tPackHeader head;
	t_valor_variable val;
	t_nombre_compartida nom;
} tPackValComp; // para envio de variables globales entre CPU y Kernel

typedef struct {

	tPackHeader head;
	int val1;
	int val2;
} tHShake2ProcAProc;

//t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio,

typedef struct {

	t_descriptor_archivo fd;
	int size;
} tPackLeer;

typedef struct {

	t_descriptor_archivo fd;
	void *info;
	t_valor_variable tamanio;
} __attribute__((packed))tPackRW; //Leer y escribir tienen los mismos campos, uso las mismas estructuras

typedef struct{
	int longitudDireccion;
	t_direccion_archivo direccion;
	t_banderas flags;
}__attribute((packed))tPackAbrir;

typedef struct{
	t_descriptor_archivo fd;
	int *cantidadOpen;
}__attribute__((packed))tPackFS; //Sirve para mandar el fd a la CPU

typedef struct{
	int dirSize;
	char * direccion;
	void * info;
	int tamanio;
	t_puntero cursor;
}__attribute__((packed))tPackRecibirRW;//Paquete para recibir la serealizacion de lectura y escritura de kernel


typedef struct{
	int dirSize;
	char * direccion;
	t_puntero cursor;
	int size;
	t_banderas flag;
}__attribute__((packed))tPackRecvRW;//Paquete para recibir la serealizacion de lectura y escritura de kernel

typedef struct{
	t_descriptor_archivo fd;
	t_valor_variable posicion;
}__attribute__((packed))tPackCursor;

typedef struct {
	t_direccion_archivo path;
	t_puntero cursor;
	void *info;
	int *size;
} tPackEscribir;


#endif /* TIPOSPAQUETES_H_ */
