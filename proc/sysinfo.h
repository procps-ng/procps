#ifndef PROC_SYSINFO_H
#define PROC_SYSINFO_H
#include <sys/types.h>
#include <dirent.h>
#include "procps.h"

EXTERN_C_BEGIN

extern unsigned long long Hertz;   /* clock tick frequency */
extern long smp_num_cpus;          /* number of CPUs */
extern int have_privs;             /* boolean, true if setuid or similar */
extern long page_bytes;            /* this architecture's bytes per page */

#if 0
#define JT double
extern void eight_cpu_numbers(JT *uret, JT *nret, JT *sret, JT *iret, JT *wret, JT *xret, JT *yret, JT *zret);
#undef JT
#endif

extern int        uptime (double *uptime_secs, double *idle_secs);
extern unsigned long getbtime(void);
extern void       loadavg(double *av1, double *av5, double *av15);

/* Shmem in 2.6.32+ */
extern unsigned long kb_main_shared;
/* old but still kicking -- the important stuff */
extern unsigned long kb_main_buffers;
extern unsigned long kb_main_cached;
extern unsigned long kb_main_free;
extern unsigned long kb_main_total;
extern unsigned long kb_swap_free;
extern unsigned long kb_swap_total;
/* recently introduced */
extern unsigned long kb_high_free;
extern unsigned long kb_high_total;
extern unsigned long kb_low_free;
extern unsigned long kb_low_total;
extern unsigned long kb_main_available;
/* 2.4.xx era */
extern unsigned long kb_active;
extern unsigned long kb_inact_laundry;  // grrr...
extern unsigned long kb_inact_dirty;
extern unsigned long kb_inact_clean;
extern unsigned long kb_inact_target;
extern unsigned long kb_swap_cached;  /* late 2.4+ */
/* derived values */
extern unsigned long kb_swap_used;
extern unsigned long kb_main_used;
/* 2.5.41+ */
extern unsigned long kb_writeback;
extern unsigned long kb_slab;
extern unsigned long nr_reversemaps;
extern unsigned long kb_committed_as;
extern unsigned long kb_dirty;
extern unsigned long kb_inactive;
extern unsigned long kb_mapped;
extern unsigned long kb_pagetables;

#define BUFFSIZE (64*1024)
typedef unsigned long long jiff;
extern void getstat(jiff *__restrict cuse, jiff *__restrict cice, jiff *__restrict csys, jiff *__restrict cide, jiff *__restrict ciow, jiff *__restrict cxxx, jiff *__restrict cyyy, jiff *__restrict czzz,
	     unsigned long *__restrict pin, unsigned long *__restrict pout, unsigned long *__restrict s_in, unsigned long *__restrict sout,
	     unsigned *__restrict intr, unsigned *__restrict ctxt,
	     unsigned int *__restrict running, unsigned int *__restrict blocked,
	     unsigned int *__restrict btime, unsigned int *__restrict processes);

extern void meminfo(void);


extern unsigned long vm_nr_dirty;
extern unsigned long vm_nr_writeback;
extern unsigned long vm_nr_pagecache;
extern unsigned long vm_nr_page_table_pages;
extern unsigned long vm_nr_reverse_maps;
extern unsigned long vm_nr_mapped;
extern unsigned long vm_nr_slab;
extern unsigned long vm_nr_slab_reclaimable;
extern unsigned long vm_nr_slab_unreclaimable;
extern unsigned long vm_nr_active_file;
extern unsigned long vm_nr_inactive_file;
extern unsigned long vm_nr_free_pages;
extern unsigned long vm_pgpgin;
extern unsigned long vm_pgpgout;
extern unsigned long vm_pswpin;
extern unsigned long vm_pswpout;
extern unsigned long vm_pgalloc;
extern unsigned long vm_pgfree;
extern unsigned long vm_pgactivate;
extern unsigned long vm_pgdeactivate;
extern unsigned long vm_pgfault;
extern unsigned long vm_pgmajfault;
extern unsigned long vm_pgscan;
extern unsigned long vm_pgrefill;
extern unsigned long vm_pgsteal;
extern unsigned long vm_kswapd_steal;
extern unsigned long vm_pageoutrun;
extern unsigned long vm_allocstall;

extern void vminfo(void);

/* extern ZFS ARC globals */
extern unsigned long arc_anon_evictable_data;
extern unsigned long arc_anon_evictable_metadata;
extern unsigned long arc_anon_size;
extern unsigned long arc_arc_loaned_bytes;
extern unsigned long arc_arc_meta_limit;
extern unsigned long arc_arc_meta_max;
extern unsigned long arc_arc_meta_min;
extern unsigned long arc_arc_meta_used;
extern unsigned long arc_arc_need_free;
extern unsigned long arc_arc_no_grow;
extern unsigned long arc_arc_prune;
extern unsigned long arc_arc_sys_free;
extern unsigned long arc_arc_tempreserve;
extern unsigned long arc_c;
extern unsigned long arc_c_max;
extern unsigned long arc_c_min;
extern unsigned long arc_data_size;
extern unsigned long arc_deleted;
extern unsigned long arc_demand_data_hits;
extern unsigned long arc_demand_data_misses;
extern unsigned long arc_demand_metadata_hits;
extern unsigned long arc_demand_metadata_misses;
extern unsigned long arc_duplicate_buffers;
extern unsigned long arc_duplicate_buffers_size;
extern unsigned long arc_duplicate_reads;
extern unsigned long arc_evict_l2_cached;
extern unsigned long arc_evict_l2_eligible;
extern unsigned long arc_evict_l2_ineligible;
extern unsigned long arc_evict_l2_skip;
extern unsigned long arc_evict_not_enough;
extern unsigned long arc_evict_skip;
extern unsigned long arc_hash_chain_max;
extern unsigned long arc_hash_chains;
extern unsigned long arc_hash_collisions;
extern unsigned long arc_hash_elements;
extern unsigned long arc_hash_elements_max;
extern unsigned long arc_hdr_size;
extern unsigned long arc_hits;
extern unsigned long arc_l2_abort_lowmem;
extern unsigned long arc_l2_asize;
extern unsigned long arc_l2_cdata_free_on_write;
extern unsigned long arc_l2_cksum_bad;
extern unsigned long arc_l2_compress_failures;
extern unsigned long arc_l2_compress_successes;
extern unsigned long arc_l2_compress_zeros;
extern unsigned long arc_l2_evict_l1cached;
extern unsigned long arc_l2_evict_lock_retry;
extern unsigned long arc_l2_evict_reading;
extern unsigned long arc_l2_feeds;
extern unsigned long arc_l2_free_on_write;
extern unsigned long arc_l2_hdr_size;
extern unsigned long arc_l2_hits;
extern unsigned long arc_l2_io_error;
extern unsigned long arc_l2_misses;
extern unsigned long arc_l2_read_bytes;
extern unsigned long arc_l2_rw_clash;
extern unsigned long arc_l2_size;
extern unsigned long arc_l2_write_bytes;
extern unsigned long arc_l2_writes_done;
extern unsigned long arc_l2_writes_error;
extern unsigned long arc_l2_writes_lock_retry;
extern unsigned long arc_l2_writes_sent;
extern unsigned long arc_memory_direct_count;
extern unsigned long arc_memory_indirect_count;
extern unsigned long arc_memory_throttle_count;
extern unsigned long arc_metadata_size;
extern unsigned long arc_mfu_evictable_data;
extern unsigned long arc_mfu_evictable_metadata;
extern unsigned long arc_mfu_ghost_evictable_data;
extern unsigned long arc_mfu_ghost_evictable_metadata;
extern unsigned long arc_mfu_ghost_hits;
extern unsigned long arc_mfu_ghost_size;
extern unsigned long arc_mfu_hits;
extern unsigned long arc_mfu_size;
extern unsigned long arc_misses;
extern unsigned long arc_mru_evictable_data;
extern unsigned long arc_mru_evictable_metadata;
extern unsigned long arc_mru_ghost_evictable_data;
extern unsigned long arc_mru_ghost_evictable_metadata;
extern unsigned long arc_mru_ghost_hits;
extern unsigned long arc_mru_ghost_size;
extern unsigned long arc_mru_hits;
extern unsigned long arc_mru_size;
extern unsigned long arc_mutex_miss;
extern unsigned long arc_other_size;
extern unsigned long arc_p;
extern unsigned long arc_prefetch_data_hits;
extern unsigned long arc_prefetch_data_misses;
extern unsigned long arc_prefetch_metadata_hits;
extern unsigned long arc_prefetch_metadata_misses;
extern unsigned long arc_size;

extern void arcstats(void);

typedef struct disk_stat{
	unsigned long long reads_sectors;
	unsigned long long written_sectors;
	char               disk_name [32];
	unsigned           inprogress_IO;
	unsigned           merged_reads;
	unsigned           merged_writes;
	unsigned           milli_reading;
	unsigned           milli_spent_IO;
	unsigned           milli_writing;
	unsigned           partitions;
	unsigned           reads;
	unsigned           weighted_milli_spent_IO;
	unsigned           writes;
}disk_stat;

typedef struct partition_stat{
	char partition_name [35];
	unsigned long long reads_sectors;
	unsigned           parent_disk;  // index into a struct disk_stat array
	unsigned           reads;
	unsigned           writes;
	unsigned long long requested_writes;
}partition_stat;

extern unsigned int getpartitions_num(struct disk_stat *disks, int ndisks);
extern unsigned int getdiskstat (struct disk_stat**,struct partition_stat**);

typedef struct slab_cache{
	char name[48];
	unsigned active_objs;
	unsigned num_objs;
	unsigned objsize;
	unsigned objperslab;
}slab_cache;

extern unsigned int getslabinfo (struct slab_cache**);

extern unsigned get_pid_digits(void) FUNCTION;

extern void cpuinfo (void);

EXTERN_C_END
#endif /* SYSINFO_H */
