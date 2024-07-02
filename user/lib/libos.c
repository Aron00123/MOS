#include <env.h>
#include <lib.h>
#include <mmu.h>

void exit(int result) {
	// After fs is ready (lab5), all our open files should be closed before dying.
#if !defined(LAB) || LAB >= 5
	close_all();
#endif

    syscall_ipc_try_send(env->env_parent_id,result,NULL,0);
    syscall_is_monitor_activer();
	syscall_env_destroy(0);
	user_panic("unreachable code");
}

const volatile struct Env *env;
extern int main(int, char **);

void libmain(int argc, char **argv) {
	// set env to point at our env structure in envs[].
	env = &envs[ENVX(syscall_getenvid())];

	// call user main routine
	int result = main(argc, argv);

	// exit gracefully
	exit(result);
}
