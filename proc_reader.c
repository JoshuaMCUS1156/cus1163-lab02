#include "proc_reader.h"

// Return 1 if str is all digits, else 0
int is_number(const char* str) {
    if (str == NULL || *str == '\0') return 0;
    for (const unsigned char* p = (const unsigned char*)str; *p; ++p) {
        if (!isdigit(*p)) return 0;
    }
    return 1;
}

// Read a whole file using open/read/close and print to stdout
int read_file_with_syscalls(const char* filename) {
    if (!filename) return -1;

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    char buf[4096];
    ssize_t n;

    int treat_nuls_as_spaces = 0;
    // /proc/<pid>/cmdline is NUL-separated; show spaces instead
    if (strstr(filename, "/cmdline") != NULL) {
        treat_nuls_as_spaces = 1;
    }

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (treat_nuls_as_spaces) {
            for (ssize_t i = 0; i < n; ++i) {
                if (buf[i] == '\0') buf[i] = ' ';
            }
        }
        if (write(STDOUT_FILENO, buf, n) < 0) {
            perror("write");
            close(fd);
            return -1;
        }
    }
    if (n < 0) {
        perror("read");
        close(fd);
        return -1;
    }

    if (treat_nuls_as_spaces) {
        // ensure a newline at the end for cmdline readability
        if (write(STDOUT_FILENO, "\n", 1) < 0) {
            perror("write");
            close(fd);
            return -1;
        }
    }

    if (close(fd) == -1) {
        perror("close");
        return -1;
    }
    return 0;
}

// Read a whole file using stdio (fopen/fgets) and print to stdout
int read_file_with_library(const char* filename) {
    if (!filename) return -1;

    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        fputs(line, stdout);
    }

    if (ferror(fp)) {
        perror("fgets");
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        perror("fclose");
        return -1;
    }
    return 0;
}

// Option 1: list numeric directories under /proc
int list_process_directories(void) {
    DIR* dir = opendir("/proc");
    if (!dir) {
        perror("opendir");
        return -1;
    }

    struct dirent* entry;
    int count = 0;

    printf("Process directories in /proc:\n");
    printf("%-8s %-20s\n", "PID", "Type");
    printf("%-8s %-20s\n", "---", "----");

    // Some filesystems may not set d_type; rely on name check primarily
    while ((entry = readdir(dir)) != NULL) {
        if (is_number(entry->d_name)) {
            printf("%-8s %-20s\n", entry->d_name, "process");
            count++;
        }
    }

    if (closedir(dir) == -1) {
        perror("closedir");
        return -1;
    }

    printf("Found %d process directories\n", count);
    return 0;
}

// Option 2: print /proc/[pid]/status and /proc/[pid]/cmdline using syscalls
int read_process_info(const char* pid) {
    if (!pid || !is_number(pid)) {
        fprintf(stderr, "Invalid PID\n");
        return -1;
    }

    char path[256];

    // /proc/[pid]/status
    snprintf(path, sizeof(path), "/proc/%s/status", pid);
    printf("\n--- Process Information for PID %s ---\n", pid);
    if (read_file_with_syscalls(path) != 0) {
        fprintf(stderr, "Failed to read %s\n", path);
        return -1;
    }

    // /proc/[pid]/cmdline
    snprintf(path, sizeof(path), "/proc/%s/cmdline", pid);
    printf("\n--- Command Line ---\n");
    if (read_file_with_syscalls(path) != 0) {
        fprintf(stderr, "Failed to read %s\n", path);
        return -1;
    }

    printf("\n");
    return 0;
}

// Option 3: show first 10 lines of cpuinfo and meminfo using stdio
int show_system_info(void) {
    const int MAX_LINES = 10;
    int line_count = 0;

    printf("\n--- CPU Information (first %d lines) ---\n", MAX_LINES);
    FILE* fcpu = fopen("/proc/cpuinfo", "r");
    if (!fcpu) {
        perror("fopen /proc/cpuinfo");
    } else {
        char line[1024];
        line_count = 0;
        while (line_count < MAX_LINES && fgets(line, sizeof(line), fcpu)) {
            fputs(line, stdout);
            line_count++;
        }
        if (fclose(fcpu) != 0) perror("fclose /proc/cpuinfo");
    }

    printf("\n--- Memory Information (first %d lines) ---\n", MAX_LINES);
    FILE* fmem = fopen("/proc/meminfo", "r");
    if (!fmem) {
        perror("fopen /proc/meminfo");
    } else {
        char line[1024];
        line_count = 0;
        while (line_count < MAX_LINES && fgets(line, sizeof(line), fmem)) {
            fputs(line, stdout);
            line_count++;
        }
        if (fclose(fmem) != 0) perror("fclose /proc/meminfo");
    }

    return 0;
}

// Option 4: compare direct syscalls vs stdio on /proc/version
void compare_file_methods(void) {
    const char* test_file = "/proc/version";

    printf("Comparing file reading methods for: %s\n\n", test_file);

    printf("=== Method 1: Using System Calls ===\n");
    read_file_with_syscalls(test_file);

    printf("\n=== Method 2: Using Library Functions ===\n");
    read_file_with_library(test_file);

    printf("\nNOTE: Run this program with strace to see the difference!\n");
    printf("Example: strace -e trace=openat,read,write,close ./lab2\n");
}
