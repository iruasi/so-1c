#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <pthread.h>
#include <math.h>

#include <commons/log.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>

#include "kernelConfigurators.h"
#include "capaMemoria.h"
#include "funcionesSyscalls.h"
#include "defsKernel.h"
#include "auxiliaresKernel.h"
#include "planificador.h"

extern int sock_fs;
extern t_log *logger;

extern tKernel *kernel;

extern t_dictionary *tablaGlobal;

t_list *gl_Programas, *list_infoProc, *listaDeCpu, *finalizadosPorConsolas;
pthread_mutex_t mux_listaDeCPU, mux_listaFinalizados, mux_gl_Programas;
extern pthread_mutex_t mux_exec;
extern sem_t eventoPlani, sem_heapDict, sem_end_exec, sem_bytes;

void setupGlobales_manejadores(void){

	listaDeCpu    = list_create();
	list_infoProc = list_create();
	gl_Programas  = list_create();
	finalizadosPorConsolas = list_create();

	pthread_mutex_init(&mux_gl_Programas,     NULL);
	pthread_mutex_init(&mux_listaFinalizados, NULL);
	pthread_mutex_init(&mux_listaDeCPU,       NULL);
}


void cpu_manejador(void *infoCPU){

	t_RelCC *cpu_i = (t_RelCC *) infoCPU;
	cpu_i->con->pid=cpu_i->con->fd_con=-1;
	log_trace(logger, "cpu_manejador socket %d\n", cpu_i->cpu.fd_cpu);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = THREAD_INIT};

	bool found;
	char *buffer, *var, sfd[MAXPID_DIG];
	char *file_serial;
	int stat, pack_size, p;
	tPackBytes *sem_bytes;
	tPackVal *alloc, *fd_rta;
	fd_rta = malloc(sizeof(*fd_rta));
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
			//buffer = serializeBytes(head, abrir->direccion, abrir->longitudDireccion, &pack_size);
			buffer = serializeAbrir(abrir,&pack_size);
			printf("La direccion es %s\n", (char *) abrir->direccion);
			if ((stat = send(sock_fs, buffer, pack_size, 0)) < 0){
				perror("No se pudo validar el archivo. error");
				head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_SEND;
				informarResultado(cpu_i->cpu.fd_cpu, head); // como fallo ejecucion, avisamos a CPU
				break;
			}
		//	freeAndNULL((void **) buffer);

			tDatosTablaGlobal * datosGlobal = malloc(sizeof *datosGlobal);
			datosGlobal->direccion = malloc(abrir->longitudDireccion);

			pack_size = 0;
			//file_serial = serializeAbrir(abrir, &pack_size);


			head.tipo_de_mensaje = VALIDAR_RESPUESTA;
			head.tipo_de_proceso = FS;

			if(true){//validarRespuesta(sock_fs,head,&h_esp)
				printf("El archivo existe, ahora verificamos si la contiene la tabla Global \n");
				agregarArchivoTablaGlobal(datosGlobal,abrir);
				agregarArchivoATablaProcesos(datosGlobal,abrir->flags,cpu_i->con->pid);
			}else if (head.tipo_de_mensaje == CREAR_ARCHIVO){
				printf("Como no fue validado el archivo, fue creado.\n");
				printf("El archivo %s fue creado con éxito \n",abrir->direccion);
				printf("Se lo agrega a la lista de procesos y tabla global\n");
				agregarArchivoTablaGlobal(datosGlobal,abrir);
				agregarArchivoATablaProcesos(datosGlobal,abrir->flags,cpu_i->con->pid);
			}else{
				printf("El archivo no pudo ser validado ni creado, fijese el posible error\n");

			}

			//Ahora me fijo que permisos tiene y si puede crearlos

			/*if((stat = recv(sock_fs,&head,HEAD_SIZE,0))<0){
				perror("Error al recivir respuesta del fs");
			}*/
			/*if(false){//Agregar validar_archivo a tiposPaqueteshead.tipo_de_mensaje == VALIDAR_ARCHIVO
				printf("El archivo existe, ahora verificamos si la contiene la tabla Global \n");

				agregarArchivoTablaGlobal(datosGlobal,abrir);
				agregarArchivoATablaProcesos(datosGlobal,abrir->flags,cpu_i->con->pid);


			} else if(abrir->flags.creacion){
				if ((stat = send(sock_fs,file_serial,pack_size,0))< 0){
					perror("No se pudo validar el archivo\n");
				}
				if((stat = recv(sock_fs,&head,HEAD_SIZE,0))<0){
					perror("Error al recivir respuesta del fs");
				}
				if(true){//head.tipo_de_mensaje == ARCHIVO_CREADO
					agregarArchivoTablaGlobal(datosGlobal,abrir);
					agregarArchivoATablaProcesos(datosGlobal,abrir->flags,cpu_i->con->pid);
					printf("Agregamos el archivo a la tabla de procesos \n");

				}
			}*/

			fd_rta->head.tipo_de_proceso = KER; fd_rta->head.tipo_de_mensaje = ENTREGO_FD;
			fd_rta->val = datosGlobal->fd;
			file_serial = serializeVal(fd_rta, &pack_size);
			if((stat = send(cpu_i->cpu.fd_cpu, file_serial, pack_size, 0)) == -1){
				perror("error al enviar el paquete a la cpu. error");
				break;
			}
			freeAndNULL((void ** )&fd_rta);freeAndNULL((void **)&buffer);
			//free(abrir->direccion); freeAndNULL((void **) &abrir);
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

			printf("Se recibió el fd # %d\n", escr->fd);
			sprintf(sfd, "%d", escr->fd);

			tDatosTablaGlobal * path = (tDatosTablaGlobal *) dictionary_get(tablaGlobal, sfd);
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
		log_trace(logger, "Se inicia thread en handler de CPU");
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

	case DUMP_DISK: // todo: agregar /dmp a FS...
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
		t_finConsola *fc = malloc(sizeof(fc));
		fc->pid = pidAFinalizar ;
		fc->ecode = CONS_FIN_PROG;

		pthread_mutex_lock(&mux_listaFinalizados);
		list_add(finalizadosPorConsolas, fc);
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

			t_finConsola *fc = malloc(sizeof(fc));
			fc->pid = con_i->con->pid ;
			fc->ecode = CONS_DISCONNECT;

			pthread_mutex_lock(&mux_listaFinalizados);
			list_add(finalizadosPorConsolas,fc);
			printf("##$$## AGREGUE A FINALIZADOS POR CONSOLA AL PID %d EXITCODE %d \n",fc->pid,fc->ecode);
			pthread_mutex_unlock(&mux_listaFinalizados);
			int k;
			pthread_mutex_lock(&mux_gl_Programas);
			if(( k = getConPosByFD(con_i->con->fd_con, gl_Programas))!= -1){
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
