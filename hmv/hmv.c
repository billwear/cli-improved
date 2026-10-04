#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>

/* -------------------------------------------------------------------------
 * ANSI Terminal Styling
 * ------------------------------------------------------------------------- */
#define ANSI_RESET     "\033[0m"
#define ANSI_BOLD      "\033[1m"
#define ANSI_UNDERLINE "\033[4m"
#define ANSI_BLUE      "\033[1;34m"
#define ANSI_GREEN     "\033[1;32m"
#define ANSI_CYAN      "\033[1;36m"
#define ANSI_RED       "\033[1;31m"
#define ANSI_YELLOW    "\033[1;33m"
#define ANSI_GRAY      "\033[0;90m"

/* -------------------------------------------------------------------------
 * Data Models
 * ------------------------------------------------------------------------- */
typedef struct {
    bool verbose;
    bool dry_run;
    bool git_aware;
    bool backup;
    bool update_only;
    bool json_output;
    bool colorize;
    char *search_str;
    char *replace_str;
} Config;

static bool is_first_json = true;

/* -------------------------------------------------------------------------
 * Substring Replacement Engine
 * ------------------------------------------------------------------------- */
char *str_replace(const char *orig, const char *rep, const char *with) {
    if (!orig || !rep) return strdup(orig ? orig : "");
    if (!with) with = "";
    
    char *result;
    char *ins;
    char *tmp;
    int len_rep = strlen(rep);
    int len_with = strlen(with);
    int len_front;
    int count = 0;

    ins = (char *)orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    if (count == 0) return strdup(orig);

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);
    if (!result) return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep;
    }
    strcpy(tmp, orig);
    return result;
}

/* -------------------------------------------------------------------------
 * Git Telemetry (Simplified Native Resolution)
 * ------------------------------------------------------------------------- */
bool is_git_tracked(const char *filepath) {
    pid_t pid = fork();
    if (pid == -1) return false;

    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        char *args[] = {"git", "ls-files", "--error-unmatch", (char *)filepath, NULL};
        execvp("git", args);
        _exit(127); 
    }

    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

bool execute_git_mv(const char *src, const char *dst, const Config *cfg) {
    if (cfg->dry_run) return true;
    
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        char *args[] = {"git", "mv", (char *)src, (char *)dst, NULL};
        execvp("git", args);
        _exit(127);
    }
    
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/* -------------------------------------------------------------------------
 * Cross-Device Copy & Fallback (EXDEV Resolution)
 * ------------------------------------------------------------------------- */
bool copy_and_unlink(const char *src, const char *dst, const Config *cfg) {
    if (cfg->dry_run) return true;

    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0) return false;

    struct stat st;
    if (fstat(fd_src, &st) != 0) {
        close(fd_src);
        return false;
    }

    int fd_dst = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd_dst < 0) {
        close(fd_src);
        return false;
    }

    char buf[8192];
    ssize_t nread;
    while ((nread = read(fd_src, buf, sizeof(buf))) > 0) {
        ssize_t nwritten = 0;
        while (nwritten < nread) {
            ssize_t res = write(fd_dst, buf + nwritten, nread - nwritten);
            if (res < 0) {
                if (errno == EINTR) continue;
                close(fd_src);
                close(fd_dst);
                unlink(dst);
                return false;
            }
            nwritten += res;
        }
    }

    struct timespec ts[2];
    ts[0] = st.st_atimespec;
    ts[1] = st.st_mtimespec;
    futimens(fd_dst, ts);

    close(fd_src);
    close(fd_dst);

    if (nread == 0) {
        return (unlink(src) == 0);
    }
    
    unlink(dst);
    return false;
}

/* -------------------------------------------------------------------------
 * Core Move Engine
 * ------------------------------------------------------------------------- */
void report_action(const char *src, const char *dst, const char *method, const char *status, const Config *cfg) {
    if (cfg->json_output) {
        printf("%s\n  {\n", is_first_json ? "" : ",");
        is_first_json = false;
        printf("    \"source\": \"%s\",\n", src);
        printf("    \"destination\": \"%s\",\n", dst);
        printf("    \"method\": \"%s\",\n", method);
        printf("    \"status\": \"%s\"\n  }", status);
    } else if (cfg->verbose || cfg->dry_run || strcmp(status, "failed") == 0) {
        const char *color_src = cfg->colorize ? ANSI_BLUE : "";
        const char *color_dst = cfg->colorize ? ANSI_GREEN : "";
        const char *color_stat = cfg->colorize ? (strcmp(status, "failed") == 0 ? ANSI_RED : ANSI_YELLOW) : "";
        const char *color_meth = cfg->colorize ? ANSI_GRAY : "";
        const char *reset     = cfg->colorize ? ANSI_RESET : "";
        
        printf("%s%-8s%s ", color_meth, method, reset);
        printf("%s%s%s -> %s%s%s", color_src, src, reset, color_dst, dst, reset);
        if (strcmp(status, "ok") != 0) {
            printf(" [%s%s%s]", color_stat, status, reset);
        }
        putchar('\n');
    }
}

void process_move(const char *src, const char *target_dir_or_file, const Config *cfg) {
    char final_dst[PATH_MAX];
    struct stat dst_st;
    bool dest_is_dir = (stat(target_dir_or_file, &dst_st) == 0 && S_ISDIR(dst_st.st_mode));

    char *src_copy = strdup(src);
    char *base = basename(src_copy);
    
    char *renamed_base = NULL;
    if (cfg->search_str) {
        renamed_base = str_replace(base, cfg->search_str, cfg->replace_str ? cfg->replace_str : "");
    } else {
        renamed_base = strdup(base);
    }

    if (dest_is_dir) {
        snprintf(final_dst, sizeof(final_dst), "%s/%s", target_dir_or_file, renamed_base);
    } else {
        strncpy(final_dst, target_dir_or_file, sizeof(final_dst) - 1);
        final_dst[sizeof(final_dst) - 1] = '\0';
    }
    
    free(src_copy);
    free(renamed_base);

    struct stat src_st;
    if (stat(src, &src_st) != 0) {
        report_action(src, final_dst, "stat", "failed", cfg);
        return;
    }

    if (stat(final_dst, &dst_st) == 0) {
        if (cfg->update_only && src_st.st_mtime <= dst_st.st_mtime) {
            report_action(src, final_dst, "skip", "destination newer", cfg);
            return;
        }

        if (cfg->backup) {
            char backup_dst[PATH_MAX];
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            char time_buf[32];
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H%M%S", tm_info);
            
            snprintf(backup_dst, sizeof(backup_dst), "%s.~%s~", final_dst, time_buf);
            
            if (!cfg->dry_run) {
                rename(final_dst, backup_dst);
            }
            report_action(final_dst, backup_dst, "backup", cfg->dry_run ? "dry-run" : "ok", cfg);
        }
    }

    const char *method = "rename";
    if (cfg->git_aware && is_git_tracked(src)) {
        method = "git mv";
        if (!cfg->dry_run) {
            if (execute_git_mv(src, final_dst, cfg)) {
                report_action(src, final_dst, method, "ok", cfg);
                return;
            } else {
                /* If git fails, fallback silently to standard rename */
                method = "rename"; 
            }
        }
    }

    if (cfg->dry_run) {
        report_action(src, final_dst, method, "dry-run", cfg);
        return;
    }

    if (rename(src, final_dst) == 0) {
        report_action(src, final_dst, method, "ok", cfg);
    } else {
        if (errno == EXDEV) {
            method = "copy+rm";
            if (copy_and_unlink(src, final_dst, cfg)) {
                report_action(src, final_dst, method, "ok", cfg);
            } else {
                report_action(src, final_dst, method, "failed", cfg);
            }
        } else {
            report_action(src, final_dst, method, "failed", cfg);
        }
    }
}

/* -------------------------------------------------------------------------
 * Manual Engine
 * ------------------------------------------------------------------------- */
void print_short_help(const char *progname) {
    printf("Usage: %s [-%svdbGuSj%s] [-%sR%s string] [-%sh%s] [-%sm%s] source target\n",
           progname, ANSI_BOLD, ANSI_RESET, ANSI_BOLD, ANSI_RESET, ANSI_BOLD, ANSI_RESET, ANSI_BOLD, ANSI_RESET);
    printf("Hacker's file relocation and bulk-renaming tool.\n");
    printf("Execute '%s -m' for the comprehensive manual page.\n", progname);
}

void print_manpage(bool colorize) {
    const char *b   = colorize ? ANSI_BOLD : "";
    const char *rst = colorize ? ANSI_RESET : "";

    printf("%sHMV(1)%s                   General Commands Manual                  %sHMV(1)%s\n\n", b, rst, b, rst);
    printf("%sNAME%s\n", b, rst);
    printf("     %shmv%s -- hacker's file relocation and bulk-renaming tool\n\n", b, rst);
    printf("%sSYNOPSIS%s\n", b, rst);
    printf("     %shmv%s [-%svdbGuSj%s] [-%sR%s string] source target\n\n", b, rst, b, rst, b, rst);
    printf("%sDESCRIPTION%s\n", b, rst);
    printf("     %shmv%s extends the standard move utility with native Git integration, safe backups,\n", b, rst);
    printf("     JSON output formatting, and on-the-fly substring renaming.\n\n");
    printf("     Options:\n\n");
    printf("     %s-b%s      Backup destination. Appends an ISO-8601 timestamp instead of overwriting.\n", b, rst);
    printf("     %s-d%s      Dry run. Preview operations visually without executing them.\n", b, rst);
    printf("     %s-G%s      Git integration. Executes 'git mv' automatically for tracked files.\n", b, rst);
    printf("     %s-j%s      Emit output as a structured JSON array.\n", b, rst);
    printf("     %s-S str%s  Search string. Identifies a substring in the filename to replace.\n", b, rst);
    printf("     %s-R str%s  Replace string. Used alongside -S to rename files in transit.\n", b, rst);
    printf("     %s-u%s      Update only. Skip if destination exists and is newer than source.\n", b, rst);
    printf("     %s-v%s      Verbose. Print all operations to standard output.\n\n", b, rst);
    printf("HMV Project Suite               September 2026                         HMV(1)\n");
}

/* -------------------------------------------------------------------------
 * Driver Entry Point
 * ------------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    Config cfg = {
        .verbose = false,
        .dry_run = false,
        .git_aware = false,
        .backup = false,
        .update_only = false,
        .json_output = false,
        .colorize = isatty(STDOUT_FILENO),
        .search_str = NULL,
        .replace_str = NULL
    };

    int opt;
    while ((opt = getopt(argc, argv, "vdbGuhjmS:R:")) != -1) {
        switch (opt) {
            case 'v': cfg.verbose = true;     break;
            case 'd': cfg.dry_run = true;     break;
            case 'b': cfg.backup = true;      break;
            case 'G': cfg.git_aware = true;   break;
            case 'u': cfg.update_only = true; break;
            case 'j': cfg.json_output = true; break;
            case 'S': cfg.search_str = optarg;  break;
            case 'R': cfg.replace_str = optarg; break;
            case 'h':
                print_short_help(argv[0]);
                exit(EXIT_SUCCESS);
            case 'm':
                print_manpage(isatty(STDOUT_FILENO));
                exit(EXIT_SUCCESS);
            default:
                fprintf(stderr, "Try '%s -h' for more information.\n", argv[0]);
                exit(EX_USAGE);
        }
    }

    if (optind >= argc - 1) {
        fprintf(stderr, "%shmv: missing file operand%s\n", cfg.colorize ? ANSI_RED : "", cfg.colorize ? ANSI_RESET : "");
        exit(EX_USAGE);
    }

    if (cfg.json_output) printf("[\n");

    const char *target = argv[argc - 1];
    for (int i = optind; i < argc - 1; i++) {
        process_move(argv[i], target, &cfg);
    }

    if (cfg.json_output) printf("\n]\n");

    return EXIT_SUCCESS;
}
