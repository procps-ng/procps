/*
 * cgmeminfo.c - cgroup memory information functions
 *
 * Copyright © 2025 Chunsheng Luo <luffyluo@tencent.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <mntent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <sys/types.h>
#include <linux/limits.h>

#include "include/meminfo.h"


/* Conversion constants */
#define BYTES_TO_KB 1024
#define CGMEMINFO_LEN 8192

/* Cgroup version types */
#define CGROUP_TYPE_UNKNOWN    (0)
#define CGROUP_TYPE_LEGACY    (1 << 0)
#define CGROUP_TYPE_UNIFIED   (1 << 1)
#define CGROUP_TYPE_HYBRID   (CGROUP_TYPE_LEGACY | CGROUP_TYPE_UNIFIED)

struct memory_stat {
    unsigned long total_cache;
    unsigned long total_rss; /* not used now */
    unsigned long total_rss_huge;
    unsigned long total_shmem;
    unsigned long total_mapped_file;
    unsigned long total_dirty;
    unsigned long total_writeback;
    unsigned long total_inactive_anon;
    unsigned long total_active_anon;
    unsigned long total_inactive_file;
    unsigned long total_active_file;
    unsigned long total_unevictable;
    unsigned long slab_reclaimable;
    unsigned long slab_unreclaimable;
    unsigned long slab;
};

struct memcg_data {
    unsigned long memory_limit;     /* v1: memory.limit_in_bytes,       v2: memory.max          */
    unsigned long memory_current;   /* v1: memory.usage_in_bytes,       v2: memory.current      */
    unsigned long swap_limit;       /* v1: memory.memsw.limit_in_bytes, v2: memory.swap.max     */
    unsigned long swap_current;     /* v1: memory.memsw.usage_in_bytes, v2: memory.swap.current */
    struct memory_stat memory_stat; /* v1: memory.stat,                 v2: memory.stat         */
};

struct memcg_meminfo {
    int version;
    int refcount;
    char *cgmem_mount;
    char *cgmem_path;
    struct memcg_data cgmem_data;
};

/**
 * Clean up memcg_meminfo structure resources
 * @info: memcg_meminfo structure to clean up
 */
static void cleanup_memcg_info(struct memcg_meminfo *info) {
    if (!info) return;

    if (info->cgmem_mount) {
        free(info->cgmem_mount);
        info->cgmem_mount = NULL;
    }

    if (info->cgmem_path) {
        free(info->cgmem_path);
        info->cgmem_path = NULL;
    }
}

/**
 * Read all content from a file into buffer
 * @path: file path to read from
 * @buf: buffer to store the content
 * @buf_size: size of the buffer
 *
 * Returns: 0 on success, other on error
 */
static int read_from_file(const char *path, char *buf, size_t buf_size) {
    FILE *fp = NULL;
    size_t bytes_read;
    int ret;

    if (!path || !buf || buf_size == 0) {
        return EINVAL;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return errno;
    }

    memset(buf, 0, buf_size);

    bytes_read = fread(buf, 1, buf_size - 1, fp);
    if (ferror(fp)) {
        ret = errno ? errno : EIO;
        goto cleanup;
    }

    buf[bytes_read] = '\0';
    ret = 0;

cleanup:
    if (fp)
        fclose(fp);

    return ret;
}

static char *cgroup_mount(int version) {
    FILE *fp;
    char *ret = NULL;
    struct mntent *mnt;

    if (!(fp = setmntent("/proc/mounts", "r"))) {
        return NULL;
    }

    while ((mnt = getmntent(fp)) != NULL) {
        if (version == CGROUP_TYPE_UNIFIED
            && strcmp(mnt->mnt_type, "cgroup2") == 0) {
            if (!(ret = strdup(mnt->mnt_dir)))
                break;
            break;
        } else if (version == CGROUP_TYPE_LEGACY
            && strcmp(mnt->mnt_type, "cgroup") == 0
            && strstr(mnt->mnt_opts, "memory") != NULL) {
            if (!(ret = strdup(mnt->mnt_dir)))
                break;
            break;
        }
    }

    endmntent(fp);
    return ret;
}

enum memcg_metric_type {
    MEMCG_MEMORY_LIMIT = 0,
    MEMCG_MEMORY_CURRENT = 1,
    MEMCG_SWAP_LIMIT = 2,
    MEMCG_SWAP_CURRENT = 3,
    MEMCG_MEMORY_STAT = 4
};

struct memcg_file_mapping {
    const char *v1_file;
    const char *v2_file;
};

/* Define file mappings for different memory metrics */
static const struct memcg_file_mapping file_mappings[] = {
    [MEMCG_MEMORY_LIMIT]   = { "memory.limit_in_bytes",       "memory.max"          },
    [MEMCG_MEMORY_CURRENT] = { "memory.usage_in_bytes",       "memory.current"      },
    [MEMCG_SWAP_LIMIT]     = { "memory.memsw.limit_in_bytes", "memory.swap.max"     },
    [MEMCG_SWAP_CURRENT]   = { "memory.memsw.usage_in_bytes", "memory.swap.current" },
    [MEMCG_MEMORY_STAT]    = { "memory.stat",                 "memory.stat"         }
};

static int memcg_parse_memory_stat(char *buf, struct memory_stat *stat, int cgroup_version) {
    /* Initialize all fields to 0 */
    memset(stat, 0, sizeof(struct memory_stat));

#define sTv(f) &stat->f
    /* Define mapping table for memory statistics */
    struct {
        const char *v1_key;
        const char *v2_key;
        unsigned long *field;
    } stat_mappings[] = {
        { "total_cache",           "file",                sTv(total_cache)         },
        { "total_rss_huge",        NULL,                  sTv(total_rss_huge)      }, /* v1 only */
        { "total_shmem",           "shmem",               sTv(total_shmem)         },
        { "total_mapped_file",     "file_mapped",         sTv(total_mapped_file)   },
        { "total_dirty",           "file_dirty",          sTv(total_dirty)         },
        { "total_writeback",       "file_writeback",      sTv(total_writeback)     },
        { "total_inactive_anon",   "inactive_anon",       sTv(total_inactive_anon) },
        { "total_active_anon",     "active_anon",         sTv(total_active_anon)   },
        { "total_inactive_file",   "inactive_file",       sTv(total_inactive_file) },
        { "total_active_file",     "active_file",         sTv(total_active_file)   },
        { "total_unevictable",     "unevictable",         sTv(total_unevictable)   },
        { NULL,                    "slab_reclaimable",    sTv(slab_reclaimable)    }, /* v2 only */
        { NULL,                    "slab_unreclaimable",  sTv(slab_unreclaimable)  }, /* v2 only */
        { NULL,                    "slab",                sTv(slab)                }, /* v2 only */
        { NULL,                    NULL,                  NULL                     }  /* End marker */
    };

    char *line = buf;
    char *next_line;

    while (line && *line) {
        next_line = strchr(line, '\n');
        if (next_line) {
            *next_line = '\0';
            next_line++;
        }

        char stat_key[64];
        unsigned long stat_value;
        if (sscanf(line, "%63s %lu", stat_key, &stat_value) == 2) {
            for (int i = 0; stat_mappings[i].field != NULL; i++) {
                const char *key = NULL;

                if (cgroup_version & CGROUP_TYPE_LEGACY)
                    key = stat_mappings[i].v1_key;
                else
                    key = stat_mappings[i].v2_key;

                if (key && strcmp(stat_key, key) == 0) {
                    *(stat_mappings[i].field) = stat_value;
                    break;
                }
            }
        }

        if (next_line && next_line > line) {
            *(next_line - 1) = '\n';
        }

        line = next_line;
    }

#undef sTv
    return 0;
}

/**
 * Common helper function to build cgroup metric file path
 * @info: memcg_meminfo structure
 * @metric_type: type of metric to read
 * @path_buffer: buffer to store the constructed path
 * @buffer_size: size of the path buffer
 *
 * Returns: 0 on success, other on error
 */
static inline int memcg_build_file_path(struct memcg_meminfo *info,
                                       enum memcg_metric_type metric_type,
                                       char *path_buffer,
                                       size_t buffer_size) {
    if (metric_type >= sizeof(file_mappings)/sizeof(file_mappings[0]))
        return 1;

    const char *filename = (info->version & CGROUP_TYPE_LEGACY)
        ? file_mappings[metric_type].v1_file
        : file_mappings[metric_type].v2_file;

    if (!filename)
        return 1;

    int ret = snprintf(path_buffer, buffer_size, "%s%s/%s",
                      info->cgmem_mount, info->cgmem_path, filename);
    if (ret < 0 || ret >= (int)buffer_size)
        return 1;

    return 0;
}

/**
 * Generic function to read memory metric from cgroup file
 * @info: memcg_meminfo structure
 * @metric_type: type of metric to read
 *
 * Returns: 0 on success, other on error
 */
static int memcg_read_metric(struct memcg_meminfo *info,
                            enum memcg_metric_type metric_type) {
    char path[PATH_MAX];
    char *buf = NULL;
    int ret;
    unsigned long result = 0;

    if (memcg_build_file_path(info, metric_type, path, sizeof(path)) != 0)
        return 1;

    buf = (char *)malloc(CGMEMINFO_LEN);
    if (!buf)
        return ENOMEM;

    ret = read_from_file(path, buf, CGMEMINFO_LEN);
    if (ret != 0) {
        goto out;
    }

    /* For memory.stat, contents are key-value, so parsing is handled separately */
    if (metric_type != MEMCG_MEMORY_STAT) {
        /* cgroup v2 memory.max can return "max", so we need to handle that. */
        if (sscanf(buf, "%lu", &result) != 1) {
            result = ULLONG_MAX;
        }
    }

    ret = EINVAL;
    switch (metric_type) {
        case MEMCG_MEMORY_LIMIT:
            info->cgmem_data.memory_limit = result;
            break;
        case MEMCG_MEMORY_CURRENT:
            info->cgmem_data.memory_current = result;
            break;
        case MEMCG_SWAP_LIMIT:
            if (info->version & CGROUP_TYPE_LEGACY) {
                if (info->cgmem_data.memory_limit > result) {
                    result = 0;
                } else {
                    result -= info->cgmem_data.memory_limit;
                }
            }
            info->cgmem_data.swap_limit = result;
            break;
        case MEMCG_SWAP_CURRENT:
            if (info->version & CGROUP_TYPE_LEGACY) {
                if (info->cgmem_data.memory_current > result
                    || info->cgmem_data.swap_limit == 0) {
                    result = 0;
                } else {
                    result -= info->cgmem_data.memory_current;
                }
            }
            info->cgmem_data.swap_current = result;
            break;
        case MEMCG_MEMORY_STAT:
            memcg_parse_memory_stat(buf, &info->cgmem_data.memory_stat, info->version);
            break;
        default:
            goto out;
    }
    ret = 0;

out:
    free(buf);
    return ret;
}

static int memcg_get_memory_limit(struct memcg_meminfo *info) {
    return memcg_read_metric(info, MEMCG_MEMORY_LIMIT);
}

static int memcg_get_memory_current(struct memcg_meminfo *info) {
    return memcg_read_metric(info, MEMCG_MEMORY_CURRENT);
}

static int memcg_get_swap_limit(struct memcg_meminfo *info) {
    return memcg_read_metric(info, MEMCG_SWAP_LIMIT);
}

static int memcg_get_swap_current(struct memcg_meminfo *info) {
    return memcg_read_metric(info, MEMCG_SWAP_CURRENT);
}

static int memcg_get_memory_stat(struct memcg_meminfo *info) {
    return memcg_read_metric(info, MEMCG_MEMORY_STAT);
}

/*
 * Check if current process exists in cgroup tasks
 * @cgroup_version: cgroup version flags (CGROUP_TYPE_LEGACY or CGROUP_TYPE_UNIFIED)
 * @cgroup_mount: mount point of the cgroup
 * @cgroup_path: path to the cgroup
 *
 * Returns: true if current process exists in cgroup tasks, false otherwise
 */
static bool memcg_process_in_cgroup_tasks(int cgroup_type,
                    const char* cgmount, const char *path) {
    pid_t current_pid = getpid();
    char tasks_file[PATH_MAX];
    FILE *fp;
    pid_t task_pid;
    bool found = false;
    char *line = NULL;
    size_t line_len = 0;
    ssize_t rlen;

    if (cgroup_type & CGROUP_TYPE_UNIFIED) {
        snprintf(tasks_file, sizeof(tasks_file), "%s%s/cgroup.procs", cgmount, path);
    } else if (cgroup_type & CGROUP_TYPE_LEGACY) {
        snprintf(tasks_file, sizeof(tasks_file), "%s%s/tasks", cgmount, path);
    }

    fp = fopen(tasks_file, "r");
    if (!fp)
        return false;

    /* Search for current process ID in the file */
    while ((rlen = getline(&line, &line_len, fp)) != -1) {
        if (sscanf(line, "%d", &task_pid) == 1) {
            if (task_pid == current_pid) {
                found = true;
                break;
            }
        }
    }
    free(line);
    fclose(fp);

    return found;
}

/**
 * Remove the first layer of a path (e.g., "/a/b/c" -> "/b/c")
 * @path: Input path to modify (non-NULL, and not a string constant)

 * Note: The input path string will be modified in-place.
 * For root path ("/"), returns NULL as no layer can be removed.
 *
 * Returns: Pointer to the modified path, or NULL if path is "/"
 */
static char *remove_path_layer(char *path) {
    if (strcmp(path, "/") == 0) {
        return NULL;
    }

    char *first_slash = strchr(path + 1, '/');
    if (first_slash) {
        memmove(path, first_slash, strlen(first_slash) + 1);
    } else {
        strcpy(path, "/");
    }
    return path;
}

/*
 * Traverse cgroup path and find real memory cgroup path
 * @path: path to the cgroup directory
 * @cgroup_type: cgroup version flags
 * @info: pointer to memcg_meminfo structure to populate
 * @found: pointer to bool to indicate if cgroup was found
 *
 * Returns: 0 on success, >0 on error
 */
static int traverse_cgroup_path(const char *path, int cgroup_type,
        struct memcg_meminfo *info, bool *found) {
    char *current_path = NULL;
    char *cgmount = NULL;
    int ret = 0;

    *found = false;

    if (!(current_path = strdup(path)))
        return ENOMEM;

    cgmount = cgroup_mount(cgroup_type);
    if (!cgmount) {
        free(current_path);
        return ENOENT;
    }

    while (current_path && strlen(current_path) > 0) {
        if (memcg_process_in_cgroup_tasks(cgroup_type, cgmount, current_path)) {
            info->version = cgroup_type;
            if (!(info->cgmem_path = strdup(current_path))) {
                ret = ENOMEM;
                goto out;
            }
            info->cgmem_mount = cgmount;
            cgmount = NULL;
            *found = true;
            break;
        }
        current_path = remove_path_layer(current_path);
    }

out:
    free(current_path);
    free(cgmount);

    return ret;
}

static int parse_thread_cgroup_paths(char **v1_path, char **v2_path) {
    FILE *fp;
    char *line = NULL;
    size_t line_len = 0;
    ssize_t rlen;
    int ret = 0;

    /* Check cgroup version by reading /proc/self/cgroup */
    fp = fopen("/proc/self/cgroup", "r");
    if (!fp) {
        return errno;
    }

    while ((rlen = getline(&line, &line_len, fp)) != -1) {
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';

        if (strncmp(line, "0::", 3) == 0) {
            /* cgroup v2 format: "0::/user.slice/user-0.slice" */
            if (!(*v2_path = strdup(line + 3))) {
                ret = ENOMEM;
                goto cleanup;
            }
        } else {
            /* cgroup v1 format: "13:memory:/user.slice/user-0.slice/" */
            char *first_colon = strchr(line, ':');
            if (first_colon) {
                char *second_colon = strchr(first_colon + 1, ':');
                if (second_colon) {
                    *second_colon = '\0';
                    /* Check if subsystems contain "memory" */
                    if (strstr(first_colon + 1, "memory")) {
                        char *path_start = second_colon + 1;
                        char *newline = strchr(path_start, '\n');
                        if (newline) {
                            *newline = '\0';
                        }
                        if (!(*v1_path = strdup(path_start))) {
                            ret = ENOMEM;
                            goto cleanup;
                        }
                    }

                    *second_colon = ':';
                }
            }
        }
    }

cleanup:
    if (line)
        free(line);

    if (fp)
        fclose(fp);

    return ret;
}

static int memcg_get_memory_info(struct memcg_meminfo *info) {
    char *v1_path = NULL;
    char *v2_path = NULL;
    bool found = false;
    int ret = ENOENT;

    ret = parse_thread_cgroup_paths(&v1_path, &v2_path);
    if (ret != 0) {
        goto cleanup;
    }

    if (v1_path)
        traverse_cgroup_path(v1_path, CGROUP_TYPE_LEGACY, info, &found);

    if (!found && v2_path)
        traverse_cgroup_path(v2_path, CGROUP_TYPE_UNIFIED, info, &found);

    if (!found) {
        ret = ENOENT;
        goto cleanup;
    }

    /* Get all memory information - fail fast on any error */
    ret = memcg_get_memory_limit(info);
    if (ret != 0)
        goto cleanup;

    ret = memcg_get_memory_current(info);
    if (ret != 0)
        goto cleanup;

    ret = memcg_get_swap_limit(info);
    if (ret != 0)
        goto cleanup;

    ret = memcg_get_swap_current(info);
    if (ret != 0)
        goto cleanup;

    ret = memcg_get_memory_stat(info);
    if (ret != 0)
        goto cleanup;

    ret = 0;

cleanup:
    free(v1_path);
    free(v2_path);

    return ret;
}

static int memcg_meminfo_unref(struct memcg_meminfo **info) {
    if (info == NULL || *info == NULL)
        return EINVAL;

    (*info)->refcount--;

    if ((*info)->refcount < 1) {
        cleanup_memcg_info(*info);
        free(*info);
        *info = NULL;
        return 0;
    }

    return (*info)->refcount;
}

#define STRLITERALLEN(x) (sizeof(""x"") - 1)
static inline bool startswith(const char *line, const char *pref)
{
    return strncmp(line, pref, strlen(pref)) == 0;
}

int cgroup_meminfo_read_buf(struct meminfo_info *info, char *buf, int len) {
    struct memcg_meminfo *cginfo = NULL;
    struct memcg_data data;
    struct memory_stat mstat;
    int total_len = 0;
    unsigned long memlimit = 0, memusage = 0, swfree = 0, swusage = 0, swtotal = 0;
    FILE *meminfo_file = NULL;
    int ret;
    char *line = NULL;
    size_t line_len = 0;
    ssize_t rlen;

    if (!buf || len <= 0) {
        errno = EINVAL;
        return EINVAL;
    }

    cginfo = calloc(1, sizeof(struct memcg_meminfo));
    if (!cginfo) {
        return ENOMEM;
    }

    ret = memcg_get_memory_info(cginfo);
    if (ret != 0) {
        errno = ret; /* Set errno to the actual error code */
        goto cleanup;
    }

    data = cginfo->cgmem_data;
    mstat = data.memory_stat;

    meminfo_file = fopen("/proc/meminfo", "r");
    if (!meminfo_file) {
        ret = errno;
        goto cleanup;
    }

    memusage = data.memory_current / BYTES_TO_KB;
    memlimit = data.memory_limit / BYTES_TO_KB;
    swtotal = data.swap_limit / BYTES_TO_KB;
    swusage = data.swap_current / BYTES_TO_KB;
    swfree = swtotal - swusage;

    while ((rlen = getline(&line, &line_len, meminfo_file)) != -1) {
        ssize_t l;
        char *printme, lbuf[100];
        memset(lbuf, 0, 100);

        if (startswith(line, "MemTotal:")) {
            unsigned long hosttotal = 0;
            sscanf(line+sizeof("MemTotal:")-1, "%" PRIu64, &hosttotal);
            if (memlimit == 0)
                memlimit = hosttotal;

            if (hosttotal < memlimit)
                memlimit = hosttotal;
            snprintf(lbuf, 100, "MemTotal:       %8" PRIu64 " kB\n", memlimit);
            printme = lbuf;
        } else if (startswith(line, "MemFree:")) {
            snprintf(lbuf, 100, "MemFree:        %8" PRIu64 " kB\n", memlimit - memusage);
            printme = lbuf;
        } else if (startswith(line, "MemAvailable:")) {
            snprintf(lbuf, 100, "MemAvailable:   %8" PRIu64 " kB\n", memlimit - memusage +
                (mstat.total_active_file + mstat.total_inactive_file + mstat.slab_reclaimable) / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "SwapTotal:")) {
            unsigned long hostswtotal = 0;
            sscanf(line + STRLITERALLEN("SwapTotal:"), "%" PRIu64, &hostswtotal);
            if (hostswtotal < swtotal) {
                swtotal = hostswtotal;
            }

            snprintf(lbuf, 100, "SwapTotal:      %8" PRIu64 " kB\n", swtotal);
            printme = lbuf;
        } else if (startswith(line, "SwapFree:")) {
            swfree = swtotal - swusage;
            snprintf(lbuf, 100, "SwapFree:       %8" PRIu64 " kB\n", swfree);
            printme = lbuf;
        } else if (startswith(line, "Slab:")) {
            snprintf(lbuf, 100, "Slab:           %8" PRIu64 " kB\n", mstat.slab / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "Buffers:")) {
            snprintf(lbuf, 100, "Buffers:        %8" PRIu64 " kB\n", (uint64_t)0);
            printme = lbuf;
        } else if (startswith(line, "Cached:")) {
            snprintf(lbuf, 100, "Cached:         %8" PRIu64 " kB\n",
                 mstat.total_cache / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "SwapCached:")) {
            snprintf(lbuf, 100, "SwapCached:     %8" PRIu64 " kB\n", (uint64_t)0);
            printme = lbuf;
        } else if (startswith(line, "Active:")) {
            snprintf(lbuf, 100, "Active:         %8" PRIu64 " kB\n",
                 (mstat.total_active_anon +
                  mstat.total_active_file) /
                     BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "Inactive:")) {
            snprintf(lbuf, 100, "Inactive:       %8" PRIu64 " kB\n",
                 (mstat.total_inactive_anon +
                  mstat.total_inactive_file) /
                     BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "Active(anon):")) {
            snprintf(lbuf, 100, "Active(anon):   %8" PRIu64 " kB\n",
                 mstat.total_active_anon / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "Inactive(anon):")) {
            snprintf(lbuf, 100, "Inactive(anon): %8" PRIu64 " kB\n",
                 mstat.total_inactive_anon / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "Active(file):")) {
            snprintf(lbuf, 100, "Active(file):   %8" PRIu64 " kB\n",
                 mstat.total_active_file / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "Inactive(file):")) {
            snprintf(lbuf, 100, "Inactive(file): %8" PRIu64 " kB\n",
                 mstat.total_inactive_file / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "Unevictable:")) {
            snprintf(lbuf, 100, "Unevictable:    %8" PRIu64 " kB\n",
                 mstat.total_unevictable / BYTES_TO_KB);
            printme = lbuf;
         } else if (startswith(line, "Dirty:")) {
            snprintf(lbuf, 100, "Dirty:          %8" PRIu64 " kB\n",
                 mstat.total_dirty / BYTES_TO_KB);
            printme = lbuf;
         } else if (startswith(line, "Writeback:")) {
            snprintf(lbuf, 100, "Writeback:      %8" PRIu64 " kB\n",
                 mstat.total_writeback / BYTES_TO_KB);
            printme = lbuf;
         } else if (startswith(line, "AnonPages:")) {
            snprintf(lbuf, 100, "AnonPages:      %8" PRIu64 " kB\n",
                 (mstat.total_active_anon +
                  mstat.total_inactive_anon - mstat.total_shmem) /
                     BYTES_TO_KB);
            printme = lbuf;
         } else if (startswith(line, "Mapped:")) {
            snprintf(lbuf, 100, "Mapped:         %8" PRIu64 " kB\n",
                 mstat.total_mapped_file / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "SReclaimable:")) {
            snprintf(lbuf, 100, "SReclaimable:   %8" PRIu64 " kB\n", mstat.slab_reclaimable / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "SUnreclaim:")) {
            snprintf(lbuf, 100, "SUnreclaim:     %8" PRIu64 " kB\n", mstat.slab_unreclaimable / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "Shmem:")) {
            snprintf(lbuf, 100, "Shmem:          %8" PRIu64 " kB\n",
                 mstat.total_shmem / BYTES_TO_KB);
            printme = lbuf;
        } else if (startswith(line, "ShmemHugePages:")) {
            snprintf(lbuf, 100, "ShmemHugePages: %8" PRIu64 " kB\n", (uint64_t)0);
            printme = lbuf;
        } else if (startswith(line, "ShmemPmdMapped:")) {
            snprintf(lbuf, 100, "ShmemPmdMapped: %8" PRIu64 " kB\n", (uint64_t)0);
             printme = lbuf;
         } else if (startswith(line, "AnonHugePages:")) {
            snprintf(lbuf, 100, "AnonHugePages:  %8" PRIu64 " kB\n",
                 mstat.total_rss_huge / BYTES_TO_KB);
            printme = lbuf;
         } else {
             printme = line;
        }

        l = snprintf(buf + total_len, len - total_len, "%s", printme);
        if (l < 0 || l >= (len - total_len)) {
            ret = EOVERFLOW;
            errno = ret; /* Set errno to the actual error code */
            goto cleanup;
        }
        total_len += l;
    }

    buf[total_len] = '\0';
    ret = 0;

cleanup:
    free(line);

    if (meminfo_file)
        fclose(meminfo_file);

    if (cginfo)
        memcg_meminfo_unref(&cginfo);

    return ret;
}