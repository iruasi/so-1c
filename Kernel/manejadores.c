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
extern int globalFD;

void setupGlobales_manejadores(void){
	log_trace(logTrace,"setup globales_manejadores");

	globalFD = 3; // 0, 1, 2 reservados stdio

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
	int stat, pack_size, pos, fd;
	tPackBytes *sem_bytes, *bytes;
	tPackVal *alloc, *fd_rta, *oper_fd;
	tPackRW *pack_io;
	fd_rta = malloc(sizeof(*fd_rta));
	t_puntero ptr;
	tPackCursor *cursor;
	tDatosTablaGlobal *datosGlobal;
	tProcesoArchivo * procArchivo;
	tPackHeader h_esp;

	do {
	log_trace(logTrace,"(CPU) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
	switch((int) head.tipo_de_mensaje){
	case S_WAIT:
		//puts("Signal wait a semaforo");
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
		//puts("Signal continuar a semaforo");
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
		//puts("Se reasigna una variable global");
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
		//puts("Se pide el valor de una variable global");
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
		break;

	case ABRIR: //todo: mutexear utilizacion de Tabla Global y TablaPorProceso
			log_trace(logTrace,"CPU quiere abrir un archivo");

			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackAbrir * abrir = deserializeAbrir(buffer);
			freeAndNULL((void **) &buffer);
			head.tipo_de_proceso = KER;

			if ((stat = validarArchivo(abrir, sock_fs)) == FALLO_SEND){
				head.tipo_de_mensaje = stat;
				informarResultado(cpu_i->cpu.fd_cpu, head);
				break;
			}

			if (stat == INVALIDAR_RESPUESTA){ // hay que validar permiso de creacion y registrar el Archivo
				if (!abrir->flags.creacion){
					head.tipo_de_mensaje = NOCREAT_PERM;
					informarResultado(cpu_i->cpu.fd_cpu, head);
					break;

				} else
					fd = crearArchivo(abrir, cpu_i->cpu.fd_cpu, sock_fs, cpu_i->cpu.pid);
			}

			fd_rta->head.tipo_de_proceso = KER; fd_rta->head.tipo_de_mensaje = ENTREGO_FD;
			fd_rta->val = fd;
			file_serial = serializeVal(fd_rta, &pack_size);
			if((stat = send(cpu_i->cpu.fd_cpu, file_serial, pack_size, 0)) == -1){
				log_error(logTrace,"Error al enviar el paquete a la cpu");
				perror("error al enviar el paquete a la cpu. error");
				break;
			}

			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			freeAndNULL((void **) &buffer);
			freeAndNULL((void **) &file_serial);
			log_trace(logTrace,"fin case abrir");
			break;

		case BORRAR:
			log_trace(logTrace,"Case Borrar");

			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			oper_fd = deserializeVal(buffer);
			freeAndNULL((void **) &buffer);

			datosGlobal = encontrarEnTablaGlobalPorFD(oper_fd->val,cpu_i->con->pid, 'r');
			sprintf(sfd, "%d", datosGlobal->fd);

			head.tipo_de_proceso = KER;
			if (datosGlobal->cantidadOpen > 1){
				log_error(logTrace, "Se trato de borrar un archivo abierto mas de una vez");
				head.tipo_de_mensaje = FALLO_GRAL;
				informarResultado(cpu_i->cpu.fd_cpu, head);
				dictionary_put(tablaGlobal, sfd, datosGlobal);
				break;
			}

			head.tipo_de_mensaje = BORRAR;
			buffer = serializeBytes(head, datosGlobal->direccion, strlen(datosGlobal->direccion) + 1, &pack_size);

			free(datosGlobal->direccion);
			freeAndNULL((void **) &datosGlobal);

			if (send(sock_fs, buffer, pack_size, 0) == -1){
				perror("Fallo send de direccion a Filesystem. error");
				log_error(logTrace, "Fallo send de direccion a Filesystem");
				head.tipo_de_mensaje = FALLO_SEND;
				informarResultado(cpu_i->cpu.fd_cpu, head);
				break;
			}

			h_esp.tipo_de_proceso = FS; h_esp.tipo_de_mensaje = ARCHIVO_BORRADO;
			validarRespuesta(sock_fs, h_esp, &head);

			head.tipo_de_proceso = KER;
			informarResultado(cpu_i->cpu.fd_cpu, head);

			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			freeAndNULL((void ** )&buffer);
			break;

		case CERRAR:
			log_trace(logTrace, "Case CERRAR");

			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			oper_fd =  deserializeVal(buffer);

			datosGlobal = encontrarEnTablaGlobalPorFD(oper_fd->val, cpu_i->con->pid, 'r');
			if (datosGlobal->cantidadOpen == 1){ // es el ultimo abierto, lo eliminamos
				free(datosGlobal->direccion);
				freeAndNULL((void **) &datosGlobal);
			}
			sprintf(sfd, "%d", datosGlobal->fd);
			dictionary_put(tablaGlobal, sfd, datosGlobal);

			cerrarArchivoDeProceso(cpu_i->cpu.pid, oper_fd->val);

			log_trace(logTrace, "Se cerro el fd #%d de direccion %s", oper_fd->val, datosGlobal->direccion);
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = ARCHIVO_CERRADO;
			pack_size = 0;
			informarResultado(cpu_i->cpu.fd_cpu, head);

			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			freeAndNULL((void **) &buffer);
			freeAndNULL((void **) &oper_fd);
			break;

		case MOVERCURSOR:
			log_trace(logTrace,"Case Mover Cursor");

			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			cursor = deserializeMoverCursor(buffer);

			procArchivo = obtenerProcesoSegunFDLocal(cursor->fd, cpu_i->cpu.pid, 'g');
			procArchivo->posicionCursor = cursor->posicion;
			pack_size = 0;

			head.tipo_de_proceso = KER; head.tipo_de_mensaje = CURSOR_MOVIDO;
			informarResultado(cpu_i->cpu.fd_cpu, head);

			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			freeAndNULL((void **) &cursor);
			break;

		case ESCRIBIR:
			log_trace(logTrace, "Escribir a un fd");

			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			pack_io = deserializeEscribir(buffer);

			head.tipo_de_proceso = KER;
			log_trace(logTrace,"Se recibio el fd %d", pack_io->fd);

			if (pack_io->fd == FD_CONSOLA){ // lo mandamos a Consola y avisamos a CPU
				head.tipo_de_mensaje = (escribirAConsola(cpu_i->con->pid,cpu_i->con->fd_con, pack_io) < 0)?
						FALLO_INSTR : ARCHIVO_ESCRITO;
				informarResultado(cpu_i->cpu.fd_cpu, head);
				break;
			}

			if ((procArchivo = obtenerProcesoSegunFDLocal(pack_io->fd, cpu_i->cpu.pid, 'g')) == NULL){
				head.tipo_de_mensaje = FALLO_GRAL;
				informarResultado(cpu_i->cpu.fd_cpu, head);
				break;
			}
			if ((datosGlobal = encontrarEnTablaGlobalPorFD(pack_io->fd, cpu_i->cpu.pid, 'g')) == NULL){
				head.tipo_de_mensaje = FALLO_GRAL;
				informarResultado(cpu_i->cpu.fd_cpu, head);
				break;
			}

			head.tipo_de_mensaje = ESCRIBIR;
			buffer = serializeLeerFS2(head, datosGlobal->direccion, procArchivo->posicionCursor, pack_io->tamanio, &pack_size);
			if (buffer == NULL){
				head.tipo_de_mensaje = FALLO_SERIALIZAC;
				informarResultado(cpu_i->cpu.fd_cpu, head);
				break;
			}

			if (send(sock_fs, buffer, pack_size, 0) == -1){
				perror("Fallo send pedido escritura a Filesystem. error");
				log_error(logTrace, "Fallo send pedido escritura a Filesystem");
				head.tipo_de_mensaje = FALLO_SEND;
				informarResultado(cpu_i->cpu.fd_cpu, head);
				break;
			}

			h_esp.tipo_de_proceso = FS; h_esp.tipo_de_mensaje = ARCHIVO_ESCRITO;
			head.tipo_de_mensaje = (validarRespuesta(sock_fs, h_esp, &head) != 0)?
					FALLO_ESCRITURA : ARCHIVO_ESCRITO;

			MUX_LOCK(&mux_archivosAbiertos); MUX_LOCK(&mux_tablaPorProceso);
			if(dictionary_has_key(tablaGlobal,sfd)){
				path = dictionary_get(tablaGlobal, sfd);
				procArchivo = obtenerProcesoSegunFDLocal(pack_io->fd, cpu_i->con->pid, 'g');
				log_trace(logTrace,"El archivo fd #%d del PID #%d se obtuvo de la Tabla Global",pack_io->fd,cpu_i->con->pid);
			}else{
				log_error(logTrace,"La tabla global de archivos no posee el fd solicitado");
				free(pack_io->info); free(pack_io);
				freeAndNULL((void **) &buffer);
				MUX_UNLOCK(&mux_archivosAbiertos); MUX_UNLOCK(&mux_tablaPorProceso);
				break;
			}
			MUX_UNLOCK(&mux_archivosAbiertos); MUX_UNLOCK(&mux_tablaPorProceso);

			//printf("El path del direcctorio elegido es: %s\n", path->direccion);
			log_trace(logTrace,"el path del dir elegido es %s",path->direccion);


			//printf("El path del direcctorio elegido es: %s\n", path->direccion);

			head.tipo_de_proceso = KER; head.tipo_de_mensaje = ESCRIBIR;
			file_serial = serializeLeerFS(head, path->direccion, pack_io->info, pack_io->tamanio, procArchivo->flag, &pack_size);
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

			free(pack_io->info); free(pack_io);
			freeAndNULL((void **) &buffer);
			break;

		case LEER:
			log_trace(logTrace,"case leer");
			buffer = recvGeneric(cpu_i->cpu.fd_cpu);
			tPackLeer *leer = deserializeLeer(buffer);
			freeAndNULL((void **) &buffer);

			sprintf(sfd, "%d", leer->fd);
			path = dictionary_get(tablaGlobal, sfd);
			log_trace(logTrace, "el valor del fd en leer es %d", leer->fd);
			if ((procArchivo = obtenerProcesoSegunFDLocal(leer->fd,cpu_i->con->pid, 'g')) == NULL){
				head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_GRAL;
				informarResultado(cpu_i->cpu.fd_cpu, head);
			}

			head.tipo_de_proceso = KER; head.tipo_de_mensaje = LEER;
			file_serial = serializeLeerFS2(head, path->direccion, procArchivo->posicionCursor, leer->size, &pack_size); //todo: segfault
			if((stat = send(sock_fs, file_serial, pack_size, 0)) == -1){
				log_error(logTrace, "error al enviar el paquete a Filesystem");
				perror("error al enviar el paquete al filesystem");
				break;
			}
			freeAndNULL((void **) &file_serial);

			h_esp.tipo_de_proceso = FS;
			h_esp.tipo_de_mensaje = ARCHIVO_LEIDO;
			if(validarRespuesta(sock_fs, h_esp, &head) != 0){
				head.tipo_de_proceso = KER; //Esta asignacion para que el validarRespuesta de la primitiva la reconozca
				log_error(logTrace, "No se pudieron leer los datos solicitados");
				informarResultado(cpu_i->cpu.fd_cpu, head);
				break;
			}

			buffer = recvGeneric(sock_fs);
			bytes = deserializeBytes(buffer);

			head.tipo_de_proceso = KER;
			file_serial = serializeBytes(head, bytes->bytes, bytes->bytelen, &pack_size);

			if (send(cpu_i->cpu.fd_cpu, file_serial, pack_size, 0) == -1){
				log_error(logTrace, "No se pudo enviar paquete leido a CPU");
				perror("Fallo send de paquete lectura a CPU. error");
				break;
			}

			MUX_LOCK(&mux_infoProc);
			sumarSyscall(cpu_i->cpu.pid);
			MUX_UNLOCK(&mux_infoProc);

			freeAndNULL((void **) &leer);
			freeAndNULL((void **)&buffer); freeAndNULL((void **) &bytes);
			break;

	case(FIN_PROCESO): case(ABORTO_PROCESO): case(RECURSO_NO_DISPONIBLE):
			case(PCB_PREEMPT): case(PCB_BLOCK)://COLA EXIT o BLOCK
			log_trace(logTrace,"case para enviar a cola exit o block");
		cpu_i->msj = head.tipo_de_mensaje;
		cpu_handler_planificador(cpu_i);
		informarNuevoQSLuego(cpu_i);
	break;

	case HSHAKE:
		log_trace(logTrace,"se recibe handshake de cpu");
		head.tipo_de_proceso = KER; head.tipo_de_mensaje = KERINFO;
		if ((stat = contestar2ProcAProc(head, kernel->quantum_sleep, kernel->stack_size, cpu_i->cpu.fd_cpu)) < 0){
			log_error(logTrace, "No se pudo informar el quantum sleep ni stack size a cpu");
			return;
		}

		pthread_mutex_lock(&mux_listaDeCPU);
		list_add(listaDeCpu, cpu_i);
		pthread_mutex_unlock(&mux_listaDeCPU);
		sem_post(&eventoPlani);
		//puts("Fin case HSHAKE.");
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
	if ((pos = getCPUPosByFD(cpu_i->cpu.fd_cpu, listaDeCpu)) != -1){
		list_remove(listaDeCpu, pos);
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
	con_i->con->pid=-1;
	do {
	switch(head.tipo_de_mensaje){
	//printf("(CON) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
	log_trace(logTrace,"Se recibe un mensaje de consola %d",con_i->con->fd_con);
	case SRC_CODE:
		//puts("Consola quiere iniciar un programa");
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
		avisarPIDaPrograma(new_pcb->id,con_i->con->fd_con);
		log_trace(logTrace,"CONSOLA(fd=%d) ENVIA UN PROGRAMA PARA EJECUTAR. SU PID %d###\n",con_i->con->fd_con,new_pcb->id);
		printf("###CONSOLA(fd=%d) ENVIA UN PROGRAMA PARA EJECUTAR. SU PID %d###\n",con_i->con->fd_con,new_pcb->id);
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
		//con_i->con->pid = -1;
		break;

	default:
		log_trace(logTrace,"Se recibe un msj no reconocido de CON");
		puts("Se recibe un mensaje no reconocido!");
		break;

	}} while ((stat = recv(con_i->con->fd_con, &head, HEAD_SIZE, 0)) > 0);
	if(con_i->con->pid != -1){
		if(con_i->con->fd_con != -1){
			pthread_mutex_lock(&mux_exec);
			if(!estaEnExit(con_i->con->pid)){//el programa no esta en la lista de exit, osea sigue en ejecucion
				//printf("La consola %d asociada al PID: %d se desconecto.\n", con_i->con->fd_con, con_i->con->pid);

				t_finConsola *fc = malloc(sizeof(fc));
				fc->pid = con_i->con->pid ;
				fc->ecode = CONS_DISCONNECT;
				log_trace(logTrace,"CONS_DISCONNECT");
				pthread_mutex_lock(&mux_listaFinalizados);
				list_add(finalizadosPorConsolas,fc);
				pthread_mutex_unlock(&mux_listaFinalizados);
				printf("#### LA CONSOLA %d ASOCIADA AL PID %d  SE DESCONECTO. EXITCODE %d \n",con_i->con->fd_con,fc->pid,fc->ecode);
				log_trace(logTrace,"LA CONSOLA %d ASOCIADA AL PID %d  SE DESCONECTO. EXITCODE %d",con_i->con->fd_con,fc->pid,fc->ecode);

				/*int k;
				pthread_mutex_lock(&mux_gl_Programas);
				if(( k = getConPosByFD(con_i->con->fd_con, gl_Programas))!= -1){
					list_remove(gl_Programas,k);
				}
				pthread_mutex_unlock(&mux_gl_Programas);*/

			}
			pthread_mutex_unlock(&mux_exec);
		}
	}
	else{
		log_trace(logTrace,"Ciera thread de consola fd %d",con_i->con->fd_con);
		printf("cierro thread de consola fd %d\n",con_i->con->fd_con);
	}
}
