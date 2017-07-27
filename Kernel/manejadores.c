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
extern t_log *logTrace;

extern tKernel *kernel;

t_dictionary *tablaGlobal;
t_list *gl_Programas, *list_infoProc, *listaDeCpu, *finalizadosPorConsolas, *tablaProcesos,*listaAvisarQS;
pthread_mutex_t mux_listaDeCPU, mux_listaFinalizados, mux_gl_Programas, mux_infoProc,mux_listaAvisar;
extern pthread_mutex_t mux_exec, mux_tablaPorProceso, mux_archivosAbiertos;
extern sem_t eventoPlani, sem_heapDict, sem_end_exec, sem_bytes;

void setupGlobales_manejadores(void){
	log_trace(logTrace,"setup globales_manejadores");

	tablaGlobal   = dictionary_create();
	tablaProcesos = list_create();
	listaDeCpu    = list_create();
	list_infoProc = list_create();
	gl_Programas  = list_create();
	finalizadosPorConsolas = list_create();
	listaAvisarQS = list_create();

	pthread_mutex_init(&mux_tablaPorProceso,  NULL);
	pthread_mutex_init(&mux_archivosAbiertos, NULL);
	pthread_mutex_init(&mux_infoProc,         NULL);
	pthread_mutex_init(&mux_gl_Programas,     NULL);
	pthread_mutex_init(&mux_listaFinalizados, NULL);
	pthread_mutex_init(&mux_listaDeCPU,       NULL);
	pthread_mutex_init(&mux_listaAvisar,NULL);
}

void cpu_manejador(void *infoCPU){
	t_RelCC *cpu_i = (t_RelCC *) infoCPU;
	cpu_i->con->pid=cpu_i->con->fd_con=-1;
	log_trace(logTrace, "cpu_manejador socket %d\n", cpu_i->cpu.fd_cpu);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = THREAD_INIT};

	bool found;
	char *buffer, *var, sfd[MAXPID_DIG];
	char *file_serial;
	int stat, pack_size, p;
	tPackBytes *sem_bytes;
	tPackVal *alloc, *fd_rta;
	fd_rta = malloc(sizeof(*fd_rta));
	t_puntero ptr;
	tPackHeader h_esp;

	do {
	log_trace(logTrace,"(CPU) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
	switch((int) head.tipo_de_mensaje){
	case S_WAIT:
		puts("Signal wait a semaforo");
		log_trace(logTrace,"Signal wait a semaforo");
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
		MUX_LOCK(&mux_infoProc);
		sumarSyscall(cpu_i->cpu.pid);
		MUX_UNLOCK(&mux_infoProc);

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &sem_bytes);
		break;

	case S_SIGNAL:
		puts("Signal continuar a semaforo");
		log_trace(logTrace,"Signal continuar a semaforo");
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
		MUX_LOCK(&mux_infoProc);
		sumarSyscall(cpu_i->cpu.pid);
		MUX_UNLOCK(&mux_infoProc);

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &sem_bytes);
		break;

	case SET_GLOBAL:
		puts("Se reasigna una variable global");
		log_trace(logTrace,"Se reasigna una variable global");
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
		MUX_LOCK(&mux_infoProc);
		sumarSyscall(cpu_i->cpu.pid);
		MUX_UNLOCK(&mux_infoProc);

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &val_comp);
		break;

	case GET_GLOBAL:
		puts("Se pide el valor de una variable global");
		log_trace(logTrace,"Se pide el valor de una variable global");
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
			log_error(logTrace,"Fallo send variable global a CPU");
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_SEND;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}
		MUX_LOCK(&mux_infoProc);
		sumarSyscall(cpu_i->cpu.pid);
		MUX_UNLOCK(&mux_infoProc);

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &var);
		freeAndNULL((void **) &var_name->bytes); freeAndNULL((void **) &var_name);
		break;

	case RESERVAR:
		log_trace(logTrace,"Funcion reservar");
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
			log_error(logTrace,"Fallo send de puntero alojado a cpu");
			perror("Fallo send de puntero alojado a CPU. error");
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_SEND;
			informarResultado(cpu_i->cpu.fd_cpu, head);
			break;
		}
		MUX_LOCK(&mux_infoProc);
		sumarSyscall(cpu_i->cpu.pid);
		MUX_UNLOCK(&mux_infoProc);

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &alloc);
		break;

	case LIBERAR:
		puts("Funcion liberar");
		log_trace(logTrace,"Funcion liberar");
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
		MUX_LOCK(&mux_infoProc);
		sumarSyscall(cpu_i->cpu.pid);
		MUX_UNLOCK(&mux_infoProc);

		//puts("Fin case LIBERAR");
		break;

	case ABRIR:
			puts("CPU quiere abrir un archivo");
			log_trace(logTrace,"Cpu quiere abrir un archivo");
			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackAbrir * abrir = deserializeAbrir(buffer);
			freeAndNULL((void **) &buffer);

			head.tipo_de_proceso = KER; head.tipo_de_mensaje = VALIDAR_ARCHIVO;
			buffer = serializeBytes(head, abrir->direccion, abrir->longitudDireccion, &pack_size);
			//buffer = serializeAbrir(abrir,&pack_size);
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

			if(validarRespuesta(sock_fs,head,&h_esp)==0){//validarRespuesta(sock_fs,head,&h_esp)
				//printf("El archivo existe, ahora verificamos si la contiene la tabla Global \n");
				log_trace(logTrace,"El archivo existe, verificamos si la contiene la tabla global");
				agregarArchivoTablaGlobal(datosGlobal,abrir);
				agregarArchivoATablaProcesos(datosGlobal,abrir->flags,cpu_i->con->pid);
			}else if(abrir->flags.creacion){
				head.tipo_de_mensaje = CREAR_ARCHIVO;
				buffer = serializeBytes(head, abrir->direccion, abrir->longitudDireccion, &pack_size);
				if ((stat = send(sock_fs, buffer, pack_size, 0)) < 0){
					perror("No se pudo validar el archivo. error");
					head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_SEND;
					informarResultado(cpu_i->cpu.fd_cpu, head); // como fallo ejecucion, avisamos a CPU
					break;
				}else if(validarRespuesta(sock_fs,head,&h_esp)==0){
					log_trace(logTrace,"El archivo se crea con exito y se agrega a la listsa de procesos y tabla global");
					agregarArchivoTablaGlobal(datosGlobal,abrir);
					agregarArchivoATablaProcesos(datosGlobal,abrir->flags,cpu_i->con->pid);
				}
			}else{
				printf("El archivo no pudo ser validado ni creado, fijese el posible error\n");
				log_error(logTrace,"El archivo no pudo ser validado ni creado");
			}



			fd_rta->head.tipo_de_proceso = KER; fd_rta->head.tipo_de_mensaje = ENTREGO_FD;
			fd_rta->val = datosGlobal->fd;
			file_serial = serializeVal(fd_rta, &pack_size);
			if((stat = send(cpu_i->cpu.fd_cpu, file_serial, pack_size, 0)) == -1){
				log_error(logTrace,"Error al enviar el paquete a la cpu");
				perror("error al enviar el paquete a la cpu. error");
				break;
			}
			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			freeAndNULL((void ** )&fd_rta);freeAndNULL((void **)&buffer);
			//free(abrir->direccion); freeAndNULL((void **) &abrir);
			//puts("Fin case ABRIR");
			log_trace(logTrace,"fin case abrir");
			break;
		case BORRAR:
			log_trace(logTrace,"Case Borrar");
			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackPID * borrar_fd =  deserializeVal(buffer);
			tDatosTablaGlobal * unaTabla;

			unaTabla = encontrarTablaPorFD(borrar_fd->val,cpu_i->con->pid);
			if(unaTabla->cantidadOpen <= 0){
				log_error(logTrace,"El arc hivo no se encuentra abierto");
				perror("El archivo no se encuentra abierto");
				break;
			}else if(unaTabla->cantidadOpen > 1){
				log_error(logTrace,"El archivo solciitado esa abierto mas de  1 vez al mismo tiempo");
				perror("El archivo solicitado esta abierto más de 1 vez al mismo tiempo");
				break;
			}

			tPackHeader header = {.tipo_de_proceso = FS, .tipo_de_mensaje = BORRAR};
			char * borrar_serial = serializeBytes(header,borrar_fd->val,sizeof(int),&pack_size);

			if((stat = send(sock_fs,borrar_serial,pack_size,0)) == -1){
				log_error(logTrace,"Error al mandar peticion de borrado de archivo al fs");
				perror("error al mandar petición de borrado de archivo al filesystem");
				break;
			}

			if((stat = recv(sock_fs, &head, sizeof head, 0)) == -1){
				log_error(logTrace,"Error al recibir el paquete al fs");
				perror("error al recibir el paquete al filesystem");
				break;
			}
			if(head.tipo_de_mensaje == 1){//TODO: CAMBIAR ESTE 1 POR EL PROTOCOLO CORRESPONDIENTE
				buffer = recvGeneric(sock_fs);
				if((stat = send(cpu_i->cpu.fd_cpu,buffer,pack_size,0)) == -1){
					log_error(logTrace,"error al enviar el paquete al fs");
					perror("error al enviar el paquete al filesystem");
					break;
				}
			}
			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			freeAndNULL((void ** )&buffer);
			break;
		case CERRAR:
			log_trace(logTrace,"Case CERRAR");
			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackPID * cerrar_fd =  deserializeVal(buffer);
			tDatosTablaGlobal * archivoCerrado = encontrarTablaPorFD(cerrar_fd->val,cpu_i->con->pid);

			archivoCerrado -> cantidadOpen--;

			printf("Se cerró el archivo de fd #%d y de direccion %s",cerrar_fd->val,(char *) &(archivoCerrado-> direccion));
			log_trace(logTrace,"se cerro el archivo solicitado");
			tPackHeader header2 = {.tipo_de_proceso = KER, .tipo_de_mensaje = ARCHIVO_CERRADO}; //ARCHIVO_CERRADO = 120
			pack_size = 0;
			//char * cerrar_serial = serializeHeader(header2,&pack_size);
			informarResultado(cpu_i->cpu.fd_cpu,header2);

			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			freeAndNULL((void ** )&buffer);
			break;

		case MOVERCURSOR:
			log_trace(logTrace,"case mover cursor");
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
				log_error(logTrace,"error al enviar el cambio de cursor a la cpu");
				perror("error al enviar el cambio de cursor a la cpu");
			}
			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			break;
		case ESCRIBIR:

			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackRW *escr = deserializeEscribir(buffer);

			head.tipo_de_proceso = KER;
			log_trace(logTrace,"se recibio el fd %d",escr->fd);
			printf("Se escribe en fd %d, la info %s\n", escr->fd, (char *) escr->info);

			if (escr->fd == FD_CONSOLA){ // lo mandamos a Consola y avisamos a CPU
				head.tipo_de_mensaje = (escribirAConsola(cpu_i->con->fd_con, escr) < 0)?
						FALLO_INSTR : ARCHIVO_ESCRITO;
				informarResultado(cpu_i->cpu.fd_cpu, head);
				break;
			}

			sprintf(sfd, "%d", escr->fd);
			tDatosTablaGlobal * path;
			tProcesoArchivo * banderas;
			MUX_LOCK(&mux_archivosAbiertos); MUX_LOCK(&mux_tablaPorProceso);
			if(dictionary_has_key(tablaGlobal,sfd)){
				path = dictionary_get(tablaGlobal, sfd); // todo: convendria que verificar antes si dictionary_has_key(dict, sfd)
				banderas = obtenerProcesoSegunFD(escr->fd, cpu_i->con->pid);
				log_trace(logTrace,"El archivo de fd #%d del proceso #%d fue obtenido con exito de la tabla globa",escr->fd,cpu_i->con->pid);
			}else{
				log_error(logTrace,"La tabla global de archivos no posee el fd solicitado");
				free(escr->info); free(escr);
				freeAndNULL((void **) &buffer);
				break;
			}
			MUX_UNLOCK(&mux_archivosAbiertos); MUX_UNLOCK(&mux_tablaPorProceso);

			printf("El path del direcctorio elegido es: %s\n", path->direccion);


			file_serial = serializeLeerFS(path->direccion, escr->info, escr->tamanio, banderas->flag, &pack_size);
			if((stat = send(sock_fs, file_serial, pack_size, 0)) == -1){
				log_error(logTrace,"error al enviar el paquete al fs");
				perror("error al enviar el paquete al filesystem");
				break;
			}
			head.tipo_de_proceso = FS;
			head.tipo_de_mensaje = ARCHIVO_ESCRITO;
			if(validarRespuesta(sock_fs,head,&h_esp)== 0){
				head.tipo_de_mensaje =  ARCHIVO_ESCRITO;
				head.tipo_de_proceso = KER; //Esta asignacion para que el validarRespuesta de la primitiva la reconozca
				informarResultado(cpu_i->cpu.fd_cpu, head);
			}else{
				log_error(logTrace,"No se pudieron escribir los datos solicitados");
				head.tipo_de_proceso = KER;
				head.tipo_de_mensaje = FALLO_ESCRITURA;
				informarResultado(cpu_i->cpu.fd_cpu,head);
			}


			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			free(escr->info); free(escr);
			freeAndNULL((void **) &buffer);
			break;

		case LEER:
			log_trace(logTrace,"case leer");
			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackRW * leer = deserializeRW(buffer);
			path =  dictionary_get(tablaGlobal,(char *) &leer->fd);
			//printf("El valor del fd en leer es %d \n", leer->fd);
			log_trace(logTrace,"el valor del fd en leer es %d",leer->fd);
			banderas = obtenerProcesoSegunFD(leer->fd,cpu_i->con->pid);
			//printf("El path del direcctorio elegido es: %s\n", (char *) path->direccion);

			file_serial = serializeLeerFS(path->direccion,leer->info,leer->tamanio,banderas->flag,&pack_size);
			if((stat = send(sock_fs,file_serial,pack_size,0)) == -1){
				log_error(logTrace,"error al enviar el paquete alfs");
				perror("error al enviar el paquete al filesystem");
				break;
			}

			head.tipo_de_proceso = FS;
			head.tipo_de_mensaje = ARCHIVO_LEIDO;
			if(validarRespuesta(sock_fs,head,&h_esp)== 0){
				head.tipo_de_mensaje =  ARCHIVO_LEIDO;
				head.tipo_de_proceso = KER; //Esta asignacion para que el validarRespuesta de la primitiva la reconozca
				informarResultado(cpu_i->cpu.fd_cpu, head);
			}else{
				log_error(logTrace,"No se pudieron escribir los datos solicitados");
				head.tipo_de_proceso = KER;
				head.tipo_de_mensaje = FALLO_ESCRITURA;
				informarResultado(cpu_i->cpu.fd_cpu,head);
			}
			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			free(leer->info); free(leer);
			freeAndNULL((void **)&buffer);
			break;

	case(FIN_PROCESO): case(ABORTO_PROCESO): case(RECURSO_NO_DISPONIBLE):
			case(PCB_PREEMPT): case(PCB_BLOCK)://COLA EXIT o BLOCK
			log_trace(logTrace,"case para enviar a cola exit o block");
		cpu_i->msj = head.tipo_de_mensaje;
		cpu_handler_planificador(cpu_i);
		informarNuevoQSLuego(cpu_i);
	break;

	case HSHAKE:
		//puts("Se recibe handshake de CPU");
		log_trace(logTrace,"se recibe handshake de cpu");
		head.tipo_de_proceso = KER; head.tipo_de_mensaje = KERINFO;
		if ((stat = contestar2ProcAProc(head, kernel->quantum_sleep, kernel->stack_size, cpu_i->cpu.fd_cpu)) < 0){
			log_error(logTrace,"no se pudo informar el qs y stack size a cpu");
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
		log_trace(logTrace, "Se inicia thread en handler de CPU");
		//puts("Fin case THREAD_INIT.");
		break;

	default:
		puts("Funcion no reconocida!");
		log_error(logTrace,"Funcion no reconocida");
		break;

	}} while((stat = recv(cpu_i->cpu.fd_cpu, &head, sizeof head, 0)) > 0);

	if (stat == -1){
		log_error(logTrace,"Error de recepcion de CPU");
		perror("Error de recepcion de CPU. error");
		return;
	}

	puts("CPU se desconecto, la sacamos de la listaDeCpu..");
	log_trace(logTrace,"Cpu se desconecto, sale de la lista de CPU");
	if(cpu_i->con->pid > -1){ //esta cpu tenia asignado un proceso. //con->pid o cpu->pid ?!? todo;

		desconexionCpu(cpu_i);//en esta funcion ponemos el pcb mas actual en exit y avisamos a consola el fin..
	}

	pthread_mutex_lock(&mux_listaDeCPU);
	if ((p = getCPUPosByFD(cpu_i->cpu.fd_cpu, listaDeCpu)) != -1){
		list_remove(listaDeCpu,p);
	}
	pthread_mutex_unlock(&mux_listaDeCPU);

	liberarCC(cpu_i);
	log_trace(logTrace,"Liberamos CC");
}

void mem_manejador(void *m_sock){
	int *sock_mem = (int*) m_sock;
	//printf("mem_manejador socket %d\n", *sock_mem);
	log_trace(logTrace,"mem_manejador");
	int stat;
	char *buffer;
	tPackHeader head = {.tipo_de_proceso = MEM, .tipo_de_mensaje = THREAD_INIT};

	do {
	switch((int) head.tipo_de_mensaje){
	//printf("(MEM) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
	log_trace(logTrace,"MEM) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);

	case ASIGN_SUCCS : case FALLO_ASIGN:
		//puts("Se recibe respuesta de asignacion de paginas para algun proceso");
		log_trace(logTrace,"se recibe rta de asignacion de pags para algun proceso");
		sem_post(&sem_heapDict);
		sem_wait(&sem_end_exec);
		//puts("Fin case ASIGN_SUCCESS_OR_FAIL");
		break;

	case BYTES:
		//puts("Se reciben bytes desde Memoria");
		log_trace(logTrace,"Se reciben bytes desde memoria");
		sem_post(&sem_bytes);
		sem_wait(&sem_end_exec);
		//puts("Fin case BYTES");
		break;

	case THREAD_INIT:
		//puts("Se inicia thread en handler de Memoria");
		//puts("Fin case THREAD_INIT");
		log_trace(logTrace,"Se inicia thread en handler de memoria");
		break;

	case DUMP_DISK: // todo: agregar /dmp a FS...
		//puts("Memoria dumpea informacion en /dmp");
		log_trace(logTrace,"Memoria dumpea info en /dmp");
		break;

	case PID_LIST:
		//puts("Memoria pide lista de procesos activos");
		log_trace(logTrace,"Memoria pide lista de procesos activos");
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
			log_error(logTrace,"No se pudo mandar lista de pids a memoria");
			perror("No se pudo mandar lista de PIDs a Memoria. error");
		//printf("Se enviaron %d bytes a Memoria\n", stat);
		log_trace(logTrace,"Se enviaron %d bytes a memoria",stat);
		break;

	default:
		//puts("Se recibe un mensaje de Memoria no considerado");
		log_trace(logTrace,"Se recibe un msj de memoria no considerado");
		break;

	}} while ((stat = recv(*sock_mem, &head, HEAD_SIZE, 0)) > 0);
}

void cons_manejador(void *conInfo){
	t_RelCC *con_i = (t_RelCC*) conInfo;
	//printf("cons_manejador socket %d\n", con_i->con->fd_con);
	log_trace(logTrace,"Cons_manejador socket %d",con_i->con->fd_con);
	int stat;
	tPackHeader head = {.tipo_de_proceso = CON, .tipo_de_mensaje = THREAD_INIT};
	char *buffer;
	tPackSrcCode *entradaPrograma;
	tPackPID *ppid;
	int pidAFinalizar;

	do {
	switch(head.tipo_de_mensaje){
	//printf("(CON) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
	log_trace(logTrace,"Se recibe un mensaje de consola %d",con_i->con->fd_con);
	case SRC_CODE:
		puts("Consola quiere iniciar un programa");
		log_trace(logTrace,"Consola quiere iniciar un programa");
		if ((buffer = recvGeneric(con_i->con->fd_con)) == NULL){
			puts("Fallo recepcion de SRC_CODE");
			log_error(logTrace,"Fallo recepcion de src code");
			return;
		}

		if ((entradaPrograma = (tPackSrcCode *) deserializeBytes(buffer)) == NULL){
			log_error(logTrace,"Fallo deserializacion de bytes");
			puts("Fallo deserializacion de Bytes");
			return;
		}

		tPCB *new_pcb = crearPCBInicial();
		con_i->con->pid = new_pcb->id;
		asociarSrcAProg(con_i, entradaPrograma);

		//printf("El size del paquete %d\n", strlen(entradaPrograma->bytes) + 1);
		encolarEnNew(new_pcb);

		freeAndNULL((void **) &buffer);
		//puts("Fin case SRC_CODE.");
		break;

	case THREAD_INIT:
		log_trace(logTrace,"Se inicia thread en handler de consola");
		//puts("Se inicia thread en handler de Consola");
		//puts("Fin case THREAD_INIT.");
		break;

	case HSHAKE:
		log_trace(logTrace,"Es solo un HS de consola");
		//puts("Es solo un handshake");
		break;

	case KILL_PID:
		log_trace(logTrace,"Case KILL_PID de consola");
		if ((buffer = recvGeneric(con_i->con->fd_con)) == NULL){
			log_error(logTrace,"error al recibir el pid");
			puts("error al recibir el pid");
			return;
		}

		if ((ppid = deserializeVal(buffer)) == NULL){
			log_error(logTrace,"error al deserializar el packPID");
			puts("Error al deserializar el PACKPID");
			return;
		}

		log_trace(logTrace,"asigno pid a la estructura");
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
		log_trace(logTrace,"Se recibe un msj no reconocido de CON");
		puts("Se recibe un mensaje no reconocido!");
		break;

	}} while ((stat = recv(con_i->con->fd_con, &head, HEAD_SIZE, 0)) > 0);

	if(con_i->con->fd_con != -1){
	pthread_mutex_lock(&mux_exec);
		if(!estaEnExit(con_i->con->pid)){//el programa no esta en la lista de exit, osea sigue en ejecucion
			//printf("La consola %d asociada al PID: %d se desconecto.\n", con_i->con->fd_con, con_i->con->pid);

			t_finConsola *fc = malloc(sizeof(fc));
			fc->pid = con_i->con->pid ;
			fc->ecode = CONS_DISCONNECT;
			log_trace(logTrace,"CON_DUSCONNECT");
			pthread_mutex_lock(&mux_listaFinalizados);
			list_add(finalizadosPorConsolas,fc);
			printf("##$$## LA CONSOLA %d ASOCIADA AL PID %d  SE DESCONECTO. EXITCODE %d \n",con_i->con->fd_con,fc->pid,fc->ecode);
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
		log_trace(logTrace,"Ciera thread de consola");
		//printf("cierro thread de consola\n");
	}
}
