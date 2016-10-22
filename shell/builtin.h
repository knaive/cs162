#define CMD_MAX_LEN 128
#define PROC_MAX 128
#define BUF_MAX_LEN 128

void try_execute_builtin(char *argv[]);
void add_background_proc(pid_t, char*);
