#ifndef PCB_H_
#define PCB_H_

typedef struct {
	uint32_t id; // PID
	uint32_t pc; // Program Counter
}pcb;

typedef struct{
	uint32_t id;
	uint32_t pag;
	uint32_t offset;
	uint32_t size;
	uint32_t valor;
}variable;


#endif /* PCB_H_ */
