struct tokens {
    int count;
    char **tokens;
    char* command;
    int cmd_len;
};

struct tokens* tokens_create(char *command);

int tokens_lookup(char* target, struct tokens* tokens);

char * tokens_get(struct tokens* ts, int index);

void tokens_destroy(struct tokens* ts);

char* get_full_path(char *cmd);
