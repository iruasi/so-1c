#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <pthread.h>
#include <math.h>

#include <commons/collections/queue.h>
#include <parser/metadata_program.h>
#include <parser/parser.h>
#include <commons/collections/list.h>

#include "defsKernel.h"
#include "kernelConfigurators.h"
#include "auxiliaresKernel.h"
#include "planificador.h"
#include "funcionesSyscalls.h"
#include "capaMemoria.h"

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>
#define MAXOPCION 200
#define VALIDAR_ARCHIVO 100
#ifndef HEAD_SIZE
#define HEAD_SIZE 8
#endif

//extern t_dictionary *heapDict;
extern sem_t sem_heapDict;
extern sem_t sem_bytes;
extern sem_t sem_end_exec;
//extern sem_t haySTDIN;

extern sem_t eventoPlani;
int globalPID;
int globalFD;

t_dictionary *dict_proc_info;
t_list *gl_Programas; // va a almacenar relaciones entre Programas y Codigo Fuente
t_list *listaDeCpu;
t_list *finalizadosPorConsolas;

extern t_queue *New, *Exit,*Ready;
extern t_list	*Exec, *Block;
extern tKernel *kernel;
extern int grado_mult;

bool estaEnExit(int pid);

// todo: poner TOOODAS las variables globales en defsKernel.h... yafu
// bueno no, pero un setup por archivo, y se llama a cada setup desde defsKernel.
extern t_dictionary * tablaGlobal;
extern t_dictionary *dict_sems_queue;


/* Este procedimiento inicializa las variables y listas globales.
 */


typedef struct{
	t_banderas flag;
	t_descriptor_archivo fd;
	t_valor_variable posicionCursor;
}tProcesoArchivo;

typedef struct{
	int pid;
	t_list * archivosPorProceso; //Esta lista va a contener tProcesosPorArchivo
}t_procesoXarchivo;


extern t_list * tablaProcesos;

extern int sock_fs;

typedef struct{
	t_direccion_archivo direccion;
	int cantidadOpen;
	t_descriptor_archivo fd;
}tDatosTablaGlobal;

int globalFD;

void agregarArchivoTablaGlobal(tDatosTablaGlobal * datos,tPackAbrir * abrir){
	char fd_str[MAXPID_DIG];
	sprintf(fd_str, "%d", datos->fd);

	memcpy(datos->direccion, abrir->direccion, abrir->longitudDireccion);
	datos->cantidadOpen = 0;
	datos->fd = globalFD; globalFD++;

	if(!dictionary_has_key(tablaGlobal, fd_str)){
		printf("La tabla no contiene el archivo, la agregamos\n");
		dictionary_put(tablaGlobal, fd_str, datos);
		printf("Los datos del fd #%d fueron agregados a la tabla global \n",datos->fd);
	}else{
		printf("El archivo ya se encuentra en la tabla global\n");
	}
}
void agregarArchivoATablaProcesos(tDatosTablaGlobal *datos,t_banderas flags, int pid){
	tProcesoArchivo * pa = malloc(sizeof *pa);

	t_procesoXarchivo * pxa = malloc(sizeof(*pxa));

	pa->fd = datos->fd;
	pa->flag = flags;
	pa->posicionCursor = 0;

	pxa -> pid = pid;
	pxa->archivosPorProceso = list_create();
	list_add(pxa->archivosPorProceso,pa);//El index es 3 + el pid, porque 0,1 y 2 están reservados
	list_add(tablaProcesos,pxa);
}


tProcesoArchivo * obtenerProcesoSegunFD(t_descriptor_archivo fd , int pid){
	bool encontrarFD(tProcesoArchivo * archivo){
		return archivo->fd == fd;
	}
	bool encontrarPid(t_procesoXarchivo * proceso){
		return proceso->pid == pid;
	}
	t_procesoXarchivo * _unProceso = (t_procesoXarchivo *)list_find(tablaProcesos,encontrarPid);
	tProcesoArchivo * _unArchivo = (tProcesoArchivo *) list_find(_unProceso->archivosPorProceso,encontrarFD);

	if(_unArchivo == NULL){
		perror("No hay archivo");
	}

	return _unArchivo;
}
tDatosTablaGlobal * encontrarTablaPorFD(t_descriptor_archivo fd, int pid){
	tDatosTablaGlobal * unaTabla;
	bool encontrarFD(tProcesoArchivo *archivo){
		return archivo->fd == fd;
	}
	bool encontrarProceso(t_procesoXarchivo * unProceso){
		return unProceso->pid == pid;
	}

	t_procesoXarchivo * _proceso  = list_find(tablaProcesos, encontrarProceso);
	tProcesoArchivo * _archivo = (tProcesoArchivo *) list_find(_proceso->archivosPorProceso, encontrarFD);

	if(_archivo->fd == fd) unaTabla = dictionary_get(tablaGlobal,(char *)&fd);

	return unaTabla;
}
/* Mutexes de cosas varias*/
void setupMutexes(){
	pthread_mutex_init(&mux_sems_queue,    NULL);
	pthread_mutex_init(&mux_listaDeCPU,    NULL);
	pthread_mutex_init(&mux_gl_Programas,  NULL);
}

void setupVariablesGlobales(void){

	dict_sems_queue = dictionary_create();
	dict_proc_info  = dictionary_create();
	gl_Programas = list_create();
	finalizadosPorConsolas = list_create();

}

tPCB *crearPCBInicial(void){

	tPCB *pcb;
	if ((pcb = malloc(sizeof *pcb)) == NULL){
		printf("Fallo mallocar %d bytes para pcbInicial\n", sizeof *pcb);
		return NULL;
	}
	pcb->indiceDeCodigo    = NULL;
	pcb->indiceDeStack     = list_create();
	pcb->indiceDeEtiquetas = NULL;

	pcb->id = globalPID;
	globalPID++;
	pcb->proxima_rafaga     = 0;
	pcb->estado_proc        = 0;
	pcb->contextoActual     = 0;
	pcb->exitCode           = 0;
	pcb->rafagasEjecutadas  = 0;

	return pcb;
}

void cpu_manejador(void *infoCPU){

	t_RelCC *cpu_i = (t_RelCC *) infoCPU;
	cpu_i->con->pid=cpu_i->con->fd_con=-1;
	printf("cpu_manejador socket %d\n", cpu_i->cpu.fd_cpu);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = THREAD_INIT};

	bool found;
	char *buffer, *var, *sfd;
	char *file_serial, leer_serial;
	int stat, pack_size, p;
	tPackBytes *sem_bytes;
	tPackVal *alloc, *fd_rta;
	t_puntero ptr;

	do {
	printf("(CPU) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);

	switch((int) head.tipo_de_mensaje){
	case S_WAIT:
		puts("Signal wait a semaforo");

		if ((buffer = recvGeneric(cpu_i->cpu.fd_cpu)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_GRAL;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		if ((sem_bytes = deserializeBytes(buffer)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_DESERIALIZAC;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		head.tipo_de_proceso = KER;
		head.tipo_de_mensaje = waitSyscall(sem_bytes->bytes, cpu_i->cpu.pid);
		informarResultado(cpu_i->cpu.fd_cpu, head);

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &sem_bytes);
		break;

	case S_SIGNAL:
		puts("Signal continuar a semaforo");

		if ((buffer = recvGeneric(cpu_i->cpu.fd_cpu)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_GRAL;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		if ((sem_bytes = deserializeBytes(buffer)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_DESERIALIZAC;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		head.tipo_de_proceso = KER;
		head.tipo_de_mensaje = (signalSyscall(sem_bytes->bytes) == -1)?
				VAR_NOT_FOUND : S_SIGNAL;
		informarResultado(cpu_i->cpu.fd_cpu, head);

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &sem_bytes);
		break;

	case SET_GLOBAL:
		puts("Se reasigna una variable global");

		if ((buffer = recvGeneric(cpu_i->cpu.fd_cpu)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_GRAL;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		tPackValComp *val_comp;
		if ((val_comp = deserializeValorYVariable(buffer)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_DESERIALIZAC;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		head.tipo_de_proceso = KER;
		head.tipo_de_mensaje = ((stat = setGlobalSyscall(val_comp)) != 0)?
				stat : SET_GLOBAL;
		informarResultado(cpu_i->cpu.fd_cpu, head);

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &val_comp);
		break;

	case GET_GLOBAL:
		puts("Se pide el valor de una variable global");
		t_valor_variable val;
		tPackBytes *var_name;

		if ((buffer = recvGeneric(cpu_i->cpu.fd_cpu)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_GRAL;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		if ((var_name = deserializeBytes(buffer)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_DESERIALIZAC;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}
		freeAndNULL((void **) &buffer);

		if ((var = malloc(var_name->bytelen)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_GRAL;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}
		memcpy(var, var_name->bytes, var_name->bytelen);

		val = getGlobalSyscall(var, &found);
		if (!found){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = GLOBAL_NOT_FOUND;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}
		head.tipo_de_proceso = KER; head.tipo_de_mensaje = GET_GLOBAL;

		if ((buffer = serializeValorYVariable(head, val, var, &pack_size)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_SERIALIZAC;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		if ((stat = send(cpu_i->cpu.fd_cpu, buffer, pack_size, 0)) == -1){
			perror("Fallo send variable global a CPU. error");
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_SEND;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &var);
		freeAndNULL((void **) &var_name->bytes); freeAndNULL((void **) &var_name);
		break;

	case RESERVAR:
		puts("Funcion reservar");

		if ((buffer = recvGeneric(cpu_i->cpu.fd_cpu)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_GRAL;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		if ((alloc = deserializeVal(buffer)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_DESERIALIZAC;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}
		freeAndNULL((void **) &buffer);

		if ((ptr = reservar(cpu_i->cpu.pid, alloc->val)) == 0){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_HEAP;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		alloc->head.tipo_de_proceso = KER; alloc->head.tipo_de_mensaje = RESERVAR;
		alloc->val = ptr;
		pack_size = 0;
		if ((buffer = serializeVal(alloc, &pack_size)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_SERIALIZAC;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		if ((stat = send(cpu_i->cpu.fd_cpu, buffer, pack_size, 0)) == -1){
			perror("Fallo send de puntero alojado a CPU. error");
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_SEND;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &alloc);
		break;

	case LIBERAR:
		puts("Funcion liberar");
		if ((buffer = recvGeneric(cpu_i->cpu.fd_cpu)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_GRAL;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}

		if ((alloc = deserializeVal(buffer)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_DESERIALIZAC;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}
		freeAndNULL((void **) &buffer);

		head.tipo_de_proceso = KER;
		head.tipo_de_mensaje = ((stat = liberar(cpu_i->cpu.pid, alloc->val)) < 0)?
				stat : LIBERAR;
		informarResultado(cpu_i->cpu.fd_cpu, head);

		puts("Fin case LIBERAR");
		break;

	case ABRIR:
		puts("CPU quiere abrir un archivo");

			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackAbrir * abrir = deserializeAbrir(buffer);
			freeAndNULL((void **) &buffer);

			head.tipo_de_proceso = KER; head.tipo_de_mensaje = VALIDAR_ARCHIVO;
			buffer = serializeBytes(head, abrir->direccion, abrir->longitudDireccion, &pack_size);

			printf("La direccion es %s\n", (char *) abrir->direccion);
			if ((stat = send(sock_fs, buffer, pack_size, 0)) < 0){
				perror("No se pudo validar el archivo. error");
				head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_SEND;
				informarResultado(cpu_i->cpu.fd_cpu, head); // como fallo ejecucion, avisamos a CPU
				break;
			}
			freeAndNULL((void **) buffer);

			tDatosTablaGlobal * datosGlobal = malloc(sizeof *datosGlobal);
			datosGlobal->direccion = malloc(abrir->longitudDireccion);

			pack_size = 0;
			file_serial = serializeAbrir(abrir, &pack_size);

			/*if((stat = recv(sock_fs,&head,HEAD_SIZE,0))<0){
				perror("Error al recivir respuesta del fs");
			}*/
			if(false){//Agregar validar_archivo a tiposPaqueteshead.tipo_de_mensaje == VALIDAR_ARCHIVO
				printf("El archivo existe, ahora verificamos si la contiene la tabla Global \n");

				agregarArchivoTablaGlobal(datosGlobal,abrir);
				agregarArchivoATablaProcesos(datosGlobal,abrir->flags,cpu_i->con->pid);


			} else if(abrir->flags.creacion){
				if ((stat = send(sock_fs,file_serial,pack_size,0))< 0){
					perror("No se pudo validar el archivo\n");
				}
				/*if((stat = recv(sock_fs,&head,HEAD_SIZE,0))<0){
					perror("Error al recivir respuesta del fs");
				}*/
				if(true){//head.tipo_de_mensaje == ARCHIVO_CREADO
					agregarArchivoTablaGlobal(datosGlobal,abrir);
					agregarArchivoATablaProcesos(datosGlobal,abrir->flags,cpu_i->con->pid);
					printf("Agregamos el archivo a la tabla de procesos \n");

				}
			}

			fd_rta->head.tipo_de_proceso = KER; fd_rta->head.tipo_de_mensaje = ENTREGO_FD;
			fd_rta->val = datosGlobal->fd;
			file_serial = serializeVal(fd_rta, &pack_size);
			if((stat = send(cpu_i->cpu.fd_cpu, file_serial, pack_size, 0)) == -1){
				perror("error al enviar el paquete a la cpu. error");
				break;
			}

			free(abrir->direccion); freeAndNULL((void **) &abrir);
			puts("Fin case ABRIR");
			break;
		case BORRAR:
			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackBytes * borrar_fd =  deserializeBytes(buffer);
			tDatosTablaGlobal * unaTabla;

			unaTabla = encontrarTablaPorFD(*((int *) borrar_fd->bytes),cpu_i->con->pid);
			if(unaTabla->cantidadOpen <= 0){
				perror("El archivo no se encuentra abierto");
				break;
			}else if(unaTabla->cantidadOpen > 1){
				perror("El archivo solicitado esta abierto más de 1 vez al mismo tiempo");
				break;
			}

			tPackHeader header = {.tipo_de_proceso = FS, .tipo_de_mensaje = BORRAR};
			char * borrar_serial = serializeBytes(header,borrar_fd->bytes,sizeof(int),&pack_size);

			if((stat = send(sock_fs,borrar_serial,pack_size,0)) == -1){
				perror("error al mandar petición de borrado de archivo al filesystem");
				break;
			}

			if((stat = recv(sock_fs, &head, sizeof head, 0)) == -1){
				perror("error al recibir el paquete al filesystem");
				break;
			}
			if(head.tipo_de_mensaje == 1){//TODO: CAMBIAR ESTE 1 POR EL PROTOCOLO CORRESPONDIENTE
				buffer = recvGeneric(sock_fs);
				if((stat = send(cpu_i->cpu.fd_cpu,buffer,pack_size,0)) == -1){
					perror("error al enviar el paquete al filesystem");
					break;
				}
			}

			freeAndNULL((void ** )&buffer);
			break;
		case CERRAR:
			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackBytes * cerrar_fd =  deserializeBytes(buffer);
			tDatosTablaGlobal * archivoCerrado = encontrarTablaPorFD(*((int *)cerrar_fd),cpu_i->con->pid);

			archivoCerrado -> cantidadOpen--;

			printf("Se cerró el archivo de fd #%d y de direccion %s",*((int *)cerrar_fd),(char *) &(archivoCerrado-> direccion));
			tPackHeader header2 = {.tipo_de_proceso = KER, .tipo_de_mensaje = 120}; //ARCHIVO_CERRADO = 120
			pack_size = 0;
			char * cerrar_serial = serializeHeader(header2,&pack_size);

			if((stat = send(cpu_i->cpu.fd_cpu,cerrar_serial,pack_size,0))<0){
				perror("error al enviar mensaje de cerrado a la cpu");
				break;
			}

			freeAndNULL((void ** )&buffer);

			break;

		case MOVERCURSOR:
			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			typedef struct{
				t_descriptor_archivo fd;
				t_valor_variable posicion;
			}__attribute__((packed))tPackCursor;

			tPackCursor * deserializeCursor(char * cursor_serial){

				tPackCursor * cursor = malloc(sizeof(*cursor));
				int off = 0;

				memcpy(&cursor->fd,cursor_serial+off,sizeof(int));
				off += sizeof(int);
				memcpy(&cursor->posicion,cursor_serial + off ,sizeof(int));
				off += sizeof(int);

				return cursor;

			}

			tPackCursor * cursor = deserializeCursor(buffer);
			tProcesoArchivo * _unArchivo = obtenerProcesoSegunFD(cursor->fd,cpu_i->cpu.pid);
			_unArchivo->posicionCursor = cursor->posicion;
			pack_size = 0;
			tPackHeader header3 = {.tipo_de_proceso = KER, .tipo_de_mensaje = 130}; // 130 = CURSOR_MOVIDO
			char * cursor_serial = serializeBytes(header3,_unArchivo->posicionCursor,sizeof(_unArchivo->posicionCursor),&pack_size);


			if((stat = send(cpu_i->cpu.fd_cpu,cursor_serial,pack_size,0))<0){
				perror("error al enviar el cambio de cursor a la cpu");
			}

			break;
		case ESCRIBIR:

			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackRW *escr = deserializeEscribir(buffer);

			printf("Se escribe en fd %d, la info %s\n", escr->fd, (char *) escr->info);
			printf("Se recibió el fd #%d\n", escr->fd);

			sfd[MAXPID_DIG];
			sprintf(sfd, "%d", (int) escr->fd);

			tDatosTablaGlobal * path =  dictionary_get(tablaGlobal, sfd);
			tProcesoArchivo * banderas = obtenerProcesoSegunFD(escr->fd, cpu_i->con->pid);

			printf("El path del direcctorio elegido es: %s\n", (char *) path->direccion);

			file_serial = serializeLeerFS(path->direccion, escr->info, escr->tamanio, banderas->flag, &pack_size);
			if((stat = send(sock_fs, file_serial, pack_size, 0)) == -1){
				perror("error al enviar el paquete al filesystem");
				break;
			}

		/*	if((stat = recv(sock_fs, &head, sizeof head, 0)) == -1){
				perror("error al recibir el paquete al filesystem");
				break;
			}
			if(true){//TODO: CAMBIAR ESTE 1 POR EL PROTOCOLO CORRESPONDIENTE head.tipo_de_mensaje == 1
				buffer = recvGeneric(sock_fs);
				if((stat = send(cpu_i->cpu.fd_cpu,buffer,pack_size,0)) == -1){
					perror("error al enviar el paquete al filesystem");
					break;
				}else{
					//Finalizar programa (hay alguna función así)?
					break;
				}
			}
			*/

			free(escr->info); free(escr);
			freeAndNULL((void **) &buffer);
			break;

		case LEER:
			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackRW * leer = deserializeLeer(buffer);
			path =  dictionary_get(tablaGlobal,(char *) &leer->fd);
			printf("El valor del fd en leer es %d \n", leer->fd);

			banderas = obtenerProcesoSegunFD(leer->fd,cpu_i->con->pid);
			printf("El path del direcctorio elegido es: %s\n", (char *) path->direccion);

			file_serial = serializeLeerFS(path->direccion,leer->info,leer->tamanio,banderas->flag,&pack_size);
			if((stat = send(sock_fs,file_serial,pack_size,0)) == -1){
				perror("error al enviar el paquete al filesystem");
				break;
			}

			/*if((stat = recv(sock_fs, &head, sizeof head, 0)) == -1){
				perror("error al recibir el paquete al filesystem");
				break;
			}
			if(head.tipo_de_mensaje == 1){//TODO: CAMBIAR ESTE 1 POR EL PROTOCOLO CORRESPONDIENTE
				buffer = recvGeneric(sock_fs);
				if((stat = send(cpu_i->cpu.fd_cpu,buffer,pack_size,0)) == -1){
					perror("error al enviar el paquete al filesystem");
					break;
				}
			}*/
			free(leer->info); free(leer);
			freeAndNULL((void **)&buffer);
			break;

	case(FIN_PROCESO): case(ABORTO_PROCESO): case(RECURSO_NO_DISPONIBLE):
			case(PCB_PREEMPT): case(PCB_BLOCK): //COLA EXIT o BLOCK
		cpu_i->msj = head.tipo_de_mensaje;
		cpu_handler_planificador(cpu_i);
	break;

	case HSHAKE:
		puts("Se recibe handshake de CPU");

		head.tipo_de_proceso = KER; head.tipo_de_mensaje = KERINFO;
		if ((stat = contestar2ProcAProc(head, kernel->quantum_sleep, kernel->stack_size, cpu_i->cpu.fd_cpu)) < 0){
			puts("No se pudo informar el quantum_sleep y stack_size a CPU.");
			return;
		}

		pthread_mutex_lock(&mux_listaDeCPU);
		list_add(listaDeCpu, cpu_i);
		pthread_mutex_unlock(&mux_listaDeCPU);
		sem_post(&eventoPlani);
		puts("Fin case HSHAKE.");
		break;

	case THREAD_INIT:
		puts("Se inicia thread en handler de CPU");
		puts("Fin case THREAD_INIT.");
		break;

	default:
		puts("Funcion no reconocida!");
		break;

	}} while((stat = recv(cpu_i->cpu.fd_cpu, &head, sizeof head, 0)) > 0);

	if (stat == -1){
		perror("Error de recepcion de CPU. error");
		return;
	}

	puts("CPU se desconecto, la sacamos de la listaDeCpu..");

	if(cpu_i->con->pid > -1){ //esta cpu tenia asignado un proceso.

		desconexionCpu(cpu_i);//en esta funcion ponemos el pcb mas actual en exit y avisamos a consola el fin..
	}

	pthread_mutex_lock(&mux_listaDeCPU);
	if ((p = getCPUPosByFD(cpu_i->cpu.fd_cpu, listaDeCpu)) != -1){
		list_remove(listaDeCpu,p);
	}
	pthread_mutex_unlock(&mux_listaDeCPU);

	liberarCC(cpu_i);
}

void desconexionCpu(t_RelCC *cpu_i){

	tPCB *pcbAuxiliar;
	int p;


	printf("La cpu que se desconecto, estaba ejecutando el proceso %d\n",cpu_i->con->pid);

	pthread_mutex_lock(&mux_exec);

	if ((p = getPCBPositionByPid(cpu_i->con->pid, Exec)) != -1){

		pcbAuxiliar = list_get(Exec,p);
		pcbAuxiliar->exitCode=DESCONEXION_CPU;

		list_remove(Exec,p);

		encolarEnExit(pcbAuxiliar,cpu_i);
	}

	pthread_mutex_unlock(&mux_exec);

}

void liberarCC(t_RelCC *cc){
	free(cc->con);
	free(cc);
}

int getConPosByFD(int fd, t_list *list){

	int i;
	t_RelPF *pf;
	for (i = 0; i < list_size(list); ++i){
		pf = list_get(list, i);
		if (pf->prog->con->fd_con == fd)
			return i;
	}

	printf("No se encontro el programa de socket %d en la gl_Programas\n", fd);
	return -1;
}

int getCPUPosByFD(int fd, t_list *list){

	int i;
	t_RelCC *cc;
	for (i = 0; i < list_size(list); ++i){
		cc = list_get(list, i);
		if (cc->cpu.fd_cpu == fd)
			return i;
	}

	printf("No se encontro el CPU de socket %d en la listaDeCpu\n", fd);
	return -1;
}

void mem_manejador(void *m_sock){
	int *sock_mem = (int*) m_sock;
	printf("mem_manejador socket %d\n", *sock_mem);

	int stat;
	char *buffer;
	tPackHeader head = {.tipo_de_proceso = MEM, .tipo_de_mensaje = THREAD_INIT};

	do {
	switch((int) head.tipo_de_mensaje){
	printf("(MEM) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);

	case ASIGN_SUCCS : case FALLO_ASIGN:
		puts("Se recibe respuesta de asignacion de paginas para algun proceso");
		sem_post(&sem_heapDict);
		sem_wait(&sem_end_exec);
		puts("Fin case ASIGN_SUCCESS_OR_FAIL");
		break;

	case BYTES:
		puts("Se reciben bytes desde Memoria");
		sem_post(&sem_bytes);
		sem_wait(&sem_end_exec);
		puts("Fin case BYTES");
		break;

	case THREAD_INIT:
		puts("Se inicia thread en handler de Memoria");
		puts("Fin case THREAD_INIT");
		break;

	//case DUMP_DISK: // todo: agregar /dmp a FS...
	case -97:
		puts("Memoria dumpea informacion en /dmp");
		break;

	case PID_LIST:
		puts("Memoria pide lista de procesos activos");

		int len, pack_size;
		int *pids = formarPtrPIDs(&len);

		head.tipo_de_proceso = KER; head.tipo_de_mensaje = PID_LIST;
		pack_size = 0;
		if ((buffer = serializeBytes(head, (char *) pids, len, &pack_size)) == NULL){
			head.tipo_de_mensaje = FALLO_SERIALIZAC;
			informarResultado(*sock_mem, head);
			break;
		}

		if ((stat = send(*sock_mem, buffer, pack_size, 0)) == -1)
			perror("No se pudo mandar lista de PIDs a Memoria. error");
		printf("Se enviaron %d bytes a Memoria\n", stat);

		break;

	default:
		puts("Se recibe un mensaje de Memoria no considerado");
		break;

	}} while ((stat = recv(*sock_mem, &head, HEAD_SIZE, 0)) > 0);
}

void cons_manejador(void *conInfo){
	t_RelCC *con_i = (t_RelCC*) conInfo;
	printf("cons_manejador socket %d\n", con_i->con->fd_con);

	int stat;
	tPackHeader head = {.tipo_de_proceso = CON, .tipo_de_mensaje = THREAD_INIT};
	char *buffer;
	tPackSrcCode *entradaPrograma;
	tPackPID *ppid;
	int pidAFinalizar;

	do {
	switch(head.tipo_de_mensaje){
	printf("(CON) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);

	case SRC_CODE:
		puts("Consola quiere iniciar un programa");

		if ((buffer = recvGeneric(con_i->con->fd_con)) == NULL){
			puts("Fallo recepcion de SRC_CODE");
			return;
		}

		if ((entradaPrograma = (tPackSrcCode *) deserializeBytes(buffer)) == NULL){
			puts("Fallo deserializacion de Bytes");
			return;
		}

		tPCB *new_pcb = crearPCBInicial();
		con_i->con->pid = new_pcb->id;
		asociarSrcAProg(con_i, entradaPrograma);

		printf("El size del paquete %d\n", strlen(entradaPrograma->bytes) + 1);

		encolarEnNew(new_pcb);

		freeAndNULL((void **) &buffer);
		puts("Fin case SRC_CODE.");
		break;

	case THREAD_INIT:
		puts("Se inicia thread en handler de Consola");
		puts("Fin case THREAD_INIT.");
		break;

	case HSHAKE:
		puts("Es solo un handshake");
		break;

	case KILL_PID:

		if ((buffer = recvGeneric(con_i->con->fd_con)) == NULL){
			//log_error(logger,"error al recibir el pid");
			puts("error al recibir el pid");
			return;
		}

		if ((ppid = deserializeVal(buffer)) == NULL){
			//log_error(logger,"error al deserializar el packPID");
			puts("Error al deserializar el PACKPID");
			return;
		}

		//log_trace(logTrace,"asigno pid a la estructura");
		pidAFinalizar = ppid->val;
		//freeAndNULL((void **)&ppid);
		printf("Pid a finalizar: %d\n",pidAFinalizar);
		t_finConsola *fc=malloc(sizeof(fc));
		fc->pid = pidAFinalizar ;
		fc->ecode = CONS_FIN_PROG;

		pthread_mutex_lock(&mux_listaFinalizados);
		list_add(finalizadosPorConsolas,fc);
		pthread_mutex_unlock(&mux_listaFinalizados);

		break;

	default:
		puts("Se recibe un mensaje no reconocido!");
		break;

	}} while ((stat = recv(con_i->con->fd_con, &head, HEAD_SIZE, 0)) > 0);

	if(con_i->con->fd_con != -1){
	pthread_mutex_lock(&mux_exec);
		if(!estaEnExit(con_i->con->pid)){//el programa no esta en la lista de exit, osea sigue en ejecucion
			printf("La consola %d asociada al PID: %d se desconecto.\n", con_i->con->fd_con, con_i->con->pid);

			t_finConsola *fc=malloc(sizeof(fc));
			fc->pid = con_i->con->pid ;
			fc->ecode = CONS_DISCONNECT;

			pthread_mutex_lock(&mux_listaFinalizados);
			list_add(finalizadosPorConsolas,fc);
			printf("##$$## AGREGUE A FINALIZADOS POR CONSOLA AL PID %d EXITCODE %d \n",fc->pid,fc->ecode);
			pthread_mutex_unlock(&mux_listaFinalizados);
			int k;
			pthread_mutex_lock(&mux_gl_Programas);
			if(( k=getConPosByFD(con_i->con->fd_con,gl_Programas))!= -1){
				list_remove(gl_Programas,k);
			}
			pthread_mutex_unlock(&mux_gl_Programas);

		}
	pthread_mutex_unlock(&mux_exec);
	}
	else{
		printf("cierro thread de consola\n");
	}
}


/* A partir de la cola de Ready y Exec forma un int* con los pids existentes.
 * `len' es una variable de salida para indicar la cantidad de pids que hay.
 */
int *formarPtrPIDs(int *len){

	int i,r,q, *pids;
	tPCB *pcb;

	MUX_LOCK(&mux_ready); MUX_LOCK(&mux_exec);
	r = queue_size(Ready);
	q = list_size(Exec);
	pids = malloc(r + q);

	for (i = 0; i < r; ++i){
		pcb = queue_get(Ready, i);
		memcpy(&pids[i], &pcb->id, sizeof(int));
	}

	for (i = r; i < q; ++i){
		pcb = list_get(Exec, i);
		memcpy(&pids[i], &pcb->id, sizeof(int));
	}
	MUX_UNLOCK(&mux_ready); MUX_UNLOCK(&mux_exec);

	*len = r + q;
	return pids;
}

void consolaKernel(void){

	printf("\n \n \nIngrese accion a realizar:\n");
	printf ("1-Para obtener los procesos en todas las colas o 1 especifica: 'procesos <cola>/<todos>'\n");
	printf ("2-Para ver info de un proceso determinado: 'info <PID>'\n");
	printf ("3-Para obtener la tabla global de archivos: 'tabla'\n");
	printf ("4-Para modificar el grado de multiprogramacion: 'nuevoGrado <GRADO>'\n");
	printf ("5-Para finalizar un proceso: 'finalizar <PID>'\n");
	printf ("6-Para detener la planificacion: 'stop'\n");

	char *opcion = malloc(MAXOPCION);
	int finalizar = 0;
	while(finalizar !=1){
			printf("Seleccione opcion: \n");
			//sem_wait(&haySTDIN);
			fgets(opcion,MAXOPCION,stdin);
			opcion[strlen(opcion) - 1] = '\0';
			if (strncmp(opcion,"procesos",8)==0){
				puts("Opcion procesos");
				char *cola = opcion+9;
				mostrarColaDe(cola);

			}
			if (strncmp(opcion,"info",4)==0){
				puts("Opcion info");
				char *pidInfo=opcion+5;
				int pidElegido = atoi(pidInfo);
				mostrarInfoDe(pidElegido);

			}
			if (strncmp(opcion,"tabla",5)==0){
				puts("Opcion tabla");
				mostrarTablaGlobal();
			}
			if (strncmp(opcion,"nuevoGrado",10)==0){
				puts("Opcion nuevoGrado");
				char *grado = opcion+11;
				int nuevoGrado = atoi(grado);
				cambiarGradoMultiprogramacion(nuevoGrado);
			}
			if (strncmp(opcion,"finalizar",9)==0){
				puts("Opcion finalizar");
				char *pidAFinalizar = opcion+9;
				int pidFin = atoi(pidAFinalizar);
				finalizarProceso(pidFin);

			}
			if (strncmp(opcion,"stop",4)==0){
				puts("Opcion stop");
				stopKernel();
			}


		}
}
//TODO: Hay q sincronizar las colas?? Solo las estoy mirando, no tendria pq poner un semaforo, no?
void mostrarColaDe(char* cola){
	if (strncmp(cola,"todos",5)==0){
		puts("Mostrar estado de todas las colas:");
		pthread_mutex_lock(&mux_new); pthread_mutex_lock(&mux_ready); pthread_mutex_lock(&mux_exec);
		pthread_mutex_lock(&mux_block); pthread_mutex_lock(&mux_exit);
		mostrarColaNew();
		mostrarColaReady();
		mostrarColaExec();
		mostrarColaExit();
		mostrarColaBlock();
		pthread_mutex_unlock(&mux_new); pthread_mutex_unlock(&mux_ready); pthread_mutex_unlock(&mux_exec);
		pthread_mutex_unlock(&mux_block); pthread_mutex_unlock(&mux_exit);
	}
	if (strncmp(cola,"new",3)==0){
		puts("Mostrar estado de cola NEW");
		pthread_mutex_lock(&mux_new);
		mostrarColaNew(); pthread_mutex_unlock(&mux_new);
	}
	if (strncmp(cola,"ready",5)==0){
		puts("Mostrar estado de cola REDADY");
		pthread_mutex_lock(&mux_ready);
		mostrarColaReady(); pthread_mutex_unlock(&mux_ready);
	}
	if (strncmp(cola,"exec",4)==0){
		puts("Mostrar estado de cola EXEC");
		pthread_mutex_lock(&mux_exec);
		mostrarColaExec(); pthread_mutex_unlock(&mux_exec);
	}
	if (strncmp(cola,"exit",4)==0){
		puts("Mostrar estado de cola EXIT");
		pthread_mutex_lock(&mux_exit);
		mostrarColaExit(); pthread_mutex_unlock(&mux_exit);
	}
	if (strncmp(cola,"block",5)==0){
		puts("Mostrar estado de cola BLOCK");
		pthread_mutex_lock(&mux_block);
		mostrarColaBlock(); pthread_mutex_unlock(&mux_block);
	}
}

void mostrarColaNew(){
	puts("###Cola New### ");
	int k=0;
	tPCB * pcbAux;
	if(queue_size(New)==0){
		printf("No hay procesos en esta cola\n");
		return;
	}
	for(k=0;k<queue_size(New);k++){
		pcbAux = (tPCB*) queue_get(New,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}


}

void mostrarColaReady(){
	puts("###Cola Ready###");
	int k=0;
	tPCB * pcbAux;
	if(queue_size(Ready)==0){
		printf("No hay procesos en esta cola\n");
		return;
	}
	for(k=0;k<queue_size(Ready);k++){
		pcbAux = (tPCB*) queue_get(Ready,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
}

void mostrarColaExec(){
	puts("###Cola Exec###");
	int k=0;
	tPCB * pcbAux;
	if(list_size(Exec)==0){
		printf("No hay procesos en esta cola\n");
		return;
	}
	for(k=0;k<list_size(Exec);k++){
		pcbAux = (tPCB*) list_get(Exec,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
}

void mostrarColaExit(){
	puts("###Cola Exit###");
	int k=0;
	tPCB * pcbAux;
	if(queue_size(Exit)==0){
		printf("No hay procesos en esta cola\n");
		return;
	}
	for(k=0;k<queue_size(Exit);k++){
		pcbAux = (tPCB*) queue_get(Exit,k);
		printf("En la posicion %d, el proceso %d con un ExitCode: %d\n",k,pcbAux->id,pcbAux->exitCode);
	}
}

void mostrarColaBlock(){
	puts("###Cola Block###");
	int k=0;
	tPCB * pcbAux;
	if(list_size(Block)==0){
			printf("No hay procesos en esta cola\n");
			return;
		}
	for(k=0;k<list_size(Block);k++){
		pcbAux = (tPCB*) list_get(Block,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
}


void mostrarInfoDe(int pidElegido){
	int p;
	printf("############PROCESO %d############\n",pidElegido);

	tPCB * pcbAuxiliar;

	pthread_mutex_lock(&mux_new);
	if ((p = getQueuePositionByPid(pidElegido, New)) != -1){
		pcbAuxiliar = queue_get(New,p);
		mostrarCantRafagasEjecutadasDe(pcbAuxiliar);
		mostrarTablaDeArchivosDe(pcbAuxiliar);
		//mostrarCantHeapUtilizadasDe(pcbAuxiliar); //tmb muestra 4.a y 4.b cant de acciones allocar y liberar
		//mostrarCantSyscallsUtilizadasDe(pcbAuxiliar);
		pthread_mutex_unlock(&mux_new);
		return;
	}pthread_mutex_unlock(&mux_new);

	pthread_mutex_lock(&mux_ready);
	if ((p = getQueuePositionByPid(pidElegido, Ready)) != -1){
		pcbAuxiliar = queue_get(Ready,p);
		mostrarCantRafagasEjecutadasDe(pcbAuxiliar);
		mostrarTablaDeArchivosDe(pcbAuxiliar);
		//mostrarCantHeapUtilizadasDe(pcbAuxiliar); //tmb muestra 4.a y 4.b cant de acciones allocar y liberar
		//mostrarCantSyscallsUtilizadasDe(pcbAuxiliar);
		pthread_mutex_unlock(&mux_ready);
		return;
	}pthread_mutex_unlock(&mux_ready);

	pthread_mutex_lock(&mux_exec);
	if ((p = getPCBPositionByPid(pidElegido, Exec)) != -1){
		pcbAuxiliar = list_get(Exec,p);
		mostrarCantRafagasEjecutadasDe(pcbAuxiliar);
		mostrarTablaDeArchivosDe(pcbAuxiliar);
		//mostrarCantHeapUtilizadasDe(pcbAuxiliar); //tmb muestra 4.a y 4.b cant de acciones allocar y liberar
		//mostrarCantSyscallsUtilizadasDe(pcbAuxiliar);
		pthread_mutex_unlock(&mux_exec);

		return;
	}
	pthread_mutex_unlock(&mux_exec);

	pthread_mutex_lock(&mux_block);
	if ((p = getPCBPositionByPid(pidElegido, Block)) != -1){
		pcbAuxiliar = list_get(Block,p);
		mostrarCantRafagasEjecutadasDe(pcbAuxiliar);
		mostrarTablaDeArchivosDe(pcbAuxiliar);
		//mostrarCantHeapUtilizadasDe(pcbAuxiliar); //tmb muestra 4.a y 4.b cant de acciones allocar y liberar
		//mostrarCantSyscallsUtilizadasDe(pcbAuxiliar);
		pthread_mutex_unlock(&mux_block);
		return;
	}pthread_mutex_unlock(&mux_block);

	pthread_mutex_lock(&mux_exit);
	if ((p = getQueuePositionByPid(pidElegido, Exit)) != -1){
		pcbAuxiliar = queue_get(Exit,p);
		mostrarCantRafagasEjecutadasDe(pcbAuxiliar);
		mostrarTablaDeArchivosDe(pcbAuxiliar);
		//mostrarCantHeapUtilizadasDe(pcbAuxiliar); //tmb muestra 4.a y 4.b cant de acciones allocar y liberar
		//mostrarCantSyscallsUtilizadasDe(pcbAuxiliar);
		pthread_mutex_unlock(&mux_exit);
		return;
	}pthread_mutex_unlock(&mux_exit);

	puts("no existe ese PID");
	return;




}

void mostrarCantRafagasEjecutadasDe(tPCB *pcb){

	//todo: ampliar pcb con la cant de rafagas totales?
	int cantRafagas=0;
	cantRafagas =  pcb->rafagasEjecutadas;
	printf("####PROCESO %d####\nCantidad de rafagas ejecutadas: %d\n",pcb->id,cantRafagas);
}
void mostrarTablaDeArchivosDe(tPCB *pcb){

	printf("####PROCESO %d####\nTabla de archivos abiertos del proceso: x!!!!!!x\n",pcb->id);
}
void mostrarCantHeapUtilizadasDe(tPCB *pcb){

	//TODO: ROMPE ACA!
	char spid[6];
	sprintf(spid, "%d", pcb->id);
	infoProcess *ip = dictionary_get(dict_proc_info, spid);

	printf("####PROCESO %d####\nCantidad de paginas de heap utilizadas: \t %d \n", pcb->id, ip->cant_heaps);
	printf("####PROCESO %d####\nCantidad de allocaciones realizadas: \t %d \n",    pcb->id, ip->cant_alloc);
	printf("####PROCESO %d####\nCantidad de bytes allocados: \t\t %d \n",          pcb->id, ip->bytes_allocd);
	printf("####PROCESO %d####\nCantidad de liberaciones realizadas: \t %d \n",    pcb->id, ip->cant_frees);
	printf("####PROCESO %d####\nCantidad de bytes liberados: \t\t %d \n",          pcb->id, ip->bytes_freed);
}
void mostrarCantSyscallsUtilizadasDe(tPCB *pcb){
	//todo:Rompe aca
	char spid[6];
	sprintf(spid, "%d", pcb->id);
	infoProcess *ip = dictionary_get(dict_proc_info, spid);

	printf("####PROCESO %d####\nCantidad de syscalls utilizadas : \t\t %d \n",pcb->id, ip->cant_syscalls);
}

void cambiarGradoMultiprogramacion(int nuevoGrado){
	printf("vamos a cambiar el grado a %d\n",nuevoGrado);
	//todo: semaforo
	grado_mult=nuevoGrado;
}
void finalizarProceso(int pidAFinalizar){
	printf("vamos a finalizar el proceso %d\n",pidAFinalizar);


	t_finConsola *fc = malloc (sizeof(fc));
	fc->pid=pidAFinalizar ;
	fc->ecode=CONS_FIN_PROG;

	pthread_mutex_lock(&mux_listaFinalizados);
	list_add(finalizadosPorConsolas,fc);
	pthread_mutex_unlock(&mux_listaFinalizados);
}

void mostrarTablaGlobal(){
	puts("Mostrar tabla global");
}
void stopKernel(){
	puts("Stop kernel");
}


void asociarSrcAProg(t_RelCC *con_i, tPackSrcCode *src){
	puts("Asociar Programa y Codigo Fuente");

	t_RelPF *pf;
	if ((pf = malloc(sizeof *pf)) == NULL){
		printf("No se pudieron mallocar %d bytes para RelPF\n", sizeof *pf);
		return;
	}

	pf->prog = con_i;
	pf->src  = src;
	MUX_LOCK(&mux_gl_Programas);
	list_add(gl_Programas, pf);
	MUX_UNLOCK(&mux_gl_Programas);
}

void* queue_get(t_queue *self,int posicion) {
	return list_get(self->elements, posicion);
}

int getQueuePositionByPid(int pid, t_queue *queue){

	int i, size;
	tPCB *pcb;

	size = queue_size(queue);
	for (i = 0; i < size; ++i){
		pcb = queue_get(queue, i);
		if (pcb->id == pid)
			return i;
	}
	return -1;
}

bool estaEnExit(int pid){

	int i;
	for(i = 0; i < queue_size(Exit); i++){
		tPCB *pcbAux = queue_get(Exit, i);
		if(pcbAux->id == pid)
			return true;
	}
	return false;
}
