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
#include <commons/log.h>

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

//extern sem_t haySTDIN;

extern t_list *gl_Programas, *list_infoProc,*finalizadosPorConsolas; // va a almacenar relaciones entre Programas y Codigo Fuente

extern pthread_mutex_t mux_exec, mux_ready, mux_gl_Programas, mux_infoProc,mux_listaFinalizados;
extern t_queue *Ready, *Exit;
extern t_list *Exec;
extern t_log * logTrace;

int globalPID;
int globalFD;
extern t_dictionary *tablaGlobal;
extern t_list *tablaProcesos;

pthread_mutex_t mux_tablaPorProceso, mux_archivosAbiertos;

tDatosTablaGlobal *crearArchivoEnTablaGlobal(tPackAbrir *abrir){
	log_trace(logTrace, "Crear registro de archivo en Tabla Global");

	char fd_str[MAXPID_DIG];
	tDatosTablaGlobal *datos = malloc(sizeof *datos);
	datos->direccion = malloc(abrir->longitudDireccion);

	datos->cantidadOpen = 1;
	datos->fd = globalFD;
	globalFD++;
	memcpy(datos->direccion, abrir->direccion, abrir->longitudDireccion);

	sprintf(fd_str, "%d", datos->fd);
	dictionary_put(tablaGlobal, fd_str, datos);

	return datos;
}

tDatosTablaGlobal *encontrarEnTablaGlobalporPath(char *path){

	tDatosTablaGlobal *dato;
	char fds[MAXPID_DIG];
	int i;

	for (i = 0; i < dictionary_size(tablaGlobal); ++i){
		sprintf(fds, "%d", i);
		if (!dictionary_has_key(tablaGlobal, fds))
			continue;

		dato = dictionary_get(tablaGlobal, fds);
		if (strcmp(path, dato->direccion) == 0){
			return dato;
		}
	}
	return NULL;
}

tDatosTablaGlobal *agregarArchivoTablaGlobal(tPackAbrir * file){
	log_trace(logTrace, "Agregar archivo a Tabla Global");

	tDatosTablaGlobal *dato;

	if ((dato = encontrarEnTablaGlobalporPath(file->direccion)) != NULL){
		dato = crearArchivoEnTablaGlobal(file);

	} else
		dato->cantidadOpen++;

	return dato;
}

tProcesoArchivo *crearArchivoDeProceso(int pid, tPackAbrir *abrir, tDatosTablaGlobal *dato){
	log_trace(logTrace, "Crear archivo del proceso");

	tProcesoArchivo *arch = malloc(sizeof *arch);
	arch->fdGlobal = dato->fd;
	arch->posicionCursor = 0;
	memcpy(&arch->flag, &abrir->flags, sizeof(t_banderas));

	t_procesoXarchivo *pid_arch = malloc(sizeof *pid_arch);
	pid_arch->pid = pid;
	pid_arch->archivosPorProceso = list_create();
	list_add(pid_arch->archivosPorProceso, arch);

	return arch;
}

void agregarArchivoATablaProcesos(tDatosTablaGlobal *datos, t_banderas flags, int pid){
	tProcesoArchivo * pa = malloc(sizeof *pa);
	log_trace(logTrace, "Agregar archivo a Tabla Procesos [PID %d]", pid);
	t_procesoXarchivo * pxa = malloc(sizeof(*pxa));

	pa->fdGlobal = datos->fd;
	pa->flag = flags;
	pa->posicionCursor = 0;

	pxa -> pid = pid;
	pxa->archivosPorProceso = list_create();
	list_add(pxa->archivosPorProceso, pa); // El index es 3 + el pid, porque 0,1 y 2 están reservados
	list_add(tablaProcesos,pxa);

	log_trace(logTrace, "Fin agregar archivo a tabla procesos [PID %d]",pid);
}

tProcesoArchivo *obtenerProcesoSegunFDLocal(t_descriptor_archivo fd , int pid, char modo){
	log_trace(logTrace,"Obtener proceso segun fd local [PID %d]", pid);


	bool encontrarFD(tProcesoArchivo * archivo){
		return archivo->fdLocal == fd;
	}
	bool encontrarPid(t_procesoXarchivo * proceso){
		return proceso->pid == pid;
	}

	t_procesoXarchivo * _unProceso;
	tProcesoArchivo * _unArchivo;

	if ((_unProceso = list_find(tablaProcesos, (void *) encontrarPid)) == NULL){
		log_error(logTrace, "No se pudo encontrar el pid en la Tabla de Procesos");
		return NULL;
	}

	if (modo == 'g'){ // list_find
		if ((_unArchivo = list_find(_unProceso->archivosPorProceso, (void *) encontrarFD)) == NULL){
			log_error(logTrace, "No se pudo encontrar el fd en la Tabla de Archivos");
			return NULL;
		}

	} else { // list_remove
		_unArchivo = list_remove_by_condition(_unProceso->archivosPorProceso, (void *) encontrarFD);
		if (_unArchivo == NULL){
			log_error(logTrace, "No se pudo encontrar el fd en la Tabla de Archivos");
			return NULL;
		}
	}
	return _unArchivo;
}

tProcesoArchivo * obtenerProcesoSegunFDGlobal(t_descriptor_archivo fd , int pid, char modo){
	log_trace(logTrace, "Obtener proceso segun fd global [PID %d]", pid);


	bool encontrarFD(tProcesoArchivo * archivo){
		return archivo->fdGlobal == fd;
	}
	bool encontrarPid(t_procesoXarchivo * proceso){
		return proceso->pid == pid;
	}

	t_procesoXarchivo * _unProceso;
	tProcesoArchivo * _unArchivo;

	if ((_unProceso = list_find(tablaProcesos, (void *) encontrarPid)) == NULL){
		log_error(logTrace, "No se encontro el PID en la Tabla de Procesos [PID %d]", pid);
		return NULL;
	}

	if (modo == 'g'){ // list_find
		if ((_unArchivo = list_find(_unProceso->archivosPorProceso, (void *) encontrarFD)) == NULL){
			log_error(logTrace, "No se encontro el PID en la Tabla de Archivos [PID %d]", pid);
			return NULL;
		}

	} else { // list_remove
		_unArchivo = list_remove_by_condition(_unProceso->archivosPorProceso, (void *) encontrarFD);
		if (_unArchivo == NULL){
			log_error(logTrace, "No se encontro el PID en la Tabla de Archivos [PID %d]", pid);
			return NULL;
		}
	}
	return _unArchivo;
}

tDatosTablaGlobal * encontrarEnTablaGlobalPorFD(t_descriptor_archivo fd_local, int pid, char modo){
	log_trace(logTrace, "Encontrar Datos en Tabla Global por fd [PID %d]", pid);

	bool encontrarFD(tProcesoArchivo *archivo){
		return archivo->fdLocal == fd_local;
	}
	bool encontrarProceso(t_procesoXarchivo * unProceso){
		return unProceso->pid == pid;
	}

	t_procesoXarchivo * _proceso = list_find(tablaProcesos, (void *) encontrarProceso);
	tProcesoArchivo * _archivo   = list_find(_proceso->archivosPorProceso, (void *) encontrarFD);

	char fd_s[MAXPID_DIG];
	sprintf(fd_s, "%d", _archivo->fdGlobal);
	if(dictionary_has_key(tablaGlobal, fd_s))
		if (modo == 'g')
			return dictionary_get(tablaGlobal, fd_s);
		else
			return dictionary_remove(tablaGlobal, fd_s);

	log_error(logTrace, "No se encontro archivo en Tabla Global");
	return NULL;
}

/* Comanda a Filesystem a que cree el archivo. Luego procede a administrarlo...
 * Trata de agregarlo a la Tabla Global, y luego agrega en Archivos por Proceso
 */
int crearArchivo(tPackAbrir *file, int sock_cpu, int sock_fs, int pid){
	log_trace(logTrace,"Crear archivo en Tabla Global [PID %d]", pid);

	char *buffer;
	int pack_size;
	tPackHeader head, h_esp;
	tDatosTablaGlobal *dato;

	head.tipo_de_mensaje = CREAR_ARCHIVO;
	buffer = serializeBytes(head, file->direccion, file->longitudDireccion, &pack_size);
	if ((send(sock_fs, buffer, pack_size, 0)) == -1){
		perror("Fallo send de crear archivo a Filesystem. error");
		log_error(logTrace, "No se pudo enviar creacion de archivo a Filesystem [PID %d]", pid);
		head.tipo_de_mensaje = FALLO_SEND;
		informarResultado(sock_cpu, head);
		return FALLO_SEND;
	}

	h_esp.tipo_de_proceso = FS; h_esp.tipo_de_mensaje = CREAR_ARCHIVO;
	if (validarRespuesta(sock_fs, h_esp, &head) != 0){
		log_error(logTrace, "Filesystem no pudo crear el archivo [PID %d]", pid);
		head.tipo_de_proceso = KER;
		informarResultado(sock_cpu, head);
		return FALLO_GRAL;
	}

	dato = agregarArchivoTablaGlobal(file);
	crearArchivoDeProceso(pid, file, dato);
	return dato->fd;
}

int validarArchivo(tPackAbrir *abrir, int sock_fs){
	log_trace(logTrace, "Validar archivo %s", abrir->direccion);

	char *buffer;
	int pack_size;
	tPackHeader head  = {.tipo_de_proceso = KER, .tipo_de_mensaje = VALIDAR_ARCHIVO};

	head.tipo_de_proceso = KER; head.tipo_de_mensaje = VALIDAR_ARCHIVO;
	buffer = serializeBytes(head, abrir->direccion, abrir->longitudDireccion, &pack_size);
	if (send(sock_fs, buffer, pack_size, 0) < 0){
		perror("No se pudo validar el archivo. error");
		return FALLO_SEND;
	}

	tPackHeader h_esp = {.tipo_de_proceso = FS,  .tipo_de_mensaje = VALIDAR_RESPUESTA};
	if (validarRespuesta(sock_fs, h_esp, &head) != 0){
		log_trace(logTrace, "Validacion del archivo %s resulto invalida", abrir->direccion);
		return head.tipo_de_mensaje;
	}

	return 0;
}

/* Remueve de la lista de ArchivosPorProceso al elemento
 * correspondiente al file descriptor fd_local del PID dado
 */
int cerrarArchivoDeProceso(int pid, int fd_local){
	log_trace(logTrace,"Cerrar archivo %d del proceso [PID %d]", fd_local, pid);

	tProcesoArchivo *file;
	if ((file = obtenerProcesoSegunFDLocal(fd_local, pid, 'r')) == NULL){
		log_error(logTrace, "No se pudo obtener fd %d del PID %d", fd_local, pid);
		return FALLO_GRAL;
	}

	free(file);
	return 0;
}

tPCB *crearPCBInicial(void){
	log_trace(logTrace,"Crear pcb inicial");

	tPCB *pcb;
	if ((pcb = malloc(sizeof *pcb)) == NULL){
		printf("Fallo mallocar %d bytes para pcbInicial\n", sizeof *pcb);
		log_error(logTrace,"fallo mallocar bytes para pcb inicial");
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
	pcb->cantSyscalls 		= 0;

	return pcb;
}

void liberarCC(t_RelCC *cc){
	log_trace(logTrace, "Liberar Relacion CPU-Consola");
	free(cc->con);
	free(cc);
}

int getConPosByFD(int fd, t_list *list){
	log_trace(logTrace, "Obtener Programa por sock %d", fd);

	int i;
	t_RelPF *pf;
	for (i = 0; i < list_size(list); ++i){
		pf = list_get(list, i);
		if (pf->prog->con->fd_con == fd){
			log_trace(logTrace,"Programa encontrado");
			return i;
		}
	}
	log_error(logTrace,"No se encontro el Programa");
	return -1;
}

int getCPUPosByFD(int fd, t_list *list){
	log_trace(logTrace,"Obtener CPU por sock %d", fd);
	int i;
	t_RelCC *cc;
	for (i = 0; i < list_size(list); ++i){
		cc = list_get(list, i);
		if (cc->cpu.fd_cpu == fd){
			log_trace(logTrace, "CPU encontrado");
			return i;
		}
	}
	log_error(logTrace, "No se encontro CPU");
	return -1;
}

/* A partir de la cola de Ready y Exec forma un int* con los pids existentes.
 * `len' es una variable de salida para indicar la cantidad de pids que hay.
 */
int *formarPtrPIDs(int *len){
	log_trace(logTrace,"inicio formar ptr pids");
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
	log_trace(logTrace,"fin formar ptr pids");
	return pids;
}

void asociarSrcAProg(t_RelCC *con_i, tPackSrcCode *src){
	//puts("Asociar Programa y Codigo Fuente");
	log_trace(logTrace,"inicio asosciar src a prog");
	t_RelPF *pf;
	if ((pf = malloc(sizeof *pf)) == NULL){
		printf("No se pudieron mallocar %d bytes para RelPF\n", sizeof *pf);
		log_error(logTrace,"no se pudieron mallocar bytes para relpf");
		return;
	}

	pf->prog = con_i;
	pf->src  = src;
	MUX_LOCK(&mux_gl_Programas);
	list_add(gl_Programas, pf);
	MUX_UNLOCK(&mux_gl_Programas);
	log_trace(logTrace,"fin asociar src a prog");
}

void *queue_get(t_queue *self, int posicion){
	return list_get(self->elements, posicion);
}


void desconexionCpu(t_RelCC *cpu_i){
	log_trace(logTrace,"inicio desocnexion cpu");
	tPCB *pcbAuxiliar;
	int p;
	log_trace(logTrace,"la cpu que se desconecto estaba ejecutando el proceso %d",cpu_i->con->pid);
	printf("La cpu que se desconecto, estaba ejecutando el proceso %d\n",cpu_i->con->pid);

	pthread_mutex_lock(&mux_exec);

	if ((p = getPCBPositionByPid(cpu_i->con->pid, Exec)) != -1){

		pcbAuxiliar = list_get(Exec,p);
		pcbAuxiliar->exitCode=DESCONEXION_CPU;

		list_remove(Exec,p);

		encolarEnExit(pcbAuxiliar,cpu_i);
	}
	log_trace(logTrace,"fin desconexion cpu");
	pthread_mutex_unlock(&mux_exec);
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

int escribirAConsola(int pidProg,int sock_con, tPackRW *escr){

	//verifico q la consola no este desconectada!

	if(estaDesconectada(pidProg) > -1 && sock_con != -1){

		char *buff;
		int pack_size, stat;
		tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = IMPRIMIR};

		if ((buff = serializeRW(head, escr, &pack_size)) == NULL)
			return FALLO_SERIALIZAC;

		memcpy(buff, &head, HEAD_SIZE);

		if ((stat = send(sock_con, buff, pack_size, 0)) == -1){
			perror("Fallo envio escritura a Consola. error");
			return FALLO_SEND;
		}
		//printf("Se enviaron %d bytes a Consola\n", stat);
		log_trace(logTrace,"se enviaron %d bytes a consola %d",stat,sock_con);
	}
	else{
		log_trace(logTrace, "la consola esta desconectada, no mandamos nada");
	}
	return 0;
}


int estaDesconectada(int pidProg){
	int k;
	MUX_LOCK(&mux_listaFinalizados);
	for(k=0;k<list_size(finalizadosPorConsolas);k++){
		t_finConsola *fcAux = list_get(finalizadosPorConsolas, k);
		if(pidProg == fcAux->pid){
			MUX_UNLOCK(&mux_listaFinalizados);
			return -2; //esta desconectada
		}
	}
	MUX_UNLOCK(&mux_listaFinalizados);
	return 1; //no esta desconectada
}

void crearInfoProcess(int pid){

	t_infoProcess *ip = malloc(sizeof *ip);
	ip->pid = pid;
	ip->cant_syscalls = 0;
	ip->rafagas_exec  = 0;
	memset(&ip->ih, 0, sizeof(ip->ih));
	MUX_LOCK(&mux_infoProc);
	list_add(list_infoProc, ip);
	MUX_UNLOCK(&mux_infoProc);
}

t_infoProcess *getInfoProcessByPID(int pid){

	int i;
	t_infoProcess *ip;

	for (i = 0; i < list_size(list_infoProc); ++i){
		ip = list_get(list_infoProc, i);

		if (ip->pid == pid)
			return ip;
	}

	return NULL;
}

void sumarSyscall(int pid){

	t_infoProcess *ip;
	if ((ip = getInfoProcessByPID(pid)) == NULL){
		printf("No se pudo obtener infoProcess para el PID %d\n", pid);
		return;
	}

	ip->cant_syscalls++;
}

void sumarPaginaHeap(int pid){

	t_infoProcess *ip;
	if ((ip = getInfoProcessByPID(pid)) == NULL){
		printf("No se pudo obtener infoProcess para el PID %d\n", pid);
		return;
	}

	ip->ih.cant_heaps++;
}

void sumarSizeYAlloc(int pid, int size){
	t_infoProcess *ip;
	if ((ip = getInfoProcessByPID(pid)) == NULL){
		printf("No se pudo obtener infoProcess para el PID %d\n", pid);
		return;
	}

	ip->ih.cant_alloc++;
	ip->ih.bytes_allocd += size;
}

void sumarFreeYDealloc(int pid, int size){
	t_infoProcess *ip;
	if ((ip = getInfoProcessByPID(pid)) == NULL){
		printf("No se pudo obtener infoProcess para el PID %d\n", pid);
		return;
	}

	ip->ih.cant_frees++;
	ip->ih.bytes_freed += size;
}
