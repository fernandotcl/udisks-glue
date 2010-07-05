/*
 * This file is part of udisks-glue.
 *
 * © 2010 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glib.h>
#include <glob.h>
#include <stdlib.h>
#include <unistd.h>

gchar *str_replace(gchar *string, gchar *search, gchar *replacement)
{
    gchar *str;
    gchar **arr = g_strsplit(string, search, -1);
    if (arr != NULL && arr[0] != NULL)
        str = g_strjoinv(replacement, arr);
    else
        str = g_strdup(string);

    g_strfreev(arr);
    return str;
}

void run_command(const char *command)
{
    if (fork() == 0) {
        if (fork() == 0) {
            static const char *shell = NULL;
            if (!shell) {
                shell = getenv("SHELL");
                if (!shell)
                    shell = "/bin/sh";
            }
            execl(shell, shell, "-c", command, NULL);
        }
        exit(0);
    }
    wait(NULL);
}

void daemonize()
{
    pid_t pid;
    if ((pid = fork()) != 0)
        exit(0);
 
    umask(0);
    setsid();
    chdir("/");

    for (int i = 0; i < 3; ++i)
        close(i);
 
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    open("/dev/null", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
}

int file_exists(const char *path)
{
    static glob_t glob_buffer;
    if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &glob_buffer) != 0)
        return 0;

    const char *expanded = glob_buffer.gl_pathc > 0 ? glob_buffer.gl_pathv[0] : path;

    struct stat s;
    return stat(expanded, &s) == -1 ? 0 : 1;
}
