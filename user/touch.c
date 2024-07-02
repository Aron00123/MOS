#include <lib.h>
#include <args.h>

void usage(void) {
	debugf("usage: touch <absolute target path>\n");
	exit(0);
}

int touch(const char *path) {
	int r;
	if (r = open(path, O_RDONLY) > 0) {
        // printf("touched fd: %d\n", r);
        close(r);
        debugf("touch: cannot create file \'%s\': File exists\n", path);
        exit(0);
    }
	if ((r = open(path, O_CREAT | FTYPE_REG)) > 0) {
        // printf("touched fd: %d\n", r);
        // debugf("r is: %d\n", r);
	}
    // printf("touched fd: %d\n", r);
	return r;
}

int main(int argc, char **argv) {

    if (argc == 1) {
        // printf("1111\n");
        usage();
        exit(0);
    } else {
        int r;
        if ((r = touch(argv[1])) < 0) {
            printf("touch: cannot touch \'%s\': No such file or directory\n", argv[1]);
        } else {
            close(r);
        }
    }

    return 0;
}