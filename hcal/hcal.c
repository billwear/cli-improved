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
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <errno.h>

/* -------------------------------------------------------------------------
 * ANSI Terminal Styling
 * ------------------------------------------------------------------------- */
#define ANSI_RESET     "\033[0m"
#define ANSI_BOLD      "\033[1m"
#define ANSI_DIM       "\033[2m"
#define ANSI_UNDERLINE "\033[4m"
#define ANSI_BLUE      "\033[1;34m"
#define ANSI_GREEN     "\033[1;32m"
#define ANSI_CYAN      "\033[1;36m"
#define ANSI_RED       "\033[1;31m"
#define ANSI_MAGENTA   "\033[1;35m"
#define ANSI_YELLOW    "\033[1;33m"
#define ANSI_GRAY      "\033[0;90m"
#define ANSI_CLEAR_LINE "\033[2K\r"

/* -------------------------------------------------------------------------
 * Data Models & Schemas
 * ------------------------------------------------------------------------- */
typedef enum { TYPE_TASK, TYPE_EVENT, TYPE_HABIT, TYPE_NOTE } ItemType;
typedef enum { STATE_TODO, STATE_DONE, STATE_MIGRATED, STATE_CANCELED } ItemState;

typedef struct {
    unsigned int id;
    ItemType type;
    ItemState state;
    time_t created_at;
    time_t scheduled_for;
    time_t completed_at;
    int priority; /* 1 (High) to 4 (Low) */
    char project[64];
    char context[64];
    int streak;
    bool recurring;
    char payload[512];
} HcalItem;

typedef struct {
    bool agenda_view;
    bool gtd_view;
    bool json_output;
    bool colorize;
    char *add_payload;
    char *filter_project;
    char *filter_context;
    unsigned int mark_done_id;
    int pomodoro_mins;
    ItemType force_type;
    int force_priority;
    bool add_recurring;
    bool reset_recurring;
    char db_path[1024];
} Config;

/* -------------------------------------------------------------------------
 * Storage Engine (Flat Text as Data)
 * ------------------------------------------------------------------------- */
void resolve_db_path(char *path_buf, size_t max_len) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    snprintf(path_buf, max_len, "%s/.hcal_data", home);
}

bool parse_line(char *line, HcalItem *item) {
    char *str = line;
    char *token;

    token = strsep(&str, "|");
    if (!token) return false;
    item->id = strtoul(token, NULL, 10);

    token = strsep(&str, "|");
    if (!token || !token[0]) return false;
    item->type = (token[0] == 'T') ? TYPE_TASK : (token[0] == 'E') ? TYPE_EVENT : 
                 (token[0] == 'H') ? TYPE_HABIT : TYPE_NOTE;

    token = strsep(&str, "|");
    if (!token || !token[0]) return false;
    item->state = (token[0] == 'D') ? STATE_DONE : (token[0] == 'X') ? STATE_CANCELED : 
                  (token[0] == 'M') ? STATE_MIGRATED : STATE_TODO;

    token = strsep(&str, "|");
    item->created_at = (token && token[0]) ? strtoul(token, NULL, 10) : 0;

    token = strsep(&str, "|");
    item->scheduled_for = (token && token[0]) ? strtoul(token, NULL, 10) : 0;

    token = strsep(&str, "|");
    item->completed_at = (token && token[0]) ? strtoul(token, NULL, 10) : 0;

    token = strsep(&str, "|");
    item->priority = (token && token[0]) ? atoi(token) : 4;

    token = strsep(&str, "|");
    strncpy(item->project, token ? token : "", sizeof(item->project) - 1);

    token = strsep(&str, "|");
    strncpy(item->context, token ? token : "", sizeof(item->context) - 1);

    token = strsep(&str, "|");
    item->streak = (token && token[0]) ? atoi(token) : 0;

    token = strsep(&str, "|");
    item->recurring = (token && token[0] == '1') ? true : false;

    token = strsep(&str, "\n");
    strncpy(item->payload, token ? token : "", sizeof(item->payload) - 1);

    return true;
}

void format_line(const HcalItem *item, char *line, size_t max_len) {
    char type_char = (item->type == TYPE_TASK) ? 'T' : (item->type == TYPE_EVENT) ? 'E' :
                     (item->type == TYPE_HABIT) ? 'H' : 'N';
    char state_char = (item->state == STATE_DONE) ? 'D' : (item->state == STATE_CANCELED) ? 'X' :
                      (item->state == STATE_MIGRATED) ? 'M' : 'O';

    snprintf(line, max_len, "%u|%c|%c|%lu|%lu|%lu|%d|%s|%s|%d|%d|%s\n",
             item->id, type_char, state_char, 
             (unsigned long)item->created_at, (unsigned long)item->scheduled_for, 
             (unsigned long)item->completed_at, item->priority, 
             item->project, item->context, item->streak, 
             item->recurring ? 1 : 0, item->payload);
}

unsigned int get_next_id(const char *db_path) {
    FILE *fp = fopen(db_path, "r");
    if (!fp) return 1;
    
    unsigned int max_id = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *line_copy = strdup(line);
        char *str = line_copy;
        char *token = strsep(&str, "|");
        if (token) {
            unsigned int id = strtoul(token, NULL, 10);
            if (id > max_id) max_id = id;
        }
        free(line_copy);
    }
    fclose(fp);
    return max_id + 1;
}

void append_item(const char *db_path, const HcalItem *item) {
    FILE *fp = fopen(db_path, "a");
    if (!fp) {
        fprintf(stderr, "hcal: cannot open database for writing.\n");
        exit(EX_IOERR);
    }
    char line[1024];
    format_line(item, line, sizeof(line));
    fputs(line, fp);
    fclose(fp);
}

void mutate_item_state(const char *db_path, unsigned int id, ItemState new_state) {
    FILE *fp = fopen(db_path, "r");
    if (!fp) return;

    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", db_path);
    FILE *tmp_fp = fopen(tmp_path, "w");
    if (!tmp_fp) {
        fclose(fp);
        return;
    }

    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        char line_copy[1024];
        strcpy(line_copy, line);
        HcalItem item;
        memset(&item, 0, sizeof(HcalItem));
        
        if (parse_line(line_copy, &item) && item.id == id) {
            item.state = new_state;
            item.completed_at = (new_state == STATE_DONE) ? time(NULL) : 0;
            if (item.type == TYPE_HABIT && new_state == STATE_DONE) item.streak++;
            format_line(&item, line, sizeof(line));
            found = true;
        }
        fputs(line, tmp_fp);
    }
    fclose(fp);
    fclose(tmp_fp);
    
    if (found) {
        rename(tmp_path, db_path);
    } else {
        unlink(tmp_path);
        fprintf(stderr, "hcal: item %u not found.\n", id);
    }
}

void reset_recurring_items(const char *db_path, const Config *cfg) {
    FILE *fp = fopen(db_path, "r");
    if (!fp) return;

    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", db_path);
    FILE *tmp_fp = fopen(tmp_path, "w");
    if (!tmp_fp) {
        fclose(fp);
        return;
    }

    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        char line_copy[1024];
        strcpy(line_copy, line);
        HcalItem item;
        memset(&item, 0, sizeof(HcalItem));
        
        if (parse_line(line_copy, &item)) {
            if (item.recurring && item.state == STATE_DONE) {
                item.state = STATE_TODO;
                item.completed_at = 0;
                count++;
            }
            format_line(&item, line, sizeof(line));
        }
        fputs(line, tmp_fp);
    }
    fclose(fp);
    fclose(tmp_fp);
    rename(tmp_path, db_path);
    
    if (cfg->colorize) {
        printf("%s↻ Respawned %d recurring items.%s\n", ANSI_GREEN, count, ANSI_RESET);
    } else {
        printf("Respawned %d recurring items.\n", count);
    }
}

/* -------------------------------------------------------------------------
 * Presentation Engine
 * ------------------------------------------------------------------------- */
void print_json(const char *db_path, const Config *cfg) {
    FILE *fp = fopen(db_path, "r");
    if (!fp) {
        printf("[]\n");
        return;
    }

    printf("[\n");
    char line[1024];
    bool first = true;
    while (fgets(line, sizeof(line), fp)) {
        HcalItem item;
        memset(&item, 0, sizeof(HcalItem));
        if (parse_line(line, &item)) {
            if (cfg->filter_project && strcmp(item.project, cfg->filter_project) != 0) continue;
            if (cfg->filter_context && strcmp(item.context, cfg->filter_context) != 0) continue;

            if (!first) printf(",\n");
            first = false;
            printf("  {\n");
            printf("    \"id\": %u,\n", item.id);
            printf("    \"type\": %d,\n", item.type);
            printf("    \"state\": %d,\n", item.state);
            printf("    \"priority\": %d,\n", item.priority);
            printf("    \"project\": \"%s\",\n", item.project);
            printf("    \"context\": \"%s\",\n", item.context);
            printf("    \"streak\": %d,\n", item.streak);
            printf("    \"recurring\": %s,\n", item.recurring ? "true" : "false");
            printf("    \"payload\": \"%s\"\n", item.payload);
            printf("  }");
        }
    }
    printf("\n]\n");
    fclose(fp);
}

void print_agenda(const char *db_path, const Config *cfg) {
    FILE *fp = fopen(db_path, "r");
    if (!fp) {
        printf("No data found. Add items using -A.\n");
        return;
    }

    const char *rst = cfg->colorize ? ANSI_RESET : "";
    const char *bld = cfg->colorize ? ANSI_BOLD : "";
    const char *red = cfg->colorize ? ANSI_RED : "";
    const char *grn = cfg->colorize ? ANSI_GREEN : "";
    const char *blu = cfg->colorize ? ANSI_BLUE : "";
    const char *cyn = cfg->colorize ? ANSI_CYAN : "";
    const char *gry = cfg->colorize ? ANSI_GRAY : "";
    const char *mag = cfg->colorize ? ANSI_MAGENTA : "";

    printf("\n%s❖ HCAL AGENDA VIEW%s\n", bld, rst);
    printf("%s-------------------------------------------------------%s\n", gry, rst);

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        HcalItem item;
        memset(&item, 0, sizeof(HcalItem));
        if (parse_line(line, &item)) {
            if (item.state != STATE_TODO && item.state != STATE_MIGRATED) continue;
            if (cfg->filter_project && strcmp(item.project, cfg->filter_project) != 0) continue;
            if (cfg->filter_context && strcmp(item.context, cfg->filter_context) != 0) continue;

            char prio_char = item.priority == 1 ? '!' : (item.priority == 2 ? '*' : (item.priority == 3 ? '-' : '.'));
            const char *type_col = item.type == TYPE_TASK ? blu : (item.type == TYPE_HABIT ? grn : (item.type == TYPE_EVENT ? red : cyn));
            char type_glyph = item.type == TYPE_TASK ? 'T' : (item.type == TYPE_HABIT ? 'H' : (item.type == TYPE_EVENT ? 'E' : 'N'));

            printf("%s%04u%s | %s%c%s | %s%c%s | %s%-12s%s | %s@%-10s%s | %s%s%s%s\n",
                   gry, item.id, rst,
                   item.priority == 1 ? red : gry, prio_char, rst,
                   type_col, type_glyph, rst,
                   blu, item.project[0] ? item.project : "inbox", rst,
                   cyn, item.context[0] ? item.context : "any", rst,
                   item.payload,
                   item.recurring ? mag : "",
                   item.recurring ? " [∞]" : "",
                   item.recurring ? rst : "");
        }
    }
    printf("%s-------------------------------------------------------%s\n\n", gry, rst);
    fclose(fp);
}

/* -------------------------------------------------------------------------
 * Timeblocking & Focus Engine
 * ------------------------------------------------------------------------- */
void run_pomodoro(int minutes) {
    printf("%s❖ FOCUS SESSION INITIATED: %d MINUTES%s\n", ANSI_BOLD, minutes, ANSI_RESET);
    int total_seconds = minutes * 60;
    
    while (total_seconds > 0) {
        int m = total_seconds / 60;
        int s = total_seconds % 60;
        
        printf(ANSI_CLEAR_LINE "\r⏳ %s%02d:%02d%s remaining...", 
               total_seconds < 300 ? ANSI_RED : ANSI_CYAN, m, s, ANSI_RESET);
        fflush(stdout);
        
        sleep(1);
        total_seconds--;
    }
    
    printf(ANSI_CLEAR_LINE "\r✅ %sFOCUS SESSION COMPLETE!%s Take a break.\n\a", ANSI_GREEN, ANSI_RESET);
}

/* -------------------------------------------------------------------------
 * Driver Entry Point
 * ------------------------------------------------------------------------- */
void print_help() {
    printf("Usage: hcal [-aGjR] [-A \"payload\"] [-p project] [-c context] [-x id] [-P mins] [-t|-e|-h] [-r] [-1..-4]\n");
    printf("Hacker's absolute scheduling and productivity engine.\n\n");
    printf("  -a         Show Agenda view (Default)\n");
    printf("  -A <text>  Add a new item\n");
    printf("  -x <id>    Mark item ID as complete\n");
    printf("  -P <mins>  Start Pomodoro focus timer\n");
    printf("  -p <proj>  Set/Filter Project\n");
    printf("  -c <ctx>   Set/Filter Context (e.g. '@computer')\n");
    printf("  -t,-e,-h   Set type: Task (default), Event, Habit\n");
    printf("  -r         Flag item as recurring (respawns on reset)\n");
    printf("  -R         Morning Reset: respawn all done recurring items\n");
    printf("  -1..-4     Set priority (1=Highest, 4=Lowest/Someday)\n");
    printf("  -j         JSON structured output\n");
}

int main(int argc, char *argv[]) {
    Config cfg = {
        .agenda_view = true,
        .gtd_view = false,
        .json_output = false,
        .colorize = isatty(STDOUT_FILENO),
        .add_payload = NULL,
        .filter_project = NULL,
        .filter_context = NULL,
        .mark_done_id = 0,
        .pomodoro_mins = 0,
        .force_type = TYPE_TASK,
        .force_priority = 4,
        .add_recurring = false,
        .reset_recurring = false
    };

    resolve_db_path(cfg.db_path, sizeof(cfg.db_path));

    int opt;
    while ((opt = getopt(argc, argv, "aGjA:x:P:p:c:tehrR1234")) != -1) {
        switch (opt) {
            case 'a': cfg.agenda_view = true; break;
            case 'j': cfg.json_output = true; cfg.agenda_view = false; break;
            case 'G': cfg.gtd_view = true; cfg.agenda_view = false; break;
            case 'A': cfg.add_payload = optarg; break;
            case 'x': cfg.mark_done_id = atoi(optarg); break;
            case 'P': cfg.pomodoro_mins = atoi(optarg); break;
            case 'p': cfg.filter_project = optarg; break;
            case 'c': cfg.filter_context = optarg; break;
            case 't': cfg.force_type = TYPE_TASK; break;
            case 'e': cfg.force_type = TYPE_EVENT; break;
            case 'h': cfg.force_type = TYPE_HABIT; break;
            case 'r': cfg.add_recurring = true; break;
            case 'R': cfg.reset_recurring = true; break;
            case '1': cfg.force_priority = 1; break;
            case '2': cfg.force_priority = 2; break;
            case '3': cfg.force_priority = 3; break;
            case '4': cfg.force_priority = 4; break;
            default:
                print_help();
                exit(EX_USAGE);
        }
    }

    if (cfg.reset_recurring) {
        reset_recurring_items(cfg.db_path, &cfg);
        return EXIT_SUCCESS;
    }

    if (cfg.pomodoro_mins > 0) {
        run_pomodoro(cfg.pomodoro_mins);
        return EXIT_SUCCESS;
    }

    if (cfg.mark_done_id > 0) {
        mutate_item_state(cfg.db_path, cfg.mark_done_id, STATE_DONE);
        if (cfg.colorize) printf("%sItem %u marked done.%s\n", ANSI_GREEN, cfg.mark_done_id, ANSI_RESET);
        return EXIT_SUCCESS;
    }

    if (cfg.add_payload) {
        HcalItem item;
        memset(&item, 0, sizeof(HcalItem));
        item.id = get_next_id(cfg.db_path);
        item.type = cfg.force_type;
        item.state = STATE_TODO;
        item.created_at = time(NULL);
        item.priority = cfg.force_priority;
        item.recurring = cfg.add_recurring;
        if (cfg.filter_project) strncpy(item.project, cfg.filter_project, sizeof(item.project) - 1);
        if (cfg.filter_context) strncpy(item.context, cfg.filter_context, sizeof(item.context) - 1);
        strncpy(item.payload, cfg.add_payload, sizeof(item.payload) - 1);
        
        append_item(cfg.db_path, &item);
        if (cfg.colorize) printf("%s+ Added [%c] ID %u:%s %s\n", ANSI_CYAN, 
                                 item.type == TYPE_TASK ? 'T' : item.type == TYPE_HABIT ? 'H' : 'E', 
                                 item.id, ANSI_RESET, item.payload);
        return EXIT_SUCCESS;
    }

    if (cfg.json_output) {
        print_json(cfg.db_path, &cfg);
    } else {
        print_agenda(cfg.db_path, &cfg);
    }

    return EXIT_SUCCESS;
}
