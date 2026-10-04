#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>

/* Helper to convert a string to lowercase for case-insensitive matching */
void str_tolower(char *str) {
    for (; *str; ++str) {
        *str = tolower((unsigned char)*str);
    }
}

/* Helper to strip the newline character from the end of a string */
void strip_newline(char *str) {
    str[strcspn(str, "\n")] = 0;
}

/* Helper to replace the first instance of %AGE% with a calculated integer */
void replace_age(char *str, int age) {
    char *pos = strstr(str, "%AGE%");
    if (pos) {
        char temp[1024];
        int prefix_len = pos - str;
        strncpy(temp, str, prefix_len);
        temp[prefix_len] = '\0';
        sprintf(temp + prefix_len, "%d%s", age, pos + 5);
        strcpy(str, temp);
    }
}

/* Helper to parse -t string in [[[cc]yy]mm]dd format and override target date */
bool set_target_date(const char *date_str, struct tm *target) {
    int len = strlen(date_str);
    int cc = -1, yy = -1, mm = -1, dd = -1;

    for (int i = 0; i < len; i++) {
        if (!isdigit((unsigned char)date_str[i])) return false;
    }

    if (len == 2) sscanf(date_str, "%2d", &dd);
    else if (len == 4) sscanf(date_str, "%2d%2d", &mm, &dd);
    else if (len == 6) sscanf(date_str, "%2d%2d%2d", &yy, &mm, &dd);
    else if (len == 8) sscanf(date_str, "%2d%2d%2d%2d", &cc, &yy, &mm, &dd);
    else return false;

    target->tm_mday = dd;
    if (mm != -1) target->tm_mon = mm - 1;
    if (yy != -1) {
        if (cc != -1) target->tm_year = (cc * 100 + yy) - 1900;
        else {
            if (yy >= 69) target->tm_year = yy;
            else target->tm_year = yy + 100;
        }
    }

    target->tm_isdst = -1; 
    if (mktime(target) == -1) return false;
    return true;
}

/* Core matching logic for a specific single day. Extracts year if available. */
bool matches_today(char *date_str, struct tm *target_day, int *out_year) {
    char lower_date[64];
    strncpy(lower_date, date_str, sizeof(lower_date) - 1);
    lower_date[sizeof(lower_date) - 1] = '\0';
    
    // Trim trailing whitespace manually for the rule check
    char *end = lower_date + strlen(lower_date) - 1;
    while (end > lower_date && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    str_tolower(lower_date);

    if (strcmp(lower_date, "* *") == 0) return true;
    if (strcmp(lower_date, "m-f") == 0) return (target_day->tm_wday >= 1 && target_day->tm_wday <= 5);
    if (strcmp(lower_date, "sasun") == 0) return (target_day->tm_wday == 0 || target_day->tm_wday == 6);

    int mon, day, year;
    char str_buf[20];
    const char *months[] = {"jan", "feb", "mar", "apr", "may", "jun",
                            "jul", "aug", "sep", "oct", "nov", "dec"};
    const char *days[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};

    // YYYY-MM-DD or YYYY/MM/DD (Used for %AGE% macro)
    if (sscanf(lower_date, "%d-%d-%d", &year, &mon, &day) == 3 || 
        sscanf(lower_date, "%d/%d/%d", &year, &mon, &day) == 3) {
        if (mon - 1 == target_day->tm_mon && day == target_day->tm_mday) {
            if (out_year) *out_year = year;
            return true;
        }
    }

    if (sscanf(lower_date, "* %d", &day) == 1) return (day == target_day->tm_mday);
    
    if (sscanf(lower_date, "%d/%d", &mon, &day) == 2) {
        return (mon - 1 == target_day->tm_mon && day == target_day->tm_mday);
    }

    if (sscanf(lower_date, "%d %19s", &day, str_buf) == 2) {
        for (int i = 0; i < 12; i++) {
            if (strncmp(str_buf, months[i], 3) == 0) return (i == target_day->tm_mon && day == target_day->tm_mday);
        }
    }

    if (sscanf(lower_date, "%19s %d", str_buf, &day) == 2) {
        for (int i = 0; i < 12; i++) {
            if (strncmp(str_buf, months[i], 3) == 0) return (i == target_day->tm_mon && day == target_day->tm_mday);
        }
    }

    if (sscanf(lower_date, "%19s", str_buf) == 1) {
        for (int i = 0; i < 7; i++) {
            if (strncmp(str_buf, days[i], 3) == 0) return (i == target_day->tm_wday);
        }
        for (int i = 0; i < 12; i++) {
            if (strncmp(str_buf, months[i], 3) == 0) return (i == target_day->tm_mon && target_day->tm_mday == 1);
        }
    }

    return false;
}

/* Loop through the window. Returns the integer offset of days (e.g. 1 for tomorrow). */
int find_matching_offset(char *date_str, struct tm *base_target, int lookbehind_days, int lookahead_days, int *out_year) {
    for (int i = -lookbehind_days; i <= lookahead_days; i++) {
        struct tm day_to_check = *base_target;
        day_to_check.tm_mday += i;
        day_to_check.tm_isdst = -1;
        
        if (mktime(&day_to_check) != -1) {
            int matched_year = -1;
            if (matches_today(date_str, &day_to_check, &matched_year)) {
                if (out_year) *out_year = matched_year;
                return i;
            }
        }
    }
    return INT_MAX;
}

int main(int argc, char *argv[]) {
    bool print_rule = false;
    bool print_wday = false;
    int lookahead_days = 0;
    int lookbehind_days = 0;
    const char *filename = "calendar";
    const char *target_date_str = NULL;
    const char *filter_tag = NULL;
    
    const char *day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            print_rule = true;
        } else if (strcmp(argv[i], "-w") == 0) {
            print_wday = true;
        } else if (strcmp(argv[i], "-t") == 0) {
            if (i + 1 < argc) target_date_str = argv[++i];
            else return 1;
        } else if (strcmp(argv[i], "-A") == 0) {
            if (i + 1 < argc) lookahead_days = atoi(argv[++i]);
            else return 1;
        } else if (strcmp(argv[i], "-B") == 0) {
            if (i + 1 < argc) lookbehind_days = atoi(argv[++i]);
            else return 1;
        } else if (strcmp(argv[i], "-g") == 0) {
            if (i + 1 < argc) filter_tag = argv[++i];
            else return 1;
        } else {
            filename = argv[i];
        }
    }

    FILE *file = fopen(filename, "r");
    if (!file) return 1;

    time_t t = time(NULL);
    struct tm *target = localtime(&t);

    if (target_date_str) {
        if (!set_target_date(target_date_str, target)) {
            fclose(file);
            return 1;
        }
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (buffer[0] == '\n' || buffer[0] == '#') continue;

        char *tab_pos = strchr(buffer, '\t');
        if (tab_pos) {
            *tab_pos = '\0';
            strip_newline(tab_pos + 1);
            
            // Apply the -g grep filter to the event description
            if (filter_tag && strstr(tab_pos + 1, filter_tag) == NULL) {
                *tab_pos = '\t';
                continue;
            }

            int event_year = -1;
            int offset = find_matching_offset(buffer, target, lookbehind_days, lookahead_days, &event_year);
            
            if (offset != INT_MAX) {
                // Calculate and replace %AGE% macro
                if (event_year != -1) {
                    int age = (target->tm_year + 1900) - event_year;
                    replace_age(tab_pos + 1, age);
                }

                // Determine weekday
                int matched_wday = (target->tm_wday + offset) % 7;
                if (matched_wday < 0) matched_wday += 7;

                // Format the relative countdown tag
                char countdown[64] = "";
                if (offset == 1) snprintf(countdown, sizeof(countdown), " [Tomorrow]");
                else if (offset == -1) snprintf(countdown, sizeof(countdown), " [Yesterday]");
                else if (offset > 1) snprintf(countdown, sizeof(countdown), " [In %d days]", offset);
                else if (offset < -1) snprintf(countdown, sizeof(countdown), " [%d days ago]", -offset);

                // Print Output
                if (print_wday) printf("%s ", day_names[matched_wday]);
                if (print_rule) {
                    *tab_pos = '\t';
                    printf("%s%s\n", buffer, countdown);
                } else {
                    printf("%s%s\n", tab_pos + 1, countdown);
                }
            } else {
                *tab_pos = '\t';
            }
        }
    }

    fclose(file);
    return 0;
}
