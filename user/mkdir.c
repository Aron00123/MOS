#include <lib.h>
#include <args.h>

int flag[512];

void usage(void) {
	debugf("usage: mkdir <absolute target path>\n");
	exit(0);
}

int mkdir(const char *path) {
	int r;
    if (r = open(path, O_RDONLY) > 0) {
        close(r);
        if (!flag['p']) {
            debugf("mkdir: cannot create directory \'%s\': File exists\n", path);
        }

        return;
    }
	if ((r = create(path, O_CREAT | FTYPE_DIR)) > 0) {
		debugf("mkdir %s: %d\n", path, r);
	}
	return r;
}

int main(int argc, char **argv) {
    ARGBEGIN
    {
    case 'p':
        flag[(u_char)ARGC()]++;
        break;
    default:
        usage();
    }
    ARGEND

    if (argc != 1) {
        // printf("1111\n");
        usage();
        exit(0);
    } else {
        int r;
        if ((r = mkdir(argv[0])) < 0) {
            if (!flag['p']) {
                printf("mkdir: cannot create directory \'%s\': No such file or directory\n", argv[1]);
            }
        } else {
            close(r);
            // printf("created path: ");
            // printf("%s\n", argv[1]);
        }
    }

    return 0;
}