#ifndef AUXILIARESKERNEL_H_
#define AUXILIARESKERNEL_H_

#include "../Compartidas/tiposPaquetes.h"
#include "../Compartidas/pcb.h"

/* Esta funcion queda expandida en varias responsabilidades, todas las cuales competen a iniciar un programa correctamente;
 * TODO: posiblemente sea bueno hacer el macino inverso, y separar las funciones en varias otras...
 * Lo que realiza es: recibe y serializa codigo fuente, con ese codigo fuente crea el pcb nuevo, envia el serializado a Memoria,
 * el PCB al Planificador, y a Consola deberia comunicar la creacion del proceso... (aunque esto lo puede hacer el Planificador para la cola de New)
 * Son muchas responsabilidades en una sola funcion, pero se puede delegar en otras funciones adentro (o fuera) de esta
 */
int procesarYCrearPrograma(tPackHeader *head, int fd_sender, int fd_mem, uint8_t stack_size, int frame_size);

/* Crea un puntero a un PCB nuevo, con un PID unico.
 */
tPCB *nuevoPCB(int cant_pags);

#endif // AUXILIARESKERNEL_H_
