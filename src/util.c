/*
 * This file is part of udisks-glue.
 *
 * © 2010-2011 Fernando Tarlá Cardoso Lemos
 * © 2010 Jan Palus
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
#include <stdio.h>
#include <string.h>
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
    if ((pid = fork()) != 0) {
        if (pid == (pid_t)-1) {
            perror("fork");
            exit(1);
        }
        else {
            exit(0);
        }
    }

    umask(0);

    if (setsid() == (pid_t)-1) {
        perror("setsid");
        exit(EXIT_FAILURE);
    }

    if (chdir("/") == -1) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }
}

void close_descriptors()
{
    for (int i = 0; i < 3; ++i)
        close(i);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    open("/dev/null", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
}

static char *expand_path(const char *path)
{
    glob_t glob_buffer;
    if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &glob_buffer) != 0)
        return 0;

    char *expanded = glob_buffer.gl_pathc > 0 ? strdup(glob_buffer.gl_pathv[0]) : strdup(path);
    globfree(&glob_buffer);

    return expanded;
}

static int file_exists(const char *path)
{
    struct stat s;
    return stat(path, &s) == -1 ? 0 : 1;
}

const char *find_config_file()
{
    char *path;
    int res;

    path = expand_path("~/.udisks-glue.conf");
    if (file_exists(path))
        return path;
    else
        free(path);

    char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (!xdg_config_home)
        xdg_config_home = "~/.config";

    xdg_config_home = expand_path(xdg_config_home);
    res = asprintf(&path, "%s/udisks-glue/config", xdg_config_home);
    free(xdg_config_home);
    if (res == -1)
        perror("asprintf");
    else if (file_exists(path))
        return path;
    else
        free(path);

    path = expand_path(SYSCONFDIR "/udisks-glue.conf");
    if (file_exists(path))
        return path;
    else
        free(path);

    const char *xdg_config_dirs = getenv("XDG_CONFIG_DIRS");
    if (!xdg_config_dirs)
        xdg_config_dirs = SYSCONFDIR "/xdg";

    char *buf = strdup(xdg_config_dirs);
    char *tok = strtok(buf, ":");
    while (tok != NULL) {
        tok = expand_path(tok);
        res = asprintf(&path, "%s/udisks-glue/config", tok);
        free(tok);
        if (res == -1)
            perror("asprintf");
        else if (file_exists(path))
            return path;
        else
            free(path);
        tok = strtok(NULL, ":");
    }
    free(buf);

    return NULL;
}
