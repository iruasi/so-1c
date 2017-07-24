#ifndef FUNCIONESSYSCALLS_H_
#define FUNCIONESSYSCALLS_H_

void setupGlobales_syscalls(void);

int waitSyscall(const char *sem, int pid);
int signalSyscall(const char *sem);

int setGlobalSyscall(tPackValComp *val_comp);
t_valor_variable getGlobalSyscall(t_nombre_variable *var, bool *found);

void enqueuePIDtoSem(char *sem, int pid);
int *unqueuePIDfromSem(char *sem);

#endif /* FUNCIONESSYSCALLS_H_ */
