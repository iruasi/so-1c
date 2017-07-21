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

	ENTREGO_FD = 40, //KERNEL - CPU
	// API Memoria & Mensaje que recibe/envia Memoria
	ASIGN_PAG   = 50,
	INI_PROG    = 51,
	FIN_PROG    = 52,
	SOLIC_BYTES = 53,
	ALMAC_BYTES = 54,
	MEMINFO     = 55,
	ASIGN_SUCCS = 56,

	// Mensajes que recibe/envia Consola
	PRINT       = 41,
	NEWPROG     = 42,
	PID         = 43,
	KILL_PID    = 44,
	KER_KILLED  = KILL_PID,

	//Agrego mensaje para tratar planificador todo: pensar si algunos son en realidad errores
	RECURSO_NO_DISPONIBLE = 60, //kernel - cpu y viceversa
	FIN_PROCESO = 61,			//kernel - cpu y viceversa
	ABORTO_PROCESO = 62,		//kernel - cpu y viceversa
	//RR
	FIN_QUAMTUM = 63,			//kernel - cpu y viceversa

	//CPU_MANEJADOR en kernel.c

	SYSCALL = 70, //cpu-kernel

	FIN         = 11
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
} tPackPID, tHShakeProcAProc, tPackVal;

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
	int marcos;
	int marco_size;
} tHShakeMemAKer;

//t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio,

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
	int tamanio_direccion;
	t_direccion_archivo direccion;
	t_valor_variable tamanio;
	void*info;
	t_banderas flags;
}__attribute__((packed)) tPackLE;//Esta estructura sirve para mandar solicitudes de lectura y escritura al fs

#endif /* TIPOSPAQUETES_H_ */
