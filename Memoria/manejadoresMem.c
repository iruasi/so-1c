#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <semaphore.h>

#include <commons/log.h>

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
int size_inv;   // size en bytes de la tabla de invertidas. Util p/ controlar recorrido sobre ella

extern float retardo_mem;   // latencia de acceso a Memoria Fisica
extern tMemoria *memoria;   // configuracion de Memoria
extern char *MEM_FIS;       // MEMORIA FISICA
extern char *CACHE;         // memoria CACHE
extern int *CACHE_accs;     // vector de accesos a CACHE
tCacheEntrada *CACHE_lines; // vector de lineas a CACHE
extern t_log * logTrace;
extern int sock_kernel;
extern sem_t semPidList;
extern sem_t fin_recv;

// FUNCIONES Y PROCEDIMIENTOS DE MANEJO DE MEMORIA


void liberarEstructurasMemoria(void){
	//puts("Se procede a liberar todas las estructuras de Memoria.");
	log_trace(logTrace,"se procede a liberar todas las estructuras de memoria");
	liberarConfiguracionMemoria();
	freeAndNULL((void **) &MEM_FIS);
	freeAndNULL((void **) &CACHE);
	freeAndNULL((void **) &CACHE_lines);
	freeAndNULL((void **) &CACHE_accs);
}

int setupMemoria(void){
	int stat;
	log_trace(logTrace,"setup memoria");
	retardo(memoria->retardo_memoria);

	pid_free  = -2;
	pid_inv   = -3;
	free_page = -1;

	if ((MEM_FIS = malloc(memoria->marcos * memoria->marco_size)) == NULL){
		log_error(logTrace,"no se pudo crear el espacio de memoria");
		puts("No se pudo crear el espacio de Memoria");
		return MEM_EXCEPTION;
	}
	populateInvertidas();

	// inicializamos la "CACHE"
	if ((stat = setupCache()) != 0)
		return MEM_EXCEPTION;

	return 0;
}

void populateInvertidas(void){
	log_trace(logTrace,"funcion populate invertidas");
	int i, off, fr;

	tEntradaInv entry_inv  = {.pid = pid_inv,  .pag = free_page};
	tEntradaInv entry_free = {.pid = pid_free, .pag = free_page};

	size_inv = sizeof(tEntradaInv) * memoria->marcos;
	int marcos_inv = ceil((float) size_inv / memoria->marco_size);

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
	log_trace(logTrace,"funcion leerbytes [PID %d] page %d offset %d size %d",pid,page,offset,size);
	char *cont = NULL;

	char *buffer;
	if ((buffer = malloc(size + 1)) == NULL){
		log_error(logTrace,"no se pudo crear espacio de memoria para un buffer");
		puts("No se pudo crear espacio de memoria para un buffer");
		return NULL;
	}

	if ((cont = getCacheContent(pid, page)) == NULL){
		if ((cont = getMemContent(pid, page)) == NULL){
			log_error(logTrace,"no se pudo obtener el frame");
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
	log_trace(logTrace,"funcion get mem content[PID %d],page %d",pid,page);
	int frame;
	if ((frame = buscarEnMemoria(pid, page)) < 0){
		log_error(logTrace,"Frame no encontrado (pid %d , pag %d)", pid, page);
		return NULL;
	}

	return MEM_FIS + frame * memoria->marco_size;
}

int buscarEnMemoria(int pid, int page){
	sleep(retardo_mem);
	log_trace(logTrace,"funcion buscar en memoria[PID %d],page %d",pid,page);

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
	log_trace(logTrace,"funcion frame hash [PID %d],page %d",pid,page);
	char str1[20];
	char str2[20];
	sprintf(str1, "%d", pid);
	sprintf(str2, "%d", page);
	strcat(str1, str2);
	int frame_apr = atoi(str1) % memoria->marcos;
	return frame_apr;
}

void dumpMemStructs(void){
	log_trace(logTrace,"funcion dump mem structs");
	int i;
	tEntradaInv *entry = (tEntradaInv*) MEM_FIS;
	FILE * fileDump = txt_open_for_append("dumpCache.txt");
	puts("Comienzo dump Tabla de Paginas Invertida");
	char * string = string_new();
	string = string_from_format("\n\n Comienzo dump tabla de paginas invertida\n");
	txt_write_in_file(fileDump,string);


	printf("FRAME \t\t PID \t\t PAGINA\n");
	string = string_from_format("FRAME \t\t PID \t\t PAGINA\n");
	txt_write_in_file(fileDump,string);
	for (i = 0; i < memoria->marcos; ++i){
		printf("%d \t\t %d \t\t %d\n", i, (entry +i)->pid, (entry +i)->pag);
		string = string_from_format("%d \t\t %d \t\t %d\n", i, (entry +i)->pid, (entry +i)->pag);
		txt_write_in_file(fileDump,string);
	}
	puts("Fin dump Tabla de Paginas Invertida");
	string = string_from_format("Fin dump tabla de paginas invertida\n");
	txt_write_in_file(fileDump,string);
	txt_close_file(fileDump);
}

void dumpMemContent(int pid){
	log_trace(logTrace,"funcion dump mem content[PID %d]",pid);
	if (pid < 0){
		int i, len, *pids;
		pids = obtenerPIDsKernel(&len);

		for (i = 0; i < len; ++i)
			dumpMemContent(pids[i]);
		return;
	}

	int pag, found;
	char *cont;
	int page_count = pageQuantity(pid);
	FILE * fileDump = txt_open_for_append("dumpCache.txt");
	char * string = string_new();
	string = string_from_format("Dump mem content\n\n");
	txt_write_in_file(fileDump,string);
	txt_close_file(fileDump);
	for (pag = found = 0; found < page_count; ++pag){
		if ((cont = getMemContent(pid, pag)) == NULL)
			continue;
		found++;
		FILE * dump2 = txt_open_for_append("dumpCache.txt");
		printf("PID \t PAGINA\n%d \t %d \t\n", pid, pag);
		string = string_from_format("PID \t PAGINA\n%d \t %d \t\n", pid, pag);
		txt_write_in_file(dump2,string);
		puts("CONTENIDO");
		string = string_from_format("CONTENIDO\n");
		txt_write_in_file(dump2,string);
		txt_close_file(dump2);
		DumpHex(cont, memoria->marco_size);
		puts("");
	}
}

int *obtenerPIDsKernel(int *len){
	log_trace(logTrace,"funcion obtener pids kernel");
	char *buffer;
	tPackBytes *pb;
	int *pids;

	tPackHeader head  = {.tipo_de_proceso = MEM, .tipo_de_mensaje = PID_LIST};
	tPackHeader h_esp = {.tipo_de_proceso = KER, .tipo_de_mensaje = PID_LIST};
	tPackHeader header;

	log_trace(logTrace,"Se asignaron los headers");
	informarResultado(sock_kernel, head);
	log_trace(logTrace,"pase informar resultado");
	sem_wait(&semPidList);

	log_trace(logTrace,"Pase validar respuesta");
	if ((buffer = recvGeneric(sock_kernel)) == NULL)
		return NULL;
	log_trace(logTrace,"Pase recvGeneric");
	sem_post(&fin_recv);
	if ((pb = deserializeBytes(buffer)) == NULL)
		return NULL;
	log_trace(logTrace,"Pase deserializeBytes");
	*len = pb->bytelen;
	pids = malloc(*len);
	memcpy(pids, pb->bytes, *len);

	free(buffer);
	free(pb);
	return pids;
}

void DumpHex(const void* data, size_t size){
	log_trace(logTrace,"funcion dump hex");


	FILE * fileDump = txt_open_for_append("dumpCache.txt");
	char * string = string_new();

	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		string = string_from_format("%02X ", ((unsigned char*)data)[i]);
		txt_write_in_file(fileDump,string);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			string = string_from_format("  ");
			txt_write_in_file(fileDump,string);

			if ((i+1) % 16 == 0) {
				printf("| %s \n", ascii);
				string = string_from_format("| %s \n", ascii);
				txt_write_in_file(fileDump,string);

			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
					string = string_from_format(" ");
					txt_write_in_file(fileDump,string);

				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf(" ");
					string = string_from_format(" ");
					txt_write_in_file(fileDump,string);
				}
				printf("| %s \n", ascii);
				string = string_from_format("| %s \n", ascii);
				txt_write_in_file(fileDump,string);
			}
		}
	}
	txt_close_file(fileDump);
}
