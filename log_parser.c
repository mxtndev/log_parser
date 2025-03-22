#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_LINE_LENGTH 4096
#define MAX_URLS 10
#define MAX_REFERRERS 10

typedef struct {
    char url[256];
    long bytes;
} UrlStat;

typedef struct {
    char referrer[256];
    int count;
} ReferrerStat;

typedef struct {
    pthread_mutex_t *mutex;
    long total_bytes;
    UrlStat top_urls[MAX_URLS];
    ReferrerStat top_referrers[MAX_REFERRERS];
} GlobalStats;

typedef struct {
    char **files;
    int file_count;
    int start_index;
    int end_index;
    GlobalStats *stats;
} ThreadData;

void parse_log_line(const char *line, GlobalStats *stats) {
    char ip[64], user[64], timestamp[64], request[1024], referrer[1024], agent[1024];
    int status_code, bytes_sent;
    if (sscanf(line, "%63[^ ] %*s %63[^ ] [%63[^]]] \"%1023[^\"]\" %d %d \"%1023[^\"]\" \"%*[^\"]\"",
               ip, user, timestamp, request, &status_code, &bytes_sent, referrer) == 7) {
        // Обновляем общее количество байт
        pthread_mutex_lock(stats->mutex);
        stats->total_bytes += bytes_sent;

        // Извлекаем URL из запроса
        char method[16], url[256], protocol[16];
        if (sscanf(request, "%15[^ ] %255[^ ] %15[^ ]", method, url, protocol) == 3) {
            // Обновляем топ URL'ов
            for (int i = 0; i < MAX_URLS; i++) {
                if (strcmp(stats->top_urls[i].url, "") == 0 || strcmp(stats->top_urls[i].url, url) == 0) {
                    strcpy(stats->top_urls[i].url, url);
                    stats->top_urls[i].bytes += bytes_sent;
                    break;
                }
            }
        }

        // Обновляем топ Referer'ов
        if (strcmp(referrer, "-") != 0) {
            for (int i = 0; i < MAX_REFERRERS; i++) {
                if (strcmp(stats->top_referrers[i].referrer, "") == 0 || strcmp(stats->top_referrers[i].referrer, referrer) == 0) {
                    strcpy(stats->top_referrers[i].referrer, referrer);
                    stats->top_referrers[i].count++;
                    break;
                }
            }
        }
        pthread_mutex_unlock(stats->mutex);
    }
}

void *process_logs(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    for (int i = data->start_index; i < data->end_index; i++) {
        FILE *file = fopen(data->files[i], "r");
        if (!file) {
            perror("Error opening file");
            continue;
        }
        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), file)) {
            parse_log_line(line, data->stats);
        }
        fclose(file);
    }
    return NULL;
}

void collect_files(const char *dir_path, char ***files, int *file_count) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    *file_count = 0;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG) {
            (*file_count)++;
        }
    }
    rewinddir(dir);

    *files = malloc(*file_count * sizeof(char *));
    int index = 0;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG) {
            (*files)[index] = malloc(strlen(dir_path) + strlen(entry->d_name) + 2);
            sprintf((*files)[index], "%s/%s", dir_path, entry->d_name);
            index++;
        }
    }
    closedir(dir);
}

void print_stats(GlobalStats *stats) {
    printf("Total bytes: %ld\n", stats->total_bytes);

    printf("Top URLs by traffic:\n");
    for (int i = 0; i < MAX_URLS && stats->top_urls[i].bytes > 0; i++) {
        printf("URL: %s, Bytes: %ld\n", stats->top_urls[i].url, stats->top_urls[i].bytes);
    }

    printf("Top referrers by count:\n");
    for (int i = 0; i < MAX_REFERRERS && stats->top_referrers[i].count > 0; i++) {
        printf("Referrer: %s, Count: %d\n", stats->top_referrers[i].referrer, stats->top_referrers[i].count);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <log_directory> <thread_count>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *log_dir = argv[1];
    int thread_count = atoi(argv[2]);

    char **files;
    int file_count;
    collect_files(log_dir, &files, &file_count);

    if (file_count == 0) {
        fprintf(stderr, "No log files found in the directory.\n");
        return EXIT_FAILURE;
    }

    pthread_t threads[thread_count];
    ThreadData thread_data[thread_count];
    GlobalStats global_stats = {0};
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    global_stats.mutex = &mutex;

    int files_per_thread = file_count / thread_count;
    int remainder = file_count % thread_count;

    for (int i = 0; i < thread_count; i++) {
        thread_data[i].files = files;
        thread_data[i].file_count = file_count;
        thread_data[i].start_index = i * files_per_thread + (i < remainder ? i : remainder);
        thread_data[i].end_index = thread_data[i].start_index + files_per_thread + (i < remainder ? 1 : 0);
        thread_data[i].stats = &global_stats;
        pthread_create(&threads[i], NULL, process_logs, &thread_data[i]);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    print_stats(&global_stats);

    for (int i = 0; i < file_count; i++) {
        free(files[i]);
    }
    free(files);
    pthread_mutex_destroy(&mutex);

    return EXIT_SUCCESS;
}