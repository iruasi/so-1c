#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <semaphore.h>

#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>

#include "kernelConfigurators.h"
#include "planificador.h"

extern tKernel *kernel;
extern t_valor_variable *shared_vals;

int getSemPosByID(const char *sem){

	int i;
	for (i = 0; i < kernel->sem_quant; ++i)
		if (strcmp(kernel->sem_ids[i], sem) == 0)
			return i;

	return -1; // todo: retorne error
}

void waitSyscall(const char *sem, int pid){

	int p = getSemPosByID(sem);
	kernel->sem_vals[p]--;
	if (kernel->sem_vals[p] < 0){
		puts("Ejecucion espera semaforo");
		blockByPID(pid);
	}
}

void signalSyscall(const char *sem, int pid){

	int p = getSemPosByID(sem);

	kernel->sem_vals[p]++;
	if (kernel->sem_vals[p] >= 0){
		unBlockByPID(pid);
		puts("Ejecucion continua por semaforo");
	}
}

int setGlobalSyscall(tPackValComp *val_comp){

	int i;
	int nlen = strlen(val_comp->nom) + 2; // espacio para el ! y el '\0'
	char *aux = NULL;
	for (i = 0; i < kernel->shared_quant; ++i){
		aux = realloc(aux, nlen); aux[0] = '!'; memcpy(aux + 1, val_comp->nom, nlen); aux[nlen] = '\0';

		if (strcmp(kernel->shared_vars[i], aux) == 0){
			shared_vals[i] = val_comp->val;
			free(aux);
			return 0;
		}
	}
	free(aux);
	return GLOBAL_NOT_FOUND;
}

t_valor_variable getGlobalSyscall(t_nombre_variable *var, bool* found){

	int i;
	int nlen = strlen(var) + 2; // espacio para el ! y el '\0'
	char *aux = NULL;
	*found = true;
	for (i = 0; i < kernel->shared_quant; ++i){
		aux = realloc(aux, nlen); aux[0] = '!'; memcpy(aux + 1, var, nlen); aux[nlen] = '\0';

		if (strcmp(kernel->shared_vars[i], aux) == 0){
			free(aux);
			return shared_vals[i];
		}
	}
	free(aux);
	*found = false;
	return GLOBAL_NOT_FOUND;
}
