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

extern t_list *gl_Programas, *list_infoProc; // va a almacenar relaciones entre Programas y Codigo Fuente

extern pthread_mutex_t mux_exec, mux_ready, mux_gl_Programas, mux_infoProc;
extern t_queue *Ready, *Exit;
extern t_list *Exec;
extern t_log * logTrace;

int globalPID;
int globalFD;
t_dictionary *tablaGlobal;
t_list *tablaProcesos;

pthread_mutex_t mux_tablaPorProceso, mux_archivosAbiertos;

void setupGlobales_auxiliares(void){ // todo: por ahi esto entero se puede delegar en manejadores..?

	tablaGlobal   = dictionary_create();
	tablaProcesos = list_create();

	pthread_mutex_init(&mux_tablaPorProceso,  NULL);
	pthread_mutex_init(&mux_archivosAbiertos, NULL);

	log_trace(logTrace,"fin setup variables globales");
}


void agregarArchivoTablaGlobal(tDatosTablaGlobal * datos,tPackAbrir * abrir){
	char fd_str[MAXPID_DIG];
	log_trace(logTrace,"inicio agregar archivo a tabla global");
	memcpy(datos->direccion, abrir->direccion, abrir->longitudDireccion);
	datos->cantidadOpen = 0;
	datos->fd = globalFD; globalFD++;
	sprintf(fd_str, "%d", datos->fd);

	if(!dictionary_has_key(tablaGlobal, fd_str)){
		//printf("La tabla no contiene el archivo, la agregamos\n");
		log_trace(logTrace,"la tabla no contiene el archivo, la agregamos");
		dictionary_put(tablaGlobal, fd_str, datos);
		//printf("Los datos del fd # %s fueron agregados a la tabla global \n",fd_str);
		log_trace(logTrace,"los datos fueron agregados a la tabla global");
	}else{
		//printf("El archivo ya se encuentra en la tabla global\n");
		log_trace(logTrace,"el archivo ya se encuentra en la tabla global");
	}
	log_trace(logTrace,"fin agregar archivo a tabla global");
}
void agregarArchivoATablaProcesos(tDatosTablaGlobal *datos,t_banderas flags, int pid){
	tProcesoArchivo * pa = malloc(sizeof *pa);
	log_trace(logTrace,"inicio agregar archivo a tabla procesos");
	t_procesoXarchivo * pxa = malloc(sizeof(*pxa));

	pa->fd = datos->fd;
	pa->flag = flags;
	pa->posicionCursor = 0;

	pxa -> pid = pid;
	pxa->archivosPorProceso = list_create();
	list_add(pxa->archivosPorProceso,pa);//El index es 3 + el pid, porque 0,1 y 2 están reservados
	list_add(tablaProcesos,pxa);
	log_trace(logTrace,"fin agregar archivo a tabla procesos");
}


tProcesoArchivo * obtenerProcesoSegunFD(t_descriptor_archivo fd , int pid){
	log_trace(logTrace,"inicio obtener proceso segun fd");
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
		log_error(logTrace,"no hay archivo en obtener proceso segun FD");
	}
	log_trace(logTrace,"fin obtener proceso segun fd");
	return _unArchivo;
}
tDatosTablaGlobal * encontrarTablaPorFD(t_descriptor_archivo fd, int pid){
	log_trace(logTrace,"inicio encontrar tbla por fd");
	tDatosTablaGlobal * unaTabla;
	bool encontrarFD(tProcesoArchivo *archivo){
		return archivo->fd == fd;
	}
	bool encontrarProceso(t_procesoXarchivo * unProceso){
		return unProceso->pid == pid;
	}

	t_procesoXarchivo * _proceso  = list_find(tablaProcesos, encontrarProceso);
	tProcesoArchivo * _archivo = (tProcesoArchivo *) list_find(_proceso->archivosPorProceso, encontrarFD);

	char fd_s[MAXPID_DIG];
	sprintf(fd_s,"%d",fd);
	if(_archivo->fd == fd) unaTabla = dictionary_get(tablaGlobal, fd_s);
	log_trace(logTrace,"fin encontrar tabla por fd");
	return unaTabla;
}

tPCB *crearPCBInicial(void){
	log_trace(logTrace,"inicio crear pcb inicial");
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
	log_trace(logTrace,"fin crear pcb inicial");
	return pcb;
}

void liberarCC(t_RelCC *cc){
	log_trace(logTrace,"inicio liberar cc");
	free(cc->con);
	free(cc);
	log_trace(logTrace,"fin liberar cc");
}

int getConPosByFD(int fd, t_list *list){
	log_trace(logTrace,"inicio getconposbyfd");
	int i;
	t_RelPF *pf;
	for (i = 0; i < list_size(list); ++i){
		pf = list_get(list, i);
		if (pf->prog->con->fd_con == fd){
			log_trace(logTrace,"fin exitoso con pos by fd");
			return i;
		}
	}
	log_trace(logTrace,"fin conposbyfd no se encontro el programa en gl_programas");
	printf("No se encontro el programa de socket %d en la gl_Programas\n", fd);
	return -1;
}

int getCPUPosByFD(int fd, t_list *list){
	log_trace(logTrace,"inicio get cpu pos by fd");
	int i;
	t_RelCC *cc;
	for (i = 0; i < list_size(list); ++i){
		cc = list_get(list, i);
		if (cc->cpu.fd_cpu == fd){
			log_trace(logTrace,"fin exitoso get cpu pos by fd");
			return i;
		}
	}
	log_trace(logTrace,"fin get cpu pos by fd no se encontro el socket en la lista de cpu");
	printf("No se encontro el CPU de socket %d en la listaDeCpu\n", fd);
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

int escribirAConsola(int sock_con, tPackRW *escr){

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
	printf("Se enviaron %d bytes a Consola\n", stat);

	return 0;
}


void crearInfoProcess(int pid){

	t_infoProcess *ip = malloc(sizeof *ip);
	ip->pid = pid;
	ip->cant_syscalls = 0;
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

	ip->ih->cant_heaps++;
}

void sumarSizeYAlloc(int pid, int size){
	t_infoProcess *ip;
	if ((ip = getInfoProcessByPID(pid)) == NULL){
		printf("No se pudo obtener infoProcess para el PID %d\n", pid);
		return;
	}

	ip->ih->cant_alloc++;
	ip->ih->bytes_allocd += size;
}

void sumarFreeYDealloc(int pid, int size){
	t_infoProcess *ip;
	if ((ip = getInfoProcessByPID(pid)) == NULL){
		printf("No se pudo obtener infoProcess para el PID %d\n", pid);
		return;
	}

	ip->ih->cant_frees++;
	ip->ih->bytes_freed += size;
}
