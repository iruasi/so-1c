#ifndef PCB_H_
#define PCB_H_

typedef struct {
	uint32_t id; // PID
	uint32_t pc; // Program Counter
	uint32_t refTablaProc; //Referencia a la tabla de Archivos del Proceso
	uint32_t posStack; //Posicion del Stack
	uint32_t exitCode; // Exit Code
}pcb;

#endif /* PCB_H_ */
