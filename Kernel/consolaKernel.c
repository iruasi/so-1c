#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAXOPCION 200
#define MAXPID_DIG 6

#include <tiposRecursos/tiposErrores.h>

#include "defsKernel.h"
#include "auxiliaresKernel.h"
#include "planificador.h"
#include "consolaKernel.h"



extern t_queue *New, *Ready, *Exit;
extern t_list *Exec, *Block;
extern int grado_mult;

extern pthread_mutex_t mux_new, mux_ready, mux_exec, mux_block, mux_exit, mux_listaFinalizados,mux_gradoMultiprog;

extern t_dictionary *tablaGlobal;
extern t_list *tablaProcesos, *list_infoProc, *finalizadosPorConsolas;

bool planificacionBloqueada;

pthread_mutex_t mux_planificacionBloqueada;

void setupGlobales_consolaKernel(void){
	pthread_mutex_init(&mux_planificacionBloqueada, NULL);
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
			break;

		}
		if (strncmp(opcion,"stop",4)==0){
			puts("Opcion stop");
			stopKernel();
			break;
		}
		if (strncmp(opcion,"tabla",5)==0){
			puts("Opcion tabla");
			mostrarTablaGlobal();
			break;
		}
		if (strncmp(opcion,"nuevoGrado",10)==0){
			puts("Opcion nuevoGrado");
			char *grado = opcion+11;
			int nuevoGrado = atoi(grado);
			cambiarGradoMultiprogramacion(nuevoGrado);
			break;
		}
		if(!planificacionBloqueada){
			if (strncmp(opcion,"info",4)==0){
				puts("Opcion info");
				char *pidInfo=opcion+5;
				int pidElegido = atoi(pidInfo);
				mostrarInfoDe(pidElegido);
				break;

			}
			if (strncmp(opcion,"finalizar",9)==0){
				puts("Opcion finalizar");
				char *pidAFinalizar = opcion+9;
				int pidFin = atoi(pidAFinalizar);
				finalizarProceso(pidFin);
				break;
			}
		}else{
			puts("No se puede acceder a este comando con la planificacion bloqueada :)");
		}
	}
}
//TODO: Hay q sincronizar las colas?? Solo las estoy mirando, no tendria pq poner un semaforo, no?
void mostrarColaDe(char* cola){

	if(!planificacionBloqueada){
		puts("Muestro estado de las colas con la planificacion no bloqueada");

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
	}else{
		puts("Muestro estado de las colas con planificacion bloqueada");
		if (strncmp(cola,"todos",5)==0){
					puts("Mostrar estado de todas las colas:");
					mostrarColaNew();
					mostrarColaReady();
					mostrarColaExec();
					mostrarColaExit();
					mostrarColaBlock();

				}
				if (strncmp(cola,"new",3)==0){
					puts("Mostrar estado de cola NEW");
					mostrarColaNew();
				}
				if (strncmp(cola,"ready",5)==0){
					puts("Mostrar estado de cola REDADY");
					mostrarColaReady();
				}
				if (strncmp(cola,"exec",4)==0){
					puts("Mostrar estado de cola EXEC");
					mostrarColaExec();
				}
				if (strncmp(cola,"exit",4)==0){
					puts("Mostrar estado de cola EXIT");
					mostrarColaExit();
				}
				if (strncmp(cola,"block",5)==0){
					puts("Mostrar estado de cola BLOCK");
					mostrarColaBlock();
				}

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

void armarStringPermisos(char* permisos, int creacion, int lectura, int escritura){

	if (creacion)
		string_append(&permisos, "c");

	if (lectura)
		string_append(&permisos, "r");

	if (escritura)
		string_append(&permisos, "w");
}

void mostrarTablaDeArchivosDe(tPCB *pcb){
	char pid[MAXPID_DIG];
	sprintf(pid,"%d",pcb->id);
	bool encontrarPid(t_procesoXarchivo * proceso){
			return proceso->pid == pcb->id;
	}
	t_procesoXarchivo * pa = list_find(tablaProcesos, encontrarPid);
	tProcesoArchivo * _unArchivo;
	if(list_is_empty(pa->archivosPorProceso)){
		printf("La tabla de procesos del proceso %d se encuentra vacía",pcb->id);
		return;
	}
	char * permisos = string_new();
	armarStringPermisos(permisos,_unArchivo->flag.creacion,_unArchivo->flag.escritura,_unArchivo->flag.lectura);
	int i;
	for(i = 0; i < list_size(pa->archivosPorProceso); i++){
		_unArchivo = (tProcesoArchivo *) list_get(pa->archivosPorProceso, i);
		printf("####PROCESO %d####\nTabla de archivos abiertos del proceso\n",pcb->id);
		printf("Permisos: %s --fdGlobalAsociado: %d -- cursor: %d",permisos,_unArchivo->fd,_unArchivo->posicionCursor);

	}
}

void mostrarCantHeapUtilizadasDe(tPCB *pcb){

	//TODO: ROMPE ACA!

	t_infoProcess *ip = list_get(list_infoProc, getInfoProcPosByPID(list_infoProc, pcb->id));

	printf("####PROCESO %d####\nCantidad de paginas de heap utilizadas: \t %d \n", pcb->id, ip->ih->cant_heaps);
	printf("####PROCESO %d####\nCantidad de allocaciones realizadas: \t %d \n",    pcb->id, ip->ih->cant_alloc);
	printf("####PROCESO %d####\nCantidad de bytes allocados: \t\t %d \n",          pcb->id, ip->ih->bytes_allocd);
	printf("####PROCESO %d####\nCantidad de liberaciones realizadas: \t %d \n",    pcb->id, ip->ih->cant_frees);
	printf("####PROCESO %d####\nCantidad de bytes liberados: \t\t %d \n",          pcb->id, ip->ih->bytes_freed);
}

int getInfoProcPosByPID(t_list *infoProc, int pid){

	int i;
	t_infoProcess *ip;

	for (i = 0; i < list_size(infoProc); ++i){
		ip = list_get(infoProc, i);
		if (ip->pid == pid)
			return i;
	}

	printf("No se encontro PID %d en la lista infoProc\n", pid);
	return -1;
}

void mostrarCantSyscallsUtilizadasDe(tPCB *pcb){
	//todo:Rompe aca

	t_infoProcess *ip = list_get(list_infoProc, getInfoProcPosByPID(list_infoProc, pcb->id));
	printf("####PROCESO %d####\nCantidad de syscalls utilizadas : \t\t %d \n",pcb->id, ip->cant_syscalls);
}

void cambiarGradoMultiprogramacion(int nuevoGrado){
	printf("vamos a cambiar el grado a %d\n",nuevoGrado);
	//todo: semaforo
	MUX_LOCK(&mux_gradoMultiprog);
	grado_mult=nuevoGrado;
	MUX_UNLOCK(&mux_gradoMultiprog);
}

void finalizarProceso(int pidAFinalizar){
	printf("vamos a finalizar el proceso %d\n",pidAFinalizar);


	t_finConsola *fc = malloc (sizeof(fc));
	fc->pid=pidAFinalizar ;
	fc->ecode=CONS_FIN_PROG;

	pthread_mutex_lock(&mux_listaFinalizados);
	list_add(finalizadosPorConsolas, fc);
	pthread_mutex_unlock(&mux_listaFinalizados);
}

void mostrarTablaGlobal(){
	puts("Mostrar tabla global");
	if(dictionary_is_empty(tablaGlobal)){
		printf("La tabla global de archivos se encuentra vacía\n");
		return;
	}
	tDatosTablaGlobal * datosGlobal;
	int i ;
	char contAux[MAXPID_DIG];
	for(i = 0 ; i < dictionary_size(tablaGlobal);i++){
		sprintf(contAux, "%d", i);
		datosGlobal = (tDatosTablaGlobal *) dictionary_get(tablaGlobal,contAux);
		printf("Los datos de la tabla global son: \n");
		printf("FD: %d --- Direccion: %s --- Cantidad de veces abierta: %d\n",datosGlobal->fd,datosGlobal->direccion,datosGlobal->cantidadOpen);
	}
}

void stopKernel(){
	pthread_mutex_lock(&mux_planificacionBloqueada);
	if(!planificacionBloqueada){
		puts("#####DETENEMOS LA PLANIFICACION#####");
		planificacionBloqueada=true;
		//LOCK_PLANIF;
		MUX_LOCK(&mux_new);MUX_LOCK(&mux_ready);MUX_LOCK(&mux_exec);MUX_LOCK(&mux_block);MUX_LOCK(&mux_exit);
	}else{
		puts("#####REANUDAMOS LA PLANIFICACION#####");
		planificacionBloqueada=false;
		//UNLOCK_PLANIF;
		MUX_UNLOCK(&mux_new);MUX_UNLOCK(&mux_ready);MUX_UNLOCK(&mux_exec);MUX_UNLOCK(&mux_block);MUX_UNLOCK(&mux_exit);

	}
	pthread_mutex_unlock(&mux_planificacionBloqueada);
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
