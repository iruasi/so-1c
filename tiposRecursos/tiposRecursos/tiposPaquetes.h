#ifndef TIPOSPAQUETES_H_
#define TIPOSPAQUETES_H_
#include <stdint.h>

#define HEAD_SIZE 8 // size de la cabecera de cualquier packete (tipo de proceso y de mensaje)

typedef enum {KER= 1, CPU= 2, FS= 3, CON= 4, MEM= 5} tProceso;

typedef enum {
	HSHAKE      = 1,
	SRC_CODE    = 3,

	// Mensajes para CPU send/recv...
	PCB_EXEC    = 21, // serian para Planificador
	PCB_PREEMPT = 22, // serian para Planificador
	PCB_SAVE    = PCB_PREEMPT, // serian para Memoria
	PCB_RESTORE = PCB_EXEC, // serian para Memoria
	INSTR       = 23,
	BYTES       = INSTR,

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

	//Agrego mensaje para tratar planificador
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
	unsigned long sourceLen;
	char *sourceCode;
} tPackSrcCode; // este paquete se utiliza para enviar codigo fuente

typedef struct {

	tPackHeader head;
	int pid;
	int pageCount;
} tPackPidPag; // este paquete se utiliza para enviar un pid y una cant de paginas

typedef struct {

	tPackHeader head;
	int pid;
} tPackPID;

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
} tPackBytes; // este paquete se utiliza para responder una Solicitud de Bytes

typedef struct {
	tPackHeader head;
	int pid,
		pc,
		pages,
		exit;
} tPackPCBSimul; /* este paquete simula ser un PCB, todavia no aplica todos los campos;
                  * TODO: luego sera el tPackPCB para el proyecto completo */

typedef struct {

	tPackHeader head;
	int marcos;
	int marco_size;
} tHShakeMemAKer;

#endif /* TIPOSPAQUETES_H_ */
