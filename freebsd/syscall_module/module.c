#include <sys/types.h>
#include <sys/module.h>
#include <sys/systm.h>  /* uprintf */
#include <sys/errno.h>
#include <sys/param.h>  /* defines used in kernel.h */
#include <sys/kernel.h> /* types used in module initialization */
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>

static int offset = NO_SYSCALL;

static int print(struct thread *td, void *arg)
{
    printf("Kernel module from syscall \n");
    return 0;
}

static struct sysent sysent_struct =
{
    0,
    print
};


static int load(struct module *module, int cmd, void *arg)
{
    int error = 0;
    switch (cmd)
    {
        case MOD_LOAD:
            printf ("Syscall loaded at %d\n", offset);
	    break;
        case MOD_UNLOAD:
            printf ("Syscall unloaded from %d\n", offset);
            break;
        default:
            error = EINVAL;
            break;
    }
    return error;
}

SYSCALL_MODULE(syscall, &offset, &sysent_struct, load, NULL);
