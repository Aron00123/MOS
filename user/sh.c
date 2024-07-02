#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()`"
#define MAXARGS 128
#define HISTORY_SIZE 20

#define TOK_INVAL     -1
#define TOK_NUL       0
#define TOK_WORD      1
#define TOK_IN        2 // "<"
#define TOK_OUT       3 // ">"
#define TOK_PIPE      4 // "|"
#define TOK_AND       5 // "&&"
#define TOK_OR        6 // "||"
#define TOK_APPEND    7 // ">>"
#define TOK_BACKQUOTE 8 // "`"
#define TOK_SEM       9 // ";"
#define TOK_BACKGROUND 10 // "&"

int current_line = -1;
int new_line = -1;
int fd_out = 31;
int fd_in = 32;
int fd_history;
int first_history_req = 1;
int is_back = 0;
int background_count = 0;
char saved_history[HISTORY_SIZE][1024];
char background_buf[1024];
char background_envs[16][1024];




void deal_requests(char *s1, char *s2) {
    if (strcmp(s1, "history") == 0) {
        print_history();
        exit(0);
    } else if (strcmp(s1, "fg") == 0) {
        command_fg(s2);
        exit(0);
    } else if (strcmp(s1, "kill") == 0) {
        command_kill(s2);
        exit(0);
    } else if (strcmp(s1, "jobs") == 0) {
        command_jobs();
        exit(0);
    }
}

void print_history() {
    // Start from the next line after new_line
    int current_line = (new_line + 1) % HISTORY_SIZE;

    // Iterate until we return to new_line
    while (current_line != new_line) {
        // Check if the current history entry is not empty
        if (saved_history[current_line][0] != '\0') {
            printf("%s\n", saved_history[current_line]);
        }
        // Move to the next line in circular buffer
        current_line = (current_line + 1) % HISTORY_SIZE;
    }

    // Print the new_line entry
    printf("%s\n", saved_history[new_line]);
}

void command_fg(char *s) {
    u_int job_id = 0;
    int length = strlen(s);
    for (int i = 0; s[i] != '\0'; i++) {
        job_id = job_id * 10 + (s[i] - '0');
    }
    if (job_id > background_count) {
        printf("fg: job (%d) do not exist\n", job_id);
    } else {
        int env_id = syscall_get_monitor_id(job_id - 1);
        int stat = syscall_is_done(job_id - 1);
        if (stat == 1) {
            printf("fg: (0x%08x) not running\n", env_id);
        } else {
            wait(env_id);
        }
    }
}

void command_jobs() {
    for (int i = 0; i < 16; i++) {
        u_int env_id;
        char str[1024];
        int stat = syscall_is_done(i);
        if (stat == -1) {
            break;
        } else {
            env_id = syscall_get_monitor_id(i);
            if (stat == 0) {
                printf("[%d] %-10s 0x%08x %s\n", i + 1, "Running", env_id, background_envs[i]);
            } else {
                printf("[%d] %-10s 0x%08x %s\n", i + 1, "Done", env_id, background_envs[i]);
            }
        }
    }
}

void command_kill(char *s) {
    u_int job_id = 0;
    int length = strlen(s);
    for (int i = 0; s[i] != '\0'; i++) {
        job_id = job_id * 10 + (s[i] - '0');
    }
    if (job_id > background_count) {
        printf("fg: job (%d) do not exist\n", job_id);
    } else {
        int env_id = syscall_get_monitor_id(job_id - 1);
        int stat = syscall_is_done(job_id - 1);
        if (stat == 1) {
            printf("fg: (0x%08x) not running\n", env_id);
        } else {
            syscall_kill_job(job_id);
        }
    }
}



/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */
int _gettoken(char *s, char **p1, char **p2) {
    *p1 = *p2 = NULL;
    if (s == NULL) {
        return TOK_NUL;
    }

    while (strchr(WHITESPACE, *s) != NULL) {
        *s++ = '\0';
    }
    if (*s == '\0' || *s == '#') {
        return TOK_NUL;
    }

    if (*s =='\"') {
		s++;
		*p1 = s;
		while (*s && *s != '\"') {
			s++;
		}
		if (*s == '\"') {
			*s = '\0';
			s++;
			*p2 = s;
			return TOK_WORD;
		} 
	}

    if (strchr(SYMBOLS, *s) != NULL) {
        int result;
        *p1 = s;
        switch (*s) {
            case '<':
                *s++ = '\0';
                result = TOK_IN;
                break;

            case ';':
                *s++ = '\0';
                result = TOK_SEM;
                break;

            case '>':
                if (*(s + 1) == '>') {
                    *s++ = '\0';
                    *s++ = '\0';
                    result = TOK_APPEND;
                } else {
                    *s++ = '\0';
                    result = TOK_OUT;
                }
                break;

            case '`':
                *s++ = 0;
                *p1 = s;
                while (*s && *s != '`') {
                    s++;
                }
                if (*s == '`') {
                    *s = '\0';
                    s++;
                    *p2 = s;
                    // printf("here\n");
                    result = TOK_BACKQUOTE;
                }
                break;

            case '|':
                if (*(s + 1) == '|') {
                    *s++ = '\0';
                    *s++ = '\0';
                    result = TOK_OR;
                } else {
                    *s++ = '\0';
                    result = TOK_PIPE;
                }
                break;

            case '&':
                *s++ = '\0';
                if (*s == '&') {
                    *s++ = '\0';
                    result = TOK_AND;
                } else {
                    int loc;
                    for (loc = 1; strchr(WHITESPACE, *(s + loc)); loc++) {
                        ;
                    }
                    if (*(s + loc) == '\0') {
                        *(s + 1) = '\0';
                        *s = '\0';
                        // printf("here\n");
                        result = TOK_BACKGROUND;
                    } else {
                        result = TOK_INVAL;
                    }
                }
                break;

            default:
                result = TOK_INVAL;
                break;
        }
        *p2 = s;
        return result;
    } else {
        *p1 = s;
        while (*s != '\0' && strchr(WHITESPACE SYMBOLS, *s) == NULL) {
            s++;
        }
        *p2 = s;
        return TOK_WORD;
    }
}

int gettoken(char *s, char **p1) {
    static int _t;
    static char *_s;
    static char *_p1;

    if (s != NULL) {
        _t = _gettoken(s, &_p1, &_s);
        return TOK_NUL;
    } else {
        int t = _t;
        *p1 = _p1;
        _t = _gettoken(_s, &_p1, &_s);
        return t;
    }
}

#define MAXARGS 128
char backquotebuf[1024];

int parsecmd(int *argc, char **argv, int *rightpipe) {
    *argc = 0;
    while (1) {
        char *w;
        int t = gettoken(NULL, &w), r, fd, p[2];
        int son;
        switch (t) {
            case TOK_NUL:
                return TOK_NUL;

            case TOK_OR:
                return TOK_OR;

            case TOK_AND:
                return TOK_AND;

            case TOK_SEM:
                son = fork();
                if (son == 0) {
                    return TOK_SEM;
                } else {
                    ipc_recv(NULL, NULL, NULL);
                    dup(fd_in, 0);
                    dup(fd_out, 1);
                    return parsecmd(argc, argv, rightpipe);
                }

            case TOK_WORD:
                if (*argc >= MAXARGS) {
                    debugf("too many arguments\n");
                    exit(0);
                }
                argv[(*argc)++] = w;
                break;

            case TOK_IN:
                if (gettoken(NULL, &w) != TOK_WORD) {
                    debugf("syntax error: < not followed by word\n");
                    exit(0);
                }
                fd = open(w, O_RDONLY);
                if (fd < 0) {
                    debugf("open error: %d\n", fd);
                    exit(0);
                }
                dup(fd, 0);
                close(fd);
                break;

            case TOK_OUT:
                if (gettoken(NULL, &w) != TOK_WORD) {
                    debugf("syntax error: > not followed by word\n");
                    exit(0);
                }
                fd = open(w, O_WRONLY);
                if (fd < 0) {
                    debugf("failed to open: %d\n", fd);
                    exit(0);
                }
                dup(fd, 1);
                close(fd);
                break;

            case TOK_PIPE:
                pipe(p);
                *rightpipe = fork();
                if (*rightpipe < 0) {
                    debugf("fork error");
                    exit(0);
                } else if (*rightpipe == 0) {
                    dup(p[0], 0);
                    close(p[0]);
                    close(p[1]);
                    return parsecmd(argc, argv, rightpipe);
                } else {
                    dup(p[1], 1);
                    close(p[1]);
                    r = close(p[0]);
                    return TOK_PIPE;
                }
                break;

            case TOK_BACKGROUND:
                son = fork();
                if (son == 0) {
                    is_back = 1;
                    return TOK_NUL;
                } else {
                    syscall_add_monitor(son, background_buf);
                    return TOK_BACKGROUND;
                }

            case TOK_BACKQUOTE:
                // int p[2];
                pipe(p);
                int son = fork();
                if (son == 0) {
                    close(p[0]);
                    dup(p[1], 1);
                    close(p[1]);
                    gettoken(w, NULL);
                    return parsecmd(argc, argv, rightpipe);
                } else {
                    close(p[1]);
                    int loc = 0;
                    // printf("here\n");
                    // printf("%d\n", sizeof(backquotebuf));
                    // if ((r = read(p[0], backquotebuf + loc, sizeof(backquotebuf) - 1 - loc)) < 0) {
                    //     printf("FUCK YOU\n");
                    // }
                    while ((r = read(p[0], backquotebuf + loc, sizeof(backquotebuf) - 1 - loc)) > 0) {
                        // printf("%d\n", loc);
                        // printf("here\n");
                        loc += r;
                    }
                    // printf("1111111\n");
                    close(p[0]);
                    ipc_recv(NULL, NULL, NULL);
                    if (loc >= 0) {
                        backquotebuf[loc] = '\0';
                        if (loc > 0 && backquotebuf[loc - 1] == '\n') {
                            backquotebuf[loc - 1] = '\0';
                        }
                        // printf("%s\n", backquotebuf);
                        argv[(*argc)++] = backquotebuf;
                    }
                    break;
                }

            case TOK_APPEND:
                gettoken(NULL, &w);
                fd = open(w, O_WRONLY | O_APPEND);
                // printf("%d\n", fd);
                dup(fd, 1);
                close(fd);
                break;

            default:
                return TOK_INVAL;
        }
    }
}

void runcmd(char *s) {
    int value = 0;
    int token_prev = TOK_NUL;

    gettoken(s, NULL);

    while (1) {
        int argc;
        char *argv[MAXARGS] = {NULL};
        int rightpipe;
        int next_token = parsecmd(&argc, argv, &rightpipe), son;
        // printf("argc: %d\n", argc);
        switch (next_token) {
            case TOK_NUL:
                if (value && token_prev == TOK_OR) {
                    value = token_prev == TOK_OR;
                } else if (!value && token_prev == TOK_AND) {
                    value = token_prev != TOK_AND;
                } else {
                    deal_requests(argv[0], argv[1]);
                    // printf("argc: %d\n", argc);
                    // printf("%s\n", argv[0]);
                    // printf("%s\n", argv[1]);
                    son = spawn(argv[0], argv);
                    close_all();
                    if (son > 0) {
                        value = (ipc_recv(NULL, NULL, NULL) == 0);
                    } else {
                        debugf("spawn %s: %d\n", argv[0], son);
                    }
                }
                token_prev = TOK_NUL;
                exit(0);

            case TOK_PIPE:
                deal_requests(argv[0], argv[1]);
                son = spawn(argv[0], argv);
                close_all();
                if (son > 0) {
                    ipc_recv(NULL, NULL, NULL);
                    ipc_recv(NULL, NULL, NULL);
                } else {
                    debugf("spawn %s: %d\n", argv[0], son);
                }
                exit(0);

            case TOK_OR:
                if (!value && token_prev == TOK_AND) {
                    value = token_prev != TOK_AND;
                } else if (value && token_prev == TOK_OR) {
                    value = token_prev == TOK_OR;
                } else {
                    deal_requests(argv[0], argv[1]);
                    son = spawn(argv[0], argv);
                    if (son > 0) {
                        value = (ipc_recv(NULL, NULL, NULL) == 0);
                    } else {
                        debugf("spawn %s: %d\n", argv[0], son);
                    }
                }
                token_prev = TOK_OR;
                break;

            case TOK_AND:
                if (!value && token_prev == TOK_AND) {
                    value = token_prev != TOK_AND;
                } else if (value && token_prev == TOK_OR) {
                    value = token_prev == TOK_OR;
                } else {
                    deal_requests(argv[0], argv[1]);
                    // printf("%s\n", argv[0]);
                    son = spawn(argv[0], argv);
                    if (son > 0) {
                        value = (ipc_recv(NULL, NULL, NULL) == 0);
                    }
                }
                token_prev = TOK_AND;
                break;

            case TOK_SEM:
                deal_requests(argv[0], argv[1]);
                son = spawn(argv[0], argv);
                // close_all();
                if (son > 0) {
                    // ipc_recv(NULL, NULL, NULL);
                    ipc_recv(NULL, NULL, NULL);
                }
                token_prev = TOK_SEM;
                exit(0);

            case TOK_BACKGROUND:
                token_prev = TOK_BACKGROUND;
                exit(777);

            default:
                break;
        }
    }
}

void handle_arrow_keys(char arrow_key, char *buf, int *i) {
    // printf("11112132\n");
    if (arrow_key == 'A') {  // 上箭头
        printf("11112132\n");
        handle_up_arrow(buf, i);
    } else if (arrow_key == 'B') {  // 下箭头
        handle_down_arrow(buf, i);
    }
}

void handle_up_arrow(char *buf, int *i) {
    if (first_history_req) {
        if (is_history_empty_or_first_entry()) {
            (*i)--;
        } else {
            update_history_on_up(buf, i);
            first_history_req = 0;
        }
    } else {
        if (is_history_empty_or_first_entry()) {
            reset_current_input(buf, i);
        } else {
            printf("\33[2K\n$ ");
            current_line--;
            current_line += HISTORY_SIZE;
            current_line %= HISTORY_SIZE;
            strcpy(buf, saved_history[current_line]);
            printf("%s", buf);
            *i = strlen(buf) - 1;
        }
    }
}

void handle_down_arrow(char *buf, int *i) {
    if (first_history_req) {
        (*i)--;
    } else {
        if (current_line % HISTORY_SIZE == new_line % HISTORY_SIZE) {
            (*i)--;
        } else {
            update_history_on_down(buf, i);
        }
    }
}

int is_history_empty_or_first_entry() {
    return (current_line - 1 + HISTORY_SIZE) % HISTORY_SIZE == (new_line + 1) % HISTORY_SIZE || saved_history[(current_line - 1 + HISTORY_SIZE) % HISTORY_SIZE][0] == '\0' || current_line == -1;
}

void reset_current_input(char *buf, int *i) {
    printf("\33[2K\n$ ");
    strcpy(buf, saved_history[current_line]);
    printf("%s", buf);
    *i = strlen(buf) - 1;
}

void update_history_on_up(char *buf, int *i) {
    printf("\33[2K\n$ ");
    new_line = (new_line + 1) % HISTORY_SIZE;
    buf[*i] = '\0';
    current_line = (current_line - 1 + HISTORY_SIZE) % HISTORY_SIZE;
    strcpy(saved_history[new_line], buf);
    strcpy(buf, saved_history[current_line]);
    printf("%s", buf);
    *i = strlen(buf) - 1;
}

void update_history_on_down(char *buf, int *i) {
    printf("\33[2K\n$ ");
    current_line = (current_line + 1) % HISTORY_SIZE;
    strcpy(buf, saved_history[current_line]);
    printf("%s", buf);
    *i = strlen(buf) - 1;
}

void process_newline(char *buf, int i) {
    // printf("fd_history: %d\n", fd_history);
    write(fd_history, buf, i);
    if (first_history_req || current_line == -1) {
        new_line = (new_line + 1) % HISTORY_SIZE;
        current_line = new_line;
    }
    buf[i] = '\0';
    strcpy(saved_history[new_line], buf);
    saved_history[new_line][i] = '\0';
    current_line = (new_line + 1) % HISTORY_SIZE;
    buf[i] = 0;
}

void readline(char *buf, u_int n) {
	int r;
	for (int i = 0; i < n; i++) {
		if ((r = read(0, buf + i, 1)) != 1) {
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit(0);
		}
        if (buf[i] == '\033') {
            // printf("1111111\n");
            char seq[2] = {0};
            if (read(0, seq, 2) == 2 && seq[0] == '[') {
                handle_arrow_keys(seq[1], buf, &i);
                continue;
            }
        }
		if (buf[i] == '\b' || buf[i] == 0x7f) {
			if (i > 0) {
				i -= 2;
			} else {
				i = -1;
			}
			if (buf[i] != '\b') {
				printf("\b");
			}
		}
		if (buf[i] == '\r' || buf[i] == '\n') {
			// buf[i] = 0;
            // printf("%s\n", buf);
            process_newline(buf, i);
			return;
		}
        // if (buf[i] == '#') {
        //     buf[i] = 0;
        //     char *buf_trash[1024];
        //     while ((r = read(0, buf_trash, 1)) == 1 && buf_trash[0] != '\r' && buf_trash[0] != '\n') {
        //         ;
        //     }
        //     // printf("%s\n", buf);
        //     return;
        // }
	}
	debugf("line too long\n");
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;
}

char buf[1024];

void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit(0);
}

int main(int argc, char **argv) {
	int r;
	int interactive = iscons(0);
	int echocmds = 0;
    dup(0, fd_in);
	dup(1, fd_out);
	printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	printf("::                                                         ::\n");
	printf("::                     MOS Shell 2024                      ::\n");
	printf("::                                                         ::\n");
	printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");

    // close(0);
    fd_history = open(".mosh_history", O_TRUNC | O_CREAT | O_RDWR);
    // printf("fd_history: %d\n", fd_history);
	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	if (argc > 1) {
		usage();
	}
	if (argc == 1) {
		close(0);
		if ((r = open(argv[0], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[0], r);
		}
		user_assert(r == 0);
	}
	for (;;) {
		if (interactive) {
			printf("\n$ ");
		}
        first_history_req = 1;
		readline(buf, sizeof buf);
        // printf("3333333\n");

		if (buf[0] == '#') {
			continue;
		}
		if (echocmds) {
			printf("# %s\n", buf);
		}
        strcpy(background_buf, buf);
		if ((r = fork()) < 0) {
			user_panic("fork: %d", r);
		}
        if (r == 0) {
            is_back = 0;
            runcmd(buf);
            exit(0);
        } else {
            if (ipc_recv(NULL, NULL, NULL) == 777) {
                strcpy(background_envs[background_count++], background_buf);  
            }
        }
	}
	return 0;
}
