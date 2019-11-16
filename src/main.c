#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <wait.h>
#include <sys/stat.h>
#include <stdbool.h>

#define BUFFER_SIZE 100

int parse_string(char buf[], int size, bool* prev) {
    int temp_size = 0;
    char copy[size];
    bool space = *prev;
    for (int i = 0; i < size; ++i) {
        if (buf[i] != ' ' || !space) {
            if (buf[i] == ' ') {
                space = true;
            } else {
                space = false;
            }
            copy[temp_size] = buf[i];
            temp_size++;
        }
    }
    for (int i = 0; i < temp_size; ++i) {
        buf[i] = copy[i];
    }
    *prev = space;
    return temp_size;
}

void throw_error(const char* error) {
    printf("%s\n", error);
    exit(1);
}


int main(int argc, char** argv) {
    if (argc < 2) {
        throw_error("No file");
    }
    if (strlen(argv[1]) > 100) {
        throw_error("Filename is too long");
    }

    //создание временного файла для маппинга
    char* tmp_name = strdup("/tmp/tmp_file.XXXXXX");
    int tmp_fd = mkstemp(tmp_name);
    if (tmp_fd == -1) {
        throw_error("Cannot create temp file to map");
    }
    free(tmp_name);
    int file_size = BUFFER_SIZE + 1;
    char file_filler[file_size];
    for (int i = 0; i < file_size; ++i) {
        file_filler[i] = '\0';
    }
    write(tmp_fd, file_filler, file_size);

    //маппинг файла
    unsigned char* map = (unsigned char*)mmap(NULL, file_size, PROT_WRITE | PROT_READ, MAP_SHARED, tmp_fd, 0);
    if (map == NULL) {
        throw_error("Cant map file");
    }

    //создание семафоров для синхронизации работы
    const char* in_sem_name = "/input_semaphor";
    const char* out_sem_name = "/output_semaphor";
    sem_unlink(in_sem_name);
    sem_unlink(out_sem_name);
    sem_t* in_sem = sem_open(in_sem_name, O_CREAT, 777, 0);
    sem_t* out_sem = sem_open(out_sem_name, O_CREAT, 777, 0);
    if (in_sem == SEM_FAILED || out_sem == SEM_FAILED) {
        throw_error("Cannot create semaphor");
    }

    strcpy(map, argv[1]);
    map[BUFFER_SIZE] = strlen(argv[1]);

    int pid = fork();
    if (pid == -1) {
        throw_error("Fork failure");
    } else if (pid == 0) { //child
        int output_file = open(argv[1], O_RDWR | O_TRUNC | O_CREAT, S_IREAD | S_IWRITE);
        if (output_file == -1) {
            map[BUFFER_SIZE] = 101;
            sem_post(out_sem);
            throw_error("Cannot create output file");
        }
        bool space = false;
        sem_post(out_sem);
        while (true) {
            sem_wait(in_sem);
            int new_size = parse_string(map, map[BUFFER_SIZE], &space);
            write(output_file, map, new_size);
            if (map[BUFFER_SIZE] < BUFFER_SIZE){
                sem_post(out_sem);
                break;
            }
            sem_post(out_sem);
        }
        close(output_file);
        exit(0);

    } else { //parent
        sem_wait(out_sem);
        if (map[BUFFER_SIZE] != 101) {
            int read_count = read(STDIN_FILENO, map, BUFFER_SIZE);
            map[BUFFER_SIZE] = read_count;
            sem_post(in_sem);
            while (read_count == BUFFER_SIZE) {
                sem_wait(out_sem);
                read_count = read(STDIN_FILENO, map, BUFFER_SIZE);
                map[BUFFER_SIZE] = read_count;
                sem_post(in_sem);
            }
            int stat_lock;
            wait(&stat_lock);
            if (stat_lock != 0) {
                printf("%s\n", "Child failure");
            }
        } else {
            int stat_lock;
            wait(&stat_lock);
            if (stat_lock != 0) {
                printf("%s\n", "Child failure");
            }
        }
        sem_close(in_sem);
        sem_close(out_sem);
    }

}