#include "port/config.h"
#include "port/host.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "vita_config_test_io.h"

static char directory[1024];
static char current_config[8192];
static const char original[] =
    "# user-owned settings\nuser_marker=primary\nvita_test_match=1\n"
    "# --- STRIKERS VALIDATED DEFAULTS BEGIN ---\n"
    "# profile_revision = stale\ntask_profile=1\n"
    "# --- STRIKERS VALIDATED DEFAULTS END ---\n"
    "tail_marker=after\n";
static const char backup_original[] = "user_marker=backup\ntail_marker=backup-tail\n";

int port_executable_dir(char* out, size_t size)
{
    if (strlen(directory) >= size)
        return -1;
    strcpy(out, directory);
    return 0;
}
int port_setenv_default(const char* key, const char* value)
{
    return getenv(key) ? 1 : setenv(key, value, 1);
}

static void write_text(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    assert(f && fputs(text, f) != EOF);
    assert(!fclose(f));
}

static void read_text(const char* path, char* text, size_t capacity)
{
    FILE* f = fopen(path, "rb");
    assert(f);
    const size_t size = fread(text, 1, capacity - 1, f);
    assert(!ferror(f) && feof(f));
    assert(!fclose(f));
    text[size] = 0;
}

static void equal_text(const char* path, const char* expected)
{
    char text[16384];
    read_text(path, text, sizeof text);
    assert(!strcmp(text, expected));
}

static int present(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static void clean_file_or_directory(const char* path)
{
    if (unlink(path) && errno != ENOENT)
        assert(!rmdir(path));
}

static void reset_environment(void)
{
    unsetenv("STRIKERS_CONFIG");
    unsetenv("STRIKERS_USER_MARKER");
    unsetenv("STRIKERS_TAIL_MARKER");
    unsetenv("STRIKERS_VITA_TEST_MATCH");
    setenv("STRIKERS_DIAGNOSTICS", "0", 1);
    setenv("STRIKERS_FPS_OVERLAY", "0", 1);
}

static void wait_ok(pid_t child)
{
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void run_case(unsigned mode)
{
    char path[1200], backup[1300], before[16384];
    snprintf(path, sizeof path, "%s/strikers.ini", directory);
    snprintf(backup, sizeof backup, "%s.vita-defaults.bak", path);
    reset_environment();
    int expected_success = mode == 13 || mode == 14 || mode == 15 || mode == 20 || mode == 25;
    const int missing = (mode >= 14 && mode <= 18) || mode == 24 || mode == 27;
    if (mode == 19)
        assert(!mkdir(path, 0700));
    else if (!missing) {
        if (mode == 25) {
            snprintf(before, sizeof before, "user_marker=primary\n%s", current_config);
            write_text(path, before);
        } else {
            strcpy(before, original);
            write_text(path, original);
        }
    }
    if ((mode >= 14 && mode <= 18) || mode == 20 || mode == 21 || mode == 27 || mode == 28) {
        if (mode == 18)
            assert(!mkdir(backup, 0700));
        else
            write_text(backup, backup_original);
    }

    switch (mode) {
    case 0: config_add_fault(CT_OPEN, CT_PRIMARY, 1, 0); break;
    case 1: config_add_fault(CT_READ, CT_PRIMARY, 1, 1); break;
    case 2: config_add_fault(CT_CLOSE, CT_PRIMARY, 1, 0); break;
    case 3: config_add_fault(CT_OPEN, CT_PRIMARY, 2, 0); break;
    case 4: config_add_fault(CT_READ, CT_PRIMARY, 2, 1); break;
    case 5: config_add_fault(CT_CLOSE, CT_PRIMARY, 2, 0); break;
    case 6: config_add_fault(CT_OPEN, CT_OUTPUT, 0, 0); break;
    case 7: config_add_fault(CT_COPY, CT_OUTPUT, 0, 0); break;
    case 8: config_add_fault(CT_WRITE, CT_OUTPUT, 0, 0); break;
    case 9: config_add_fault(CT_FLUSH, CT_OUTPUT, 0, 0); break;
    case 10: config_add_fault(CT_CLOSE, CT_OUTPUT, 0, 0); break;
    case 11: config_add_fault(CT_STAGE, CT_PRIMARY, 0, 0); break;
    case 12: config_add_fault(CT_PUBLISH, CT_OUTPUT, 0, 0); break;
    case 13:
        config_add_fault(CT_PUBLISH, CT_OUTPUT, 0, 0);
        config_add_fault(CT_RESTORE, CT_BACKUP, 0, 0);
        break;
    case 15: config_add_fault(CT_RESTORE, CT_BACKUP, 0, 0); break;
    case 16: config_add_fault(CT_READ, CT_BACKUP, 0, 0); break;
    case 17: config_add_fault(CT_CLOSE, CT_BACKUP, 0, 0); break;
    case 21: config_add_fault(CT_REMOVE, CT_BACKUP, 0, 0); break;
    case 22: config_add_fault(CT_READ, CT_PRIMARY, 3, 1); break;
    case 23: config_add_fault(CT_CLOSE, CT_PRIMARY, 3, 0); break;
    case 24: config_add_fault(CT_CLOSE, CT_OUTPUT, 0, 0); break;
    case 26:
        setenv("STRIKERS_CONFIG", path, 1);
        config_add_fault(CT_READ, CT_PRIMARY, 1, 0);
        break;
    case 27: config_add_fault(CT_OPEN, CT_BACKUP, 0, 0); break;
    case 28: config_add_fault(CT_READ, CT_BACKUP, 0, 0); break;
    default: break;
    }
    const int result = PortConfigLoad();
    assert(expected_success ? result > 0 : result == -1);
    config_assert_closed();
    for (int i = 0; i < config_fault_count; ++i)
        assert(config_faults[i].used);

    if (mode <= 12 || mode == 21 || mode == 26 || mode == 28) {
        equal_text(path, original);
    } else if (mode == 13) {
        assert(!present(path));
        equal_text(backup, original);
        assert(!strcmp(PortConfigPath(), backup));
        assert(!strcmp(getenv("STRIKERS_USER_MARKER"), "primary"));
        assert(!strcmp(getenv("STRIKERS_TAIL_MARKER"), "after"));
    } else if (mode == 14) {
        char refreshed[8192];
        read_text(path, refreshed, sizeof refreshed);
        assert(strstr(refreshed, backup_original));
        assert(!strcmp(getenv("STRIKERS_USER_MARKER"), "backup"));
        assert(!strcmp(PortConfigPath(), path));
    } else if (mode == 15) {
        assert(!present(path));
        equal_text(backup, backup_original);
        assert(!strcmp(PortConfigPath(), backup));
        assert(!strcmp(getenv("STRIKERS_USER_MARKER"), "backup"));
    } else if (mode == 16 || mode == 17 || mode == 27) {
        assert(!present(path));
        equal_text(backup, backup_original);
    } else if (mode == 18) {
        assert(!present(path) && present(backup));
    } else if (mode == 19) {
        assert(present(path) && !present(backup));
    } else if (mode == 20) {
        equal_text(backup, original);
        assert(!strcmp(getenv("STRIKERS_USER_MARKER"), "primary"));
        assert(!strcmp(getenv("STRIKERS_TAIL_MARKER"), "after"));
    } else if (mode == 22 || mode == 23) {
        char refreshed[8192];
        read_text(path, refreshed, sizeof refreshed);
        assert(strstr(refreshed, "user_marker=primary"));
        assert(strstr(refreshed, "tail_marker=after"));
        equal_text(backup, original);
    } else if (mode == 24) {
        assert(!present(path) && !present(backup));
    } else if (mode == 25) {
        equal_text(path, before);
        assert(!present(backup) && config_write_opens == 0);
    }
    if (mode == 21 || mode == 28)
        equal_text(backup, backup_original);
}

int main(void)
{
    char base[] = "/tmp/strikers-vita-config-io-XXXXXX";
    assert(mkdtemp(base));
    snprintf(directory, sizeof directory, "%s/current", base);
    assert(!mkdir(directory, 0700));
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        reset_environment();
        assert(PortConfigLoad() > 0);
        config_assert_closed();
        exit(0);
    }
    wait_ok(child);
    char file[1400];
    snprintf(file, sizeof file, "%s/strikers.ini", directory);
    read_text(file, current_config, sizeof current_config);
    assert(!unlink(file) && !rmdir(directory));

    for (unsigned mode = 0; mode < 29; ++mode) {
        snprintf(directory, sizeof directory, "%s/case-%u", base, mode);
        assert(!mkdir(directory, 0700));
        child = fork();
        assert(child >= 0);
        if (!child) {
            run_case(mode);
            exit(0);
        }
        wait_ok(child);
        const char* suffixes[] = {"", ".tmp", ".vita-defaults.bak"};
        for (size_t i = 0; i < sizeof suffixes / sizeof suffixes[0]; ++i) {
            snprintf(file, sizeof file, "%s/strikers.ini%s", directory, suffixes[i]);
            clean_file_or_directory(file);
        }
        assert(!rmdir(directory));
    }
    assert(!rmdir(base));
    puts("Vita config I/O: 29 migration, failure and recovery cases passed");
}
