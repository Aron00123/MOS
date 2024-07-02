#include <lib.h>

int flag[512];

void usage()
{
    printf("usage: rm [-rf] file\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    ARGBEGIN
    {
    case 'r':
    case 'f':
        flag[(u_char)ARGC()]++;
        break;
    default:
        usage();
    }
    ARGEND

    if (argc != 1)
    {
        usage();
    }

    struct Stat s;
    int r = stat(argv[0], &s); // argv[0] is *file
    if (r < 0 && !flag['f'])
    {
        printf("rm: cannot remove '%s': No such file or directory\n", argv[0]);
        exit(0);
    }
    if (s.st_isdir && !flag['r'])
    {
        printf("rm: cannot remove '%s': Is a directory\n", argv[0]);
        exit(0);
    }
    remove(argv[0]);
    
    return 0;
}