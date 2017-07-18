#ifndef FUNCIONESSYSCALLS_H_
#define FUNCIONESSYSCALLS_H_


int waitSyscall(const char *sem, int pid);
int signalSyscall(const char *sem, int pid);

int setGlobalSyscall(tPackValComp *val_comp);
t_valor_variable getGlobalSyscall(t_nombre_variable *var, bool* found);

#endif /* FUNCIONESSYSCALLS_H_ */
