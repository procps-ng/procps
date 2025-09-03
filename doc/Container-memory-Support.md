# Container Memory Support
===========

## Overview

Currently, containers do not have isolated resource views. As a result, commands
such as free, vmstat executed within a container reflect the host's global
memory state rather than the container's actual constrained resources. This
behavior is inherently misleading—especially when memory limits are imposed on
the container, since the output does not represent the resources truly
available to it.

To resolve this discrepancy, we now extract memory usage data directly from the
container's corresponding cgroup. This enhancement enables free, vmstat to
report container-specific memory information. When run inside a container, these
commands can utilize dedicated parameters to display memory usage based on
cgroup limits, providing an accurate view of the container's resource context
instead of inheriting the host's memory statistics.

## Features Added

### 1. Cgroup Version Support
- Supports both cgroup v1 and cgroup v2
- Automatically detects which version is in use

### 2. Container Memory Information
- Reads comprehensive memory limits and usage from cgroup instead of
  `/proc/meminfo`
- Provides accurate memory information within container limits
- Supports both memory and swap information when available

### 3. New Command Line Options For `free`, `vmstat`
- `--container` option to container mode

#### `--container` Option Design

This implementation adds a `--container` option rather than automatically
switching display modes based on container detection. This design decision is
based on the following considerations:

- **Resource Representation**: The limits set in cgroup represent restrictions
  on container resources, not the actual physical resources available within
  the container
- **User Control**: The `--container` option allows users to explicitly choose
  which view they need:
  - With the option: View container-specific resource limits and usage
  - Without the option: Access the global memory state of the host system
- **Operational Flexibility**: This approach enables users to obtain resource
  information from either perspective (container or host) as needed
- **Diagnostic Capabilities**: Maintaining access to host memory data is
  crucial for comprehensive troubleshooting scenarios

This flexible design enhances operational diagnostic capabilities compared to an
automatic detection approach that would force container-only views when in
container environments.

## Files Added/Modified

### New Files:
- `library/cgmeminfo.c` - Implementation of container memory detection and reading

### Modified Files:
- `src/free.c` - Enhanced to support container memory display
- `src/vmstat.c` - Enhanced to use container memory information when available

## Usage Examples

### free Container Support
When executed within a container, `free --container` detects the container's
memory cgroup path and displays its memory usage and limits:

```bash
## Start container with memory limits: 256M memory + 256M swap
$ docker run -m 256M --memory-swap 512M -it ubuntu

## Display container memory information
$ free --container -h
               total        used        free      shared  buff/cache   available
Mem:           256Mi       3.0Mi       252Mi          0B       405Ki       252Mi
Swap:          256Mi          0B       256Mi

## Display host memory information
$ free -h
               total        used        free      shared  buff/cache   available
Mem:            62Gi        16Gi        41Gi       1.3Gi       6.1Gi        45Gi
Swap:           15Gi        84Mi        15Gi
```


### vmstat Container Support
The `vmstat` command automatically detects container environments and uses
cgroup memory information:

```bash
## Start container with memory limits: 256M memory + 256M swap
$ docker run -m 256M --memory-swap 512M -it ubuntu

## Display container memory information
$ vmstat -s M --container
procs -----------memory---------- ---swap-- -----io---- -system-- -------cpu-------
 r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st gu
 2  0      0    257      0      5    0    0   376   330 12515   5  1  1 98  0  0  0

## Display host memory information
$ vmstat -s M
procs -----------memory---------- ---swap-- -----io---- -system-- -------cpu-------
 r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st gu
 3  1     92  45621     28   2298    0    0   376   330 12515   5  1  1 98  0  0  0
```

When running in a container, vmstat shows memory statistics based on container
limits rather than host system memory. The memory columns (free, buff, cache)
reflect the container's cgroup memory constraints.
``

## Technical Details

### Container Cgroup Path Logic
The implementation uses a sophisticated multi-step approach to detect and
validate container environments:

1. **Cgroup Path Analysis**: Examines `/proc/self/cgroup` to identify:
   - cgroup v2 entries (format: `0::/path`)
   - cgroup v1 memory controller entries (format: `x:memory:/path`)

2. **Cgroup Hierarchy Traversal**: For each detected path:
   - Attempts to find the current process in cgroup tasks/cgroup.procs files
   - Traverses up the cgroup hierarchy if not found in the initial path
   - Validates process membership in the cgroup

3. **Mount Point Detection**: Automatically locates cgroup mount points:
   - cgroup v2: Searches for `cgroup2` filesystem type
   - cgroup v1: Searches for `cgroup` filesystem with `memory` option


### Cgroup Memory Information Retrieval

The implementation provides comprehensive memory information by reading various
cgroup files:

#### Memory Limits and Usage
- **Memory Limit**: Maximum memory that can be used by the container
  - cgroup v1: `memory.limit_in_bytes`
  - cgroup v2: `memory.max` (returns "max" for unlimited)
- **Memory Current**: Current memory usage
  - cgroup v1: `memory.usage_in_bytes`
  - cgroup v2: `memory.current`

#### Swap Information
- **Swap Limit**: Maximum swap space available
  - cgroup v1: `memory.memsw.limit_in_bytes` (includes memory + swap)
  - cgroup v2: `memory.swap.max`
- **Swap Current**: Current swap usage
  - cgroup v1: `memory.memsw.usage_in_bytes` (includes memory + swap)
  - cgroup v2: `memory.swap.current`

#### Detailed Memory Statistics (`memory.stat`)
The implementation parses comprehensive memory statistics including:

**File and Cache Memory:**
- `total_cache` / `file`: File cache memory
- `total_mapped_file` / `file_mapped`: Memory-mapped files
- `total_dirty` / `file_dirty`: Dirty file pages
- `total_writeback` / `file_writeback`: Pages being written back

**Anonymous Memory:**
- `total_active_anon` / `active_anon`: Active anonymous pages
- `total_inactive_anon` / `inactive_anon`: Inactive anonymous pages
- `total_rss_huge`: Huge page RSS (cgroup v1 only)

**File Memory:**
- `total_active_file` / `active_file`: Active file pages
- `total_inactive_file` / `inactive_file`: Inactive file pages

**Shared and Special Memory:**
- `total_shmem` / `shmem`: Shared memory
- `total_unevictable` / `unevictable`: Unevictable pages

**Kernel Memory (cgroup v2 only):**
- `slab_reclaimable`: Reclaimable slab memory
- `slab_unreclaimable`: Unreclaimable slab memory
- `slab`: Total slab memory

#### Memory Statistics Mapping
vmstat maps container memory information to its standard output format:

- **swpd**: Swap used (from cgroup swap.current)
- **free**: Free memory (calculated from container memory limit - current usage)
- **buff**: Buffer memory (typically 0 in containers, mapped from cgroup cache statistics)
- **cache**: Cache memory (from cgroup file cache statistics)

#### Data Conversion and Processing
- All memory values are converted from bytes to KB for consistency with
  `/proc/meminfo`
- For cgroup v1, swap values are calculated by subtracting memory usage from
  memsw values
- Memory statistics are mapped to standard `/proc/meminfo` format for
  compatibility
- Host memory limits are used as fallback when container limits exceed host
  capacity

## Programming Examples

### Basic Usage
```c
#include "meminfo.h"

int main() {
    struct meminfo_info *info = NULL;
    struct meminfo_result *result;
    
    // Initialize container memory info
    if (is_container != 0) {
      rc = cgroup_meminfo_new(&mem_info);
    } else {
      rc = procps_meminfo_new(&mem_info);
    }
    
    // Get memory total
    result = procps_meminfo_get(info, MEMINFO_MEM_TOTAL);
    if (result) {
        printf("Memory Total: %lu KB\n", result->result.ul_int);
    }
    
    // Get memory usage
    result = procps_meminfo_get(info, MEMINFO_MEM_USED);
    if (result) {
        printf("Memory Used: %lu KB\n", result->result.ul_int);
    }
    
    // Get swap information
    result = procps_meminfo_get(info, MEMINFO_SWAP_TOTAL);
    if (result) {
        printf("Swap Total: %lu KB\n", result->result.ul_int);
    }
    
    result = procps_meminfo_get(info, MEMINFO_SWAP_USED);
    if (result) {
        printf("Swap Used: %lu KB\n", result->result.ul_int);
    }
    
    // Clean up
    procps_meminfo_unref(&info);
    return 0;
}
```

## Error Handling and Diagnostics

### Common Error Scenarios
1. **Container Detection Failure**: When running outside a container or in
   unsupported environments
2. **Permission Issues**: Insufficient permissions to read cgroup files
3. **Missing Cgroup Files**: Cgroup controllers not enabled or files not
   available
4. **Invalid Cgroup Paths**: Malformed or inaccessible cgroup paths


## Benefits

### For Both free and vmstat Commands

1. **Accurate Container Memory Reporting**: Shows actual container limits
   instead of host memory
2. **Both Cgroup Versions**: Supports both cgroup v1 and v2
3. **Comprehensive Statistics**: Provides detailed memory breakdown including
   cache, buffers, and swap
