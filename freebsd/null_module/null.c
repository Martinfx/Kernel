#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>

static d_open_t null_open;
static d_close_t null_close;
static d_read_t null_read;
static d_write_t null_write;

static struct cdevsw null_cdevsw = {
    .d_version = D_VERSION,
    .d_open    = null_open,
    .d_close   = null_close,
    .d_read    = null_read,
    .d_write   = null_write,
    .d_name    = "null2"
};

static struct cdev *null_dev;

static int
null_open(struct cdev *dev, int oflags, int devtype, struct thread *td) {
    uprintf("Open null device.\n");
    return 0;
}

static int
null_close(struct cdev *dev, int oflags, int devtype, struct thread *td) {
    uprintf("Close null device.\n");
    return 0;
}

static int
null_read(struct cdev *dev, struct uio *uio, int ioflag) {
    uprintf("Read null device.\n");
    return 0;
}

static int
null_write(struct cdev *dev, struct uio *uio, int ioflag) {
    uprintf("Write null device.\n");
    return 0;
}

static int
null_modevent(module_t mod __unused, int event, void *arg __unused)
{
    int error = 0;
    switch (event) {
    case MOD_LOAD:
        null_dev = make_dev(&null_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "null2");
        uprintf("Echo driver loaded.\n");
        break;
    case MOD_UNLOAD:
        destroy_dev(null_dev);
        uprintf("Echo driver unloaded.\n");
        break;
    default:
        error = EOPNOTSUPP;
        break;
    }
    return (error);
}
DEV_MODULE(null2, null_modevent, NULL);
