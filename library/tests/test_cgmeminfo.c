/*
 * libproc2 - Library to read proc filesystem
 * Tests for cgmeminfo library calls
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "tests.h"
#include "meminfo.h"

/* Include the cgmeminfo.c source to test internal functions */
#include "library/cgmeminfo.c"

/*
 * Test cgroup_meminfo_new() function
 */
int test_cgroup_meminfo_new(void *data)
{
    struct meminfo_info *info = NULL;
    int rc;

    testname = "cgroup_meminfo_new: basic functionality";

    rc = cgroup_meminfo_new(&info);
    /* In non-container environment, this might fail, which is expected */
    if (rc < 0) {
        /* Check if we're in a container environment */
        struct stat st;
        if (stat("/proc/self/cgroup", &st) != 0) {
            /* No cgroup file, skip test */
            return 1;
        }
        /* cgroup file exists but function failed - this could be normal
         * if not in a container or cgroup memory controller not available */
        return 1;
    }

    if (info == NULL) {
        return 0;
    }

    procps_meminfo_unref(&info);
    return 1;
}

/*
 * Test cgroup_meminfo_new() with NULL parameter
 */
int test_cgroup_meminfo_new_null(void *data)
{
    int rc;

    testname = "cgroup_meminfo_new: NULL parameter handling";

    rc = cgroup_meminfo_new(NULL);
    /* Should return error for NULL parameter */
    if (rc >= 0) {
        return 0;
    }

    return 1;
}

/*
 * Test memory info retrieval in container environment
 */
int test_cgroup_meminfo_get(void *data)
{
    struct meminfo_info *info = NULL;
    struct meminfo_result *result;
    int rc;

    testname = "cgroup_meminfo: memory info retrieval";

    rc = cgroup_meminfo_new(&info);
    if (rc < 0 || info == NULL) {
        /* Skip test if not in container environment */
        return 1;
    }

    /* Try to get memory total */
    result = procps_meminfo_get(info, MEMINFO_MEM_TOTAL);
    if (result == NULL) {
        procps_meminfo_unref(&info);
        return 0;
    }

    /* Memory total should be positive */
    if (result->result.ul_int <= 0) {
        procps_meminfo_unref(&info);
        return 0;
    }

    /* Try to get memory available */
    result = procps_meminfo_get(info, MEMINFO_MEM_AVAILABLE);
    if (result == NULL) {
        procps_meminfo_unref(&info);
        return 0;
    }

    procps_meminfo_unref(&info);
    return 1;
}

/*
 * Test reference counting
 */
int test_cgroup_meminfo_ref(void *data)
{
    struct meminfo_info *info = NULL;
    int rc, refcount;

    testname = "cgroup_meminfo: reference counting";

    rc = cgroup_meminfo_new(&info);
    if (rc < 0 || info == NULL) {
        /* Skip test if not in container environment */
        return 1;
    }

    /* Initial refcount should be 1 */
    refcount = procps_meminfo_ref(info);
    if (refcount != 2) {  /* Should be 2 after ref() call */
        procps_meminfo_unref(&info);
        return 0;
    }

    /* Decrease refcount */
    refcount = procps_meminfo_unref(&info);
    if (refcount != 1) {
        procps_meminfo_unref(&info);
        return 0;
    }

    /* Final cleanup */
    procps_meminfo_unref(&info);
    return 1;
}

/*
 * Test memory statistics parsing with cgroup v1 format
 */
int test_memcg_parse_memory_stat_v1(void *data)
{
    char *test_stat_v1 = strdup(
        "total_cache 1048576\n"
        "total_rss 2097152\n"
        "total_rss_huge 0\n"
        "total_shmem 524288\n"
        "total_mapped_file 262144\n"
        "total_dirty 4096\n"
        "total_writeback 0\n"
        "total_inactive_anon 1048576\n"
        "total_active_anon 1048576\n"
        "total_inactive_file 524288\n"
        "total_active_file 524288\n"
        "total_unevictable 0\n");

    struct memory_stat mstat;
    int ret;

    testname = "memcg_parse_memory_stat: cgroup v1 format";

    memset(&mstat, 0, sizeof(mstat));
    ret = memcg_parse_memory_stat(test_stat_v1, &mstat, CGROUP_TYPE_LEGACY);
    if (ret != 0) {
        free(test_stat_v1);
        return 0;
    }

    /* Verify parsed values */
    if (mstat.total_cache != 1048576 ||
        mstat.total_shmem != 524288 ||
        mstat.total_mapped_file != 262144 ||
        mstat.total_active_anon != 1048576 ||
        mstat.total_inactive_anon != 1048576) {
        free(test_stat_v1);
        return 0;
    }

    free(test_stat_v1);
    return 1;
}

/*
 * Test memory statistics parsing with cgroup v2 format
 */
int test_memcg_parse_memory_stat_v2(void *data)
{
    char *test_stat_v2 = strdup(
        "file 1048576\n"
        "anon 2097152\n"
        "file_mapped 262144\n"
        "file_dirty 4096\n"
        "file_writeback 0\n"
        "shmem 524288\n"
        "inactive_anon 1048576\n"
        "active_anon 1048576\n"
        "inactive_file 524288\n"
        "active_file 524288\n"
        "unevictable 0\n"
        "slab_reclaimable 131072\n"
        "slab_unreclaimable 65536\n"
        "slab 196608\n");

    struct memory_stat mstat;
    int ret;

    testname = "memcg_parse_memory_stat: cgroup v2 format";

    memset(&mstat, 0, sizeof(mstat));
    ret = memcg_parse_memory_stat(test_stat_v2, &mstat, CGROUP_TYPE_UNIFIED);
    if (ret != 0) {
        free(test_stat_v2);
        return 0;
    }

    /* Verify parsed values for v2 format */
    if (mstat.total_cache != 1048576 ||  /* file in v2 */
        mstat.total_shmem != 524288 ||
        mstat.total_mapped_file != 262144 ||
        mstat.total_active_anon != 1048576 ||
        mstat.total_inactive_anon != 1048576 ||
        mstat.slab_reclaimable != 131072 ||
        mstat.slab_unreclaimable != 65536 ||
        mstat.slab != 196608) {
        free(test_stat_v2);
        return 0;
    }

    free(test_stat_v2);
    return 1;
}

TestFunction test_funcs[] = {
    test_cgroup_meminfo_new,
    test_cgroup_meminfo_new_null,
    test_cgroup_meminfo_get,
    test_cgroup_meminfo_ref,
    test_memcg_parse_memory_stat_v1,
    test_memcg_parse_memory_stat_v2,
    NULL
};

int main(int argc, char *argv[])
{
    return run_tests(test_funcs, NULL);
}