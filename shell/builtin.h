#define CMD_MAX_LEN 128
#define PROC_MAX 128
#define BUF_MAX_LEN 128

int try_execute_builtin(char **);
void add_background_proc(pid_t, char*);
