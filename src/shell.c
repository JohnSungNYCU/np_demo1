#include <stdio.h>    
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
//-----------------------------------------------------
#define MAX_SIZE 15000
//-----------------------------------------------------
char * cmd_buffer = NULL;
char * PATH = NULL;
char work_dir[MAX_SIZE];

#define OUTPUT_DEFAULT 0
#define OUTPUT_FILE 1
#define OUTPUT_PIPE 2
int next_action_type = OUTPUT_DEFAULT;
//-----------------------------------------------------
int is_builtin_cmd(char * cmd) {
    if (strstr(cmd, "exit") != NULL) {
        return 0;
    } else if (strstr(cmd, "printenv") != NULL) {
        return 0;
    } else if (strstr(cmd, "setenv") != NULL) {
        return 0;
    }
    return 1;
}
//-----------------------------------------------------
int do_builtin_cmd(char * cmd, char * argv[]) {
    char * value = NULL;
    char env_str[256];
    int rc = 0;
    if (strstr(cmd, "exit") != NULL) {
        return -1;
    } else if (strstr(cmd, "setenv") != NULL) {
        if ((argv[1] != NULL) && (argv[2] != NULL)) {
            rc = setenv(argv[1], argv[2], 1);
        }
        return 0;
    } else if (strstr(cmd, "printenv") != NULL) {
        if (argv[1] != NULL) {
            value = getenv(argv[1]);
        }
        if (value != NULL) {
            fprintf (stdout, "%s\n", value);
        }
        return 0;
    }
    return 0;
}
//-----------------------------------------------------
// Check cmd in the PATH env.
char * is_bin_cmd(char * cmd) {
    char * buffer = NULL;
    char * bin_path = NULL;
    char * test_path = NULL;

    buffer = strdup(PATH);
    test_path = malloc(strlen(PATH) + sizeof(cmd) + 1);

    bin_path = strtok(buffer, ":\r\n");
    if (bin_path != NULL) {
        sprintf (test_path, "%s/%s", bin_path, cmd);
        if (access (test_path, F_OK) == 0) {
            return test_path;
        }
    }

    do {
        bin_path = strtok(NULL, ":\r\n");
        if (bin_path != NULL) {
            sprintf (test_path, "%s/%s", bin_path, cmd);
            if (access (test_path, F_OK) == 0) {
                return test_path;
            }
        }
    } while (bin_path != NULL);

    free (buffer);

    return NULL;
}
//-----------------------------------------------------
int do_bin_cmd(char * filepath, char * argv[]) {
    int rc = 0;
    pid_t pid = 0;
    int status = 0;

    pid = fork();
    if (pid == 0) {
        rc = execv(filepath, argv);
    } else {
        wait(&status);
    }

    return rc;
}
//-----------------------------------------------------
void init_path(char * path) {
    int i = 0;

    if (path[0] == '/') {
        // abosolute
        strcpy (work_dir, path);
    } else {
        realpath(path, work_dir);
    }
    int len = strlen(work_dir);
    for (i = len-1; i >= 0; i--) {
        if (work_dir[i] == '/') {
            work_dir[i+1] = 0;
            break;
        }
    }

    PATH = malloc(MAX_SIZE);
    sprintf (PATH, "bin:.");
    setenv("PATH", PATH, 1);

    return;
}
//-----------------------------------------------------
struct _NUMBER_PIPE {
    int fd[2];
    int type;
    int count;
};
//-----------------------------------------------------
struct _CMD {
    char * cmd;
    int argc;
    char * argv[128]; // each cmd max 256 characters
    int pipe_id;
};
//-----------------------------------------------------
struct _PROJ1 {
    int cmd_count;
    struct _CMD * cmd[5000];
    struct _NUMBER_PIPE pipes[5];
    char * filename;
};
//-----------------------------------------------------
struct _PROJ1 proj1;
//-----------------------------------------------------
struct _CMD * parse_this_cmd(char * this_cmd) {
    int rc = 0;
    char * filepath = NULL;
    int i = 0;
    struct _CMD * _new_cmd = NULL;
    char * buffer_cmd = NULL;
    char * arg = NULL;
    char *saveptr = NULL;

    _new_cmd = calloc(1, sizeof(struct _CMD));

    buffer_cmd = strdup(this_cmd);
    _new_cmd->cmd = strdup(strtok_r(buffer_cmd, " ", &saveptr));
    if (_new_cmd->cmd == NULL) {
        free (buffer_cmd);
        return NULL;
    }
    _new_cmd->argv[_new_cmd->argc] = strdup(_new_cmd->cmd);
    _new_cmd->argc++;
    do {
        arg = strtok_r(NULL, " ", &saveptr);
        if (arg != NULL) {
            _new_cmd->argv[_new_cmd->argc] = strdup(arg);
            _new_cmd->argc++;
        }
    } while (arg != NULL);

    free (buffer_cmd);
    buffer_cmd = NULL;

    return _new_cmd;
}
//-----------------------------------------------------
int get_new_pipe() {
    int i = 0;
    for (i = 0 ; i < 5; i++) {
        if ((proj1.pipes[i].count <= 0) && (proj1.pipes[i].fd[0] <= 0) && (proj1.pipes[i].fd[1] <= 0)) {
            proj1.pipes[i].count = 0;
            return i;
        }
    }
    return -1;
}
//-----------------------------------------------------
char * parse_cmd(char * cmdline) {
    char * next_cmdline = NULL;
    char *buffer_all = strdup(cmdline);
    char *buffer_cpy = strdup(cmdline);
    char *delim = "|!>\r\n";
    char *res = NULL;
    char *test_name = NULL;
    struct _CMD * _new_cmd = NULL;
    char type = 0;
    char *saveptr = NULL;
    char *saveptr2 = NULL;
    int new_pipe = -1;

    res = strtok_r(buffer_all, delim, &saveptr);
    while (res != NULL) {
        _new_cmd = parse_this_cmd(res);
        if (_new_cmd == NULL) {
            break;
        }
        proj1.cmd[proj1.cmd_count++] = _new_cmd;
        type = buffer_cpy[res-buffer_all+strlen(res)];
        //printf("%c\n", buffer_cpy[res-buffer_all+strlen(res)]);
        res = strtok_r (NULL, delim, &saveptr);
        if (res == NULL) {
            break;
        }
        switch (type) {
        case '|':
        case '!':
            // if numbered pipe , terminates
            if ((res[0] >= '0') && (res[0] <= '9')) {
                // valid numbered pipe
                test_name = strtok_r(res, " ", &saveptr2);
                new_pipe = get_new_pipe();
                if (new_pipe >= 0) {
                    pipe(proj1.pipes[new_pipe].fd);
                    proj1.pipes[new_pipe].count = atoi(test_name);
                    proj1.pipes[new_pipe].type = (type == '!');
                    next_cmdline = &cmdline[test_name - buffer_all + strlen(test_name)];
                }
            }
            break;
        case '>':
            // setup filename and terminates
            // avoid the spaces
            test_name = strtok_r(res, " ", &saveptr2);
            if (test_name != NULL) {
                proj1.filename = strdup (test_name);
            }

            break;
        }
        if ((new_pipe >= 0) || (proj1.filename != NULL)) {
            break;
        }
    }

    free(buffer_cpy);
    free(buffer_all);

    return next_cmdline;
}
//-----------------------------------------------------
int exec_cmd(int cmd_index) {
    int fd[2];
    int fd_file = -1;
    int rc = 0;
    char * filepath = NULL;

    pipe(fd);
    switch (fork()) {
    case -1:
        break;
    case 0:
        if (rc == 0) {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO); // parent process : exec output redirect to 1
            close(fd[0]);
        }

        if (cmd_index < proj1.cmd_count-1) {
            exec_cmd(cmd_index+1);
        }
        break;
    default:
        if (cmd_index < proj1.cmd_count-1) {
            close(fd[0]); // parent process : no need read
            dup2(fd[1], STDOUT_FILENO); // parent process : exec output redirect to 1
            close(fd[1]);
        } else if (proj1.filename != NULL) {
            // last is redirect to file
            fd_file = open(proj1.filename, O_RDWR | O_CREAT, 0666);
            if (fd_file >= 0) {
                dup2(fd_file, STDOUT_FILENO);
                close(fd_file);
            }
        }

        // cmd2
        if (proj1.cmd[cmd_index] == NULL) {
            break;
        }
        filepath = is_bin_cmd(proj1.cmd[cmd_index]->cmd);
        if (filepath != NULL) {
            int id = proj1.cmd[cmd_index]->pipe_id;
            if ((id >= 0) && (proj1.pipes[id].fd[1] > 0)) {
                if (proj1.pipes[id].type) {
                    dup2(proj1.pipes[id].fd[1], STDERR_FILENO);
                }
                dup2(proj1.pipes[id].fd[1], STDOUT_FILENO);
                close(proj1.pipes[id].fd[1]);
                proj1.pipes[id].fd[1] = -1;
            }
            if (cmd_index == 0) {
                int id = 0;
                for (id = 0; id < 5; id++) {
                    if ((proj1.pipes[id].count == 0) && (proj1.pipes[id].fd[0] > 0)) {
                        dup2(proj1.pipes[id].fd[0], STDIN_FILENO);
                        close(proj1.pipes[id].fd[0]);
                        proj1.pipes[id].fd[0] = -1;
                        proj1.pipes[id].count = 0;
                        rc = 1;
                        break;
                    }
                }
            }
            execv(filepath, proj1.cmd[cmd_index]->argv);
            free (filepath);
        } else {
            fprintf (stderr, "Unknown command : [%s].\n", proj1.cmd[cmd_index]->cmd);
        }
        wait(NULL);
        break;
    }
    return 0;
}
//-----------------------------------------------------
int start_exec_cmd() {
    int fd0[2];
    int rc = 0;
    int cmd_index = 0;

    fd0[0] = dup(STDIN_FILENO);
    fd0[1] = dup(STDOUT_FILENO);

    switch (fork()) {
    case 0:
        rc = exec_cmd(cmd_index);
        rc = 1;
        break;
    case -1:
        rc = -1;
        break;
    default:
        wait(NULL);
        dup2(fd0[0], STDIN_FILENO);
        dup2(fd0[1], STDOUT_FILENO);
        break;
    }

    close (fd0[0]);
    close (fd0[1]);

    return rc;
}
//-----------------------------------------------------
int process_cmd() {
    int rc = 0;

    if (proj1.cmd[0] == NULL) {
        // empty command
        return 0;
    }
    if (proj1.cmd[0]->cmd == NULL) {
        // empty command
        return 0;
    }

    if (is_builtin_cmd(proj1.cmd[0]->cmd) == 0) {
        rc = do_builtin_cmd(proj1.cmd[0]->cmd, proj1.cmd[0]->argv);
    } else {
        rc = start_exec_cmd();
        if (rc < 0) {
            fprintf (stderr, "Unknown command : [%s]\n", proj1.cmd[0]->cmd);
        }
    }

    return rc;
}
//-----------------------------------------------------
void clear_cmd() {
    int i = 0;
    int j = 0;

    for (j = 0; j < proj1.cmd_count; j++) {
        for (i = 0; i < proj1.cmd[j]->argc; i++) {
            if (proj1.cmd[j]->argv[i] != NULL) {
                free(proj1.cmd[j]->argv[i]);
                proj1.cmd[j]->argv[i] = NULL;
            }
        }
        if (proj1.cmd[j]->cmd != NULL) {
            free (proj1.cmd[j]->cmd);
            proj1.cmd[j]->cmd = NULL;
        }
        free (proj1.cmd[j]);
        proj1.cmd[j] = NULL;
    }
    proj1.cmd_count = 0;
    if (proj1.filename != NULL) {
        free (proj1.filename);
        proj1.filename = NULL;
    }

    return;
}
//-----------------------------------------------------
int main(int argc, char * argv[]) {
    int rc = 0;
    char * next_cmd = NULL;
    int id = 0;

    init_path(argv[0]);

    cmd_buffer = malloc(MAX_SIZE);
    do {
        fprintf (stdout, "%% ");
        if (fgets(cmd_buffer, MAX_SIZE, stdin) == NULL) {
            fprintf (stdout, "\r      \r%% ");
            break;
        }
        next_cmd = cmd_buffer;
        do {
            next_cmd = parse_cmd(next_cmd);
            rc = process_cmd();
            for (id = 0; id < 5; id++) {
                if (proj1.pipes[id].fd[1] > 0) {
                    close (proj1.pipes[id].fd[1]);
                    proj1.pipes[id].fd[1] = -1;
                }
            }
            clear_cmd();
            if (next_cmd != NULL) {
                for (id = 0; id < 5; id++) {
                    if (proj1.pipes[id].count > 0) {
                        proj1.pipes[id].count --;
                    }
                }
            }
        } while (next_cmd != NULL);

        for (id = 0; id < 5; id++) {
            if (proj1.pipes[id].count > 0) {
                proj1.pipes[id].count --;
            }
        }
    } while (rc == 0);

    if (cmd_buffer != NULL) {
        free (cmd_buffer);
    }

    if (PATH != NULL) {
        free (PATH);
    }
    return 0;
}
