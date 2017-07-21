#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "manejadoresMem.h"
#include "manejadoresCache.h"
#include "auxiliaresMemoria.h"
#include "structsMem.h"
#include "memoriaConfigurators.h"
#include "apiMemoria.h"

#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposErrores.h>

int pid_free, pid_inv, free_page;
int marcos_inv; // cantidad de frames que ocupa la tabla de invertidas en MEM_FIS

extern float retardo_mem;   // latencia de acceso a Memoria Fisica
extern tMemoria *memoria;   // configuracion de Memoria
extern char *MEM_FIS;       // MEMORIA FISICA
extern char *CACHE;         // memoria CACHE
extern int *CACHE_accs;     // vector de accesos a CACHE
tCacheEntrada *CACHE_lines; // vector de lineas a CACHE

extern int sock_kernel;
extern int stack_size;
t_list *proc_lims; // almacena t_procCtl: limites de stack por PID

// FUNCIONES Y PROCEDIMIENTOS DE MANEJO DE MEMORIA


void liberarEstructurasMemoria(void){
	puts("Se procede a liberar todas las estructuras de Memoria.");

	liberarConfiguracionMemoria();
	freeAndNULL((void **) &MEM_FIS);
	freeAndNULL((void **) &CACHE);
	freeAndNULL((void **) &CACHE_lines);
	freeAndNULL((void **) &CACHE_accs);
}

int setupMemoria(void){
	int stat;

	retardo(memoria->retardo_memoria);

	pid_free  = -2;
	pid_inv   = -3;
	free_page = -1;

	if ((MEM_FIS = malloc(memoria->marcos * memoria->marco_size)) == NULL){
		puts("No se pudo crear el espacio de Memoria");
		return MEM_EXCEPTION;
	}
	populateInvertidas();

	// inicializamos la "CACHE"
	if ((stat = setupCache()) != 0)
		return MEM_EXCEPTION;

	proc_lims = list_create();
	return 0;
}

void populateInvertidas(void){

	int i, off, fr;

	tEntradaInv entry_inv  = {.pid = pid_inv,  .pag = free_page};
	tEntradaInv entry_free = {.pid = pid_free, .pag = free_page};

	int size_inv_total = sizeof(tEntradaInv) * memoria->marcos;
	marcos_inv = ceil((float) size_inv_total / memoria->marco_size);


	// creamos las primeras entradas administrativas, una por cada `marco_invertido'
	for(i = off = fr = 0; i < marcos_inv; i++){
		memcpy(MEM_FIS + fr * memoria->marco_size + off, &entry_inv, sizeof(tEntradaInv));
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}

	// creamos las otras entradas libres, una por cada marco restante
	for(i = marcos_inv; i < memoria->marcos; i++){
		memcpy(MEM_FIS + fr * memoria->marco_size + off, &entry_free, sizeof(tEntradaInv));
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}
}

char *leerBytes(int pid, int page, int offset, int size){

	char *cont = NULL;

	char *buffer;
	if ((buffer = malloc(size + 1)) == NULL){
		puts("No se pudo crear espacio de memoria para un buffer");
		return NULL;
	}

	if ((cont = getCacheContent(pid, page)) == NULL){
		if ((cont = getMemContent(pid, page)) == NULL){
			puts("No pudo obtener el frame");
			return NULL;
		}
		insertarEnCache(pid, page, cont);
	}

	memcpy(buffer, cont + offset, size + 1);
	buffer[size] = '\0';
	return buffer;
}

char *getMemContent(int pid, int page){

	int frame;
	if ((frame = buscarEnMemoria(pid, page)) < 0){
		printf("Fallo buscar En Memoria el pid %d y pagina %d; \tError: %d\n", pid, page, frame);
		return NULL;
	}

	return MEM_FIS + frame * memoria->marco_size;
}

int buscarEnMemoria(int pid, int page){
	sleep(retardo_mem);

	tEntradaInv *entry;
	int cic, off, fr; // frame y offset para recorrer la tabla de invertidas
	int frame_ap = frameHash(pid, page); // aproximacion de frame buscado

	for(cic = 0; cic < memoria->marcos; cic++){

		gotoFrameInvertida(frame_ap, &fr, &off);
		entry = (tEntradaInv *) (MEM_FIS + fr * memoria->marco_size + off);

		if (pid == entry->pid && page == entry->pag)
			return frame_ap;
		frame_ap = (frame_ap+1) % memoria->marcos;
	}

	return FRAME_NOT_FOUND;
}

int frameHash(int pid, int page){
	char str1[20];
	char str2[20];
	sprintf(str1, "%d", pid);
	sprintf(str2, "%d", page);
	strcat(str1, str2);
	int frame_apr = atoi(str1) % memoria->marcos;
	return frame_apr;
}

void dumpMemStructs(void){ // todo: revisar

	int i;
	tEntradaInv *entry = (tEntradaInv*) MEM_FIS;

	puts("Comienzo dump Tabla de Paginas Invertida");
	printf("FRAME \t\t PID \t\t PAGINA\n");
	for (i = 0; i < memoria->marcos; ++i)
		printf("%d \t\t %d \t\t %d\n", i, (entry +i)->pid, (entry +i)->pag);
	puts("Fin dump Tabla de Paginas Invertida");

	puts("Comienzo dump Listado Procesos Activos");
	// todo: dumpear listado de procesos activos... No se ni lo que es un proc activo
	puts("Fin dump Listado Procesos Activos");

}

void dumpMemContent(int pid){

	if (pid < 0){
		int i, len, *pids;
		pids = obtenerPIDsKernel(&len);

		for (i = 0; i < len; ++i)
			dumpMemContent(pids[i]);
		return;
	}

	int pag;
	char *cont;
	int page_count = pageQuantity(pid);

	for (pag = 0; pag < page_count; ++pag){
		cont = getMemContent(pid, pag);
		printf("PID \t PAGINA\n%d \t %d \t\n", pid, pag);
		puts("CONTENIDO");
		DumpHex(cont, memoria->marco_size);
		puts("");
	}
}

int *obtenerPIDsKernel(int *len){

	char *buffer;
	tPackBytes *pb;
	int *pids;

	tPackHeader head  = {.tipo_de_proceso = MEM, .tipo_de_mensaje = PID_LIST};
	tPackHeader h_esp = {.tipo_de_proceso = KER, .tipo_de_mensaje = PID_LIST};
	informarResultado(sock_kernel, head);

	if (validarRespuesta(sock_kernel, head, &h_esp) != 0)
		return NULL;

	if ((buffer = recvGeneric(sock_kernel)) == NULL)
		return NULL;

	if ((pb = deserializeBytes(buffer)) == NULL)
		return NULL;

	*len = pb->bytelen;
	pids = malloc(*len);
	memcpy(pids, pb->bytes, *len);

	free(buffer);
	free(pb);
	return pids;
}

/* Registra el techo todo: ldalsdlaal
 */
//void registrarStackLim(int pid, int code_pages){}
//
//	int s_lim = memoria->marco_size * (code_pages + stack_size - 1);
//	t_procCtl *pc = {.pid = pid, .stack_lim = s_lim};
//	//pthread_mutex_lock(
//	list_add(proc_lims, pc);
//	//pthread_mutex_unlock(
//}


void DumpHex(const void* data, size_t size){

	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("| %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf(" ");
				}
				printf("| %s \n", ascii);
			}
		}
	}
}
