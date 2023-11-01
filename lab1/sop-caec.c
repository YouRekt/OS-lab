#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ctype.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define PATH_BUF_LEN 256
#define TEXT_BUF_LEN 256
#define MAXFD 20

ssize_t bulk_read(int fd, char *buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  // EOF
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

void make_file(const char *path)
{
    FILE *file;

    if ((file = fopen(path, "w")) == NULL)
        ERR("fopen");

    char text[TEXT_BUF_LEN];

    for (;;)
    {
        fgets(text, TEXT_BUF_LEN, stdin);

        if (!strcmp(text, "\n"))
            break;

        for (size_t i = 0; i < strlen(text); i++)
            text[i] = toupper(text[i]);

        fputs(text, file);
    }

    if (fclose(file))
        ERR("fclose");

    return;
}

void write_stage2(const char *const path, const struct stat *const stat_buf)
{
    if (!S_ISREG(stat_buf->st_mode))
    {
        printf("\nThe path specifies a non regular file\n\n");
        return;
    }
    if (unlink(path))
        ERR("unlink");
    make_file(path);
    return;
}

void print_dir(const char *path)
{
    DIR *dirp;
    struct dirent *dp;
    struct stat file_stat;

    if (NULL == (dirp = opendir(path)))
        ERR("opendir");

    do
    {
        errno = 0;
        if (NULL != (dp = readdir(dirp)))
        {
            if (stat(dp->d_name, &file_stat))
                ERR("stat");
            if (S_ISREG(file_stat.st_mode))
                printf("%s\n", dp->d_name);
        }
    } while (NULL != dp);

    if (errno != 0)
        ERR("readdir");

    if (closedir(dirp))
        ERR("closedir");
}

void print_file_info(const char *path, const struct stat *const stat_buf)
{
    const int file = open(path, O_RDONLY);

    if (-1 == file)
        ERR("open");

    char content[TEXT_BUF_LEN];

    printf("Content:\n");

    for (;;)
    {
        const ssize_t read_size = bulk_read(file, content, TEXT_BUF_LEN);

        if (read_size == -1)
            ERR("bulk_read");

        if (read_size == 0)
            break;

        printf("%s\n", content);
    }
    if (close(file))
        ERR("close");

    printf("Size: %ld bytes\n", stat_buf->st_size);
    printf("UID: %u\n", stat_buf->st_uid);
    printf("GID: %u\n", stat_buf->st_gid);
}

void show_stage3(const char *const path, const struct stat *const stat_buf)
{
    if (S_ISREG(stat_buf->st_mode))
    {
        print_file_info(path, stat_buf);
        return;
    }

    else if (S_ISDIR(stat_buf->st_mode))
    {
        print_dir(path);
        return;
    }

    else
    {
        printf("\nInvalid file type\n\n");
        return;
    }
}

int print_dir_tree(const char *path, const struct stat *stat_buf, int type, struct FTW *f)
{
    switch (type)
    {
        case FTW_DNR:
        case FTW_D:
            for (int i = 0; i < f->level; i++)
                printf(" ");

            printf("+");

            for (size_t i = f->base; i < strlen(path); i++)
                printf("%c", path[i]);

            printf("\n");
            break;
        case FTW_F:
            for (int i = 0; i < f->level; i++)
                printf(" ");
            for (size_t i = f->base; i < strlen(path); i++)
                printf("%c", path[i]);
            printf("\n");
            break;
        default:
            break;
    }
    return EXIT_SUCCESS;
}

void walk_stage4(const char *const path, const struct stat *const stat_buf)
{
    if (nftw(path, print_dir_tree, MAXFD, FTW_PHYS))
        ERR("nftw");
}

void debug_string(char *path)
{
    for (int i = 0; i < PATH_BUF_LEN; i++)
    {
        printf("%d:\t", i);
        if (path[i] == '\0')
            printf("\\0\n");
        else if (path[i] == '\n')
            printf("\\n\n");
        else
            printf("%c\n", path[i]);
    }
}

void path_str(char *str, int MAX_LENGTH)
{
    fgets(str, MAX_LENGTH, stdin);
    str[strlen(str) - 1] = '\0';
}

int get_path(char *path, struct stat *filestat)
{
    path_str(path, PATH_BUF_LEN);

    if (stat(path, filestat))
    {
        if (errno != ENOENT)
            ERR("stat");

        printf("\nFile or path doesn't exist\n\n");

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int interface_stage1()
{
    printf("A. write\n");
    printf("B. show\n");
    printf("C. walk\n");
    printf("D. exit\n");

    //fflush(stdin);

    char option;
    option = getchar();
    getchar();
    char path[PATH_BUF_LEN];
    struct stat filestat;
    switch (option)
    {
        case 'a':
        case 'A':
            if (get_path(path, &filestat))
                return EXIT_FAILURE;
            write_stage2(path, &filestat);
            return EXIT_SUCCESS;
        case 'b':
        case 'B':
            if (get_path(path, &filestat))
                return EXIT_FAILURE;
            show_stage3(path, &filestat);
            return EXIT_SUCCESS;
        case 'c':
        case 'C':
            if (get_path(path, &filestat))
                return EXIT_FAILURE;
            walk_stage4(path, &filestat);
            return EXIT_SUCCESS;
        case 'd':
        case 'D':
            return EXIT_SUCCESS;
        default:
            printf("\nInvalid command\n\n");
            return EXIT_FAILURE;
    }
    return EXIT_FAILURE;
}

int main()
{
    while (interface_stage1())
        ;
    return EXIT_SUCCESS;
}