/* Copyright (C) NIOVA Systems, Inc - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Paul Nowoczynski <00pauln00@gmail.com> 2020
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include <rocksdb/c.h>

#include "common.h"
#include "log.h"
#include "random.h"

#define OPTS "hi:sp:DdW"
static size_t testIterations = 10000;
static const char *niovaRocksDbPathRoot = "./rocksdb__test_dir";
static bool useWriteBatch = false;

struct niova_rocksdb_instance
{
    uuid_t                  nri_uuid;
    char                    nri_path[PATH_MAX];
    struct random_data      nri_rand_data;
    char                    nri_rand_state_buf[RANDOM_STATE_BUF_LEN];
    rocksdb_t              *nri_db;
    rocksdb_options_t      *nri_options;
    rocksdb_writeoptions_t *nri_writeoptions;
    rocksdb_readoptions_t  *nri_readoptions;
    bool                    nri_sync_write;
    bool                    nri_direct_io_flush_compaction;
    bool                    nri_direct_reads;
};

static int
nri_options_setup(struct niova_rocksdb_instance *nri)
{
    if (!nri)
        return -EINVAL;
    else if (nri->nri_options)
        return -EALREADY;

    nri->nri_options = rocksdb_options_create();
    if (!nri->nri_options)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "rocksdb_options_create() returned NULL");
        return -ENOMEM;
    }

    const long int cpus = sysconf(_SC_NPROCESSORS_ONLN);
    rocksdb_options_increase_parallelism(nri->nri_options, (int)(cpus));
//rocksdb_options_optimize_level_style_compaction(nri->nri_options, 0);
    rocksdb_options_set_create_if_missing(nri->nri_options, 1);

    rocksdb_options_set_use_direct_reads(nri->nri_options,
                                         nri->nri_direct_reads);
    rocksdb_options_set_use_direct_io_for_flush_and_compaction(
        nri->nri_options, nri->nri_direct_io_flush_compaction);

    return 0;
}

static void
nri_options_destroy(struct niova_rocksdb_instance *nri)
{
    if (!nri || !nri->nri_options)
        return;

    rocksdb_writeoptions_destroy(nri->nri_writeoptions);
    rocksdb_readoptions_destroy(nri->nri_readoptions);
    rocksdb_options_destroy(nri->nri_options);

    nri->nri_options = NULL;
    nri->nri_readoptions = NULL;
    nri->nri_writeoptions = NULL;
}

static int
nri_db_close(struct niova_rocksdb_instance *nri)
{
    if (!nri)
        return -EINVAL;
    else if (!nri->nri_db)
	return -EALREADY;

    rocksdb_close(nri->nri_db);

    nri->nri_db = NULL;
    nri_options_destroy(nri);

    return 0;
}

static int
nri_db_open(struct niova_rocksdb_instance *nri)
{
    if (!nri || !nri->nri_options || !nri->nri_path)
        return -EINVAL;
    else if (nri->nri_db)
        return -EALREADY;

    char *err = NULL;

    nri->nri_db = rocksdb_open(nri->nri_options, nri->nri_path, &err);
    if (!nri->nri_db)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "rocksdb_open(): %s", err);
        return -ENOTCONN;
    }

    nri->nri_writeoptions = rocksdb_writeoptions_create();
    if (!nri->nri_writeoptions)
    {
        nri_db_close(nri);
        return -ENOMEM;
    }

    rocksdb_writeoptions_set_sync(nri->nri_writeoptions, nri->nri_sync_write);

    nri->nri_readoptions = rocksdb_readoptions_create();
    if (!nri->nri_readoptions)
    {
        nri_db_close(nri);
        return -ENOMEM;
    }

    return 0;
}

static uint32_t
nri_random_get(struct niova_rocksdb_instance *nri)
{
    unsigned int result;

    if (random_r(&nri->nri_rand_data, (int *)&result))
        SIMPLE_LOG_MSG(LL_FATAL, "random_r() failed: %s", strerror(errno));

    return result;
}

static void
nri_random_init(struct niova_rocksdb_instance *nri)
{
    NIOVA_ASSERT(nri);
    NIOVA_ASSERT(!uuid_is_null(nri->nri_uuid));

    if (initstate_r(random_create_seed_from_uuid(nri->nri_uuid),
                    nri->nri_rand_state_buf,
                    RANDOM_STATE_BUF_LEN, &nri->nri_rand_data))
        SIMPLE_LOG_MSG(LL_FATAL, "initstate_r() failed: %s", strerror(errno));
}

#define MAX_KEY_LEN 1024

static int
nri_put_some_items(struct niova_rocksdb_instance *nri, char **err)
{
    if (!nri || !err)
        return -EINVAL;

    char uuid_str[UUID_STR_LEN];
    uuid_unparse(nri->nri_uuid, uuid_str);

    char key[MAX_KEY_LEN];
    char value[MAX_KEY_LEN];

    char key_header[MAX_KEY_LEN];

    const size_t key_header_len =
        snprintf(key_header, MAX_KEY_LEN, "HEADER_%s", uuid_str);

    rocksdb_put(nri->nri_db, nri->nri_writeoptions, key_header,
                key_header_len, "X", 1, err);

    for (size_t i = 0; i < testIterations; i++)
    {
        *err = NULL;

        const size_t key_len =
            snprintf(key, MAX_KEY_LEN, "%s.%zu", uuid_str, i);

        const size_t value_len = snprintf(value, MAX_KEY_LEN, "%zu.%u", i,
                                          nri_random_get(nri));
        rocksdb_put(nri->nri_db, nri->nri_writeoptions, key, key_len, value,
                    value_len, err);

        if (*err)
            return -1;

        SIMPLE_LOG_MSG(LL_DEBUG, "key=%s val=%s: error=%s", key, value, *err);
    }

    return 0;
}

static int
nri_put_some_items_wbatch(struct niova_rocksdb_instance *nri, char **err)
{
    if (!nri || !err)
        return -EINVAL;

    char uuid_str[UUID_STR_LEN];
    uuid_unparse(nri->nri_uuid, uuid_str);

    char key[MAX_KEY_LEN];
    char value[MAX_KEY_LEN];

    char key_header[MAX_KEY_LEN];
    char value_header[MAX_KEY_LEN];

    const size_t key_header_len =
        snprintf(key_header, MAX_KEY_LEN, "HEADER_%s", uuid_str);

    size_t value_header_state = 0;

    rocksdb_writebatch_t *wb = rocksdb_writebatch_create();

    for (size_t i = 0; i < testIterations; i++)
    {
        *err = NULL;

        const size_t key_len =
           snprintf(key, MAX_KEY_LEN, "%s.%zu", uuid_str, i);

        const uint32_t rand = nri_random_get(nri);

        const size_t value_len = snprintf(value, MAX_KEY_LEN, "%zu.%u", i,
                                          rand);

        rocksdb_writebatch_put(wb, key, key_len, value, value_len);

        value_header_state += rand;

        const size_t value_header_len =
            snprintf(value_header, MAX_KEY_LEN, "%zu.%zx", i,
                     value_header_state);

        rocksdb_writebatch_put(wb, key_header, key_header_len, value_header,
                               value_header_len);

        rocksdb_write(nri->nri_db, nri->nri_writeoptions, wb, err);

        if (*err)
            return -1;

        SIMPLE_LOG_MSG(LL_DEBUG, "key=%s val=%s: error=%s", key, value, *err);

        rocksdb_writebatch_clear(wb);
    }

    rocksdb_writebatch_destroy(wb);

    return 0;
}

static int
nri_get_some_items(struct niova_rocksdb_instance *nri, const size_t *key_array,
                   char **err)
{
    if (!nri || !err)
        return -EINVAL;

    char uuid_str[UUID_STR_LEN];
    uuid_unparse(nri->nri_uuid, uuid_str);

    char key[MAX_KEY_LEN];

    char key_header[MAX_KEY_LEN];
    const size_t key_header_len =
        snprintf(key_header, MAX_KEY_LEN, "HEADER_%s", uuid_str);

    size_t value_len = 0;
    char *value = rocksdb_get(nri->nri_db, nri->nri_readoptions, key_header,
                              key_header_len, &value_len, err);

    SIMPLE_LOG_MSG(LL_DEBUG, "key-header=%s val-header=%s: error=%s",
                   key_header, value, *err);
    free(value);

    for (size_t i = 0; i < testIterations; i++)
    {
        *err = NULL;

        const size_t key_len =
            snprintf(key, MAX_KEY_LEN, "%s.%zu", uuid_str,
                     key_array ? key_array[i] : i);

        size_t value_len = 0;

        char *value = rocksdb_get(nri->nri_db, nri->nri_readoptions, key,
                                  key_len, &value_len, err);

        if (*err)
            return -1;

        SIMPLE_LOG_MSG(LL_DEBUG, "key=%s val=%s: error=%s", key, value, *err);

        free(value);
    }

    return 0;
}

static size_t *
rocksdb_test_generate_random_key_array(void)
{
    size_t *key_array = malloc(sizeof(size_t) * testIterations);

    for (size_t i = 0; i < testIterations; i++)
        key_array[i] = i;

    for (size_t i = 0; i < testIterations; i++)
    {
        const unsigned int swap_idx = random_get() % testIterations;
        const size_t tmp = key_array[i];

        key_array[i] = key_array[swap_idx];
        key_array[swap_idx] = tmp;
    }

    return key_array;
}

static void
rocksdb_test_print_help(const int error, char **argv)
{
    fprintf(
        error ? stderr : stdout,
        "Usage: %s [-D (direct-io-flush-compaction)] [-d (direct-io-reads)] [-W (use write batch)] [-i iterations] [-s (sync-write)] [-p path] [-h] \n",
        argv[0]);

    exit(error);
}

static void
rocksdb_test_getopt(int argc, char **argv, struct niova_rocksdb_instance *nri)
{
    if (!argc || !argv || !nri)
        return;

    int opt;

    while ((opt = getopt(argc, argv, OPTS)) != -1)
    {
        switch (opt)
	{
        case 'D':
            nri->nri_direct_io_flush_compaction = true;
            break;
        case 'd':
            nri->nri_direct_reads = true;
            break;
        case 's':
            nri->nri_sync_write = true;
            break;
        case 'h':
            rocksdb_test_print_help(0, argv);
            break;
        case 'i':
            testIterations = atoll(optarg);
            break;
        case 'p':
            niovaRocksDbPathRoot = optarg;
            break;
        case 'W':
            useWriteBatch = true;
            break;
        default:
            rocksdb_test_print_help(EINVAL, argv);
            break;
        }
    }
}

int
main(int argc, char *argv[])
{
    struct niova_rocksdb_instance nri = {0};

    rocksdb_test_getopt(argc, argv, &nri);

    char uuid_str[UUID_STR_LEN];
    uuid_generate(nri.nri_uuid);
    uuid_unparse(nri.nri_uuid, uuid_str);

    nri_random_init(&nri);

    snprintf(nri.nri_path, PATH_MAX, "%s-%s", niovaRocksDbPathRoot, uuid_str);

    int rc = nri_options_setup(&nri);
    if (rc)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "nri_options_setup(): %s", strerror(-rc));
        exit(rc);
    }

    unsigned long long num_nsecs;
    struct timespec ts[2];

    fprintf(stdout, "    NS/OP\t\tTest Name\n"
            "--------------------------------------------------\n");

    // DB Open
    niova_unstable_clock(&ts[0]);

    rc = nri_db_open(&nri);
    if (rc)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "nri_options_open(): %s", strerror(-rc));
        exit(rc);
    }

    niova_unstable_clock(&ts[1]);
    timespecsub(&ts[1], &ts[0], &ts[0]);
    num_nsecs = timespec_2_nsec(&ts[0]);

    fprintf(stdout, "%13.3f\t\t%s\n",
            (float)num_nsecs / (float)1, "rocksdb_open");

    // DB Write
    niova_unstable_clock(&ts[0]);

    char *err;
    rc = useWriteBatch ?
        nri_put_some_items_wbatch(&nri, &err) : nri_put_some_items(&nri, &err);
    if (rc)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "nri_put_some_items(): %s", err);
        exit(rc);
    }

    niova_unstable_clock(&ts[1]);
    timespecsub(&ts[1], &ts[0], &ts[0]);
    num_nsecs = timespec_2_nsec(&ts[0]);

    fprintf(stdout, "%13.3f\t\t%s",
            (float)num_nsecs / (float)testIterations,
            nri.nri_sync_write ? "rocksdb_put_sync" : "rocksdb_put");

    fprintf(stdout, "%s\n", useWriteBatch ? "_with_batch" : "");

    // DB Read
    niova_unstable_clock(&ts[0]);

    rc = nri_get_some_items(&nri, NULL, &err);
    if (rc)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "nri_put_some_items(): %s", err);
        exit(rc);
    }

    niova_unstable_clock(&ts[1]);
    timespecsub(&ts[1], &ts[0], &ts[0]);
    num_nsecs = timespec_2_nsec(&ts[0]);

    fprintf(stdout, "%13.3f\t\t%s\n",
            (float)num_nsecs / (float)testIterations, "rocksdb_get");


    // DB Random Read
    size_t *rand_key = rocksdb_test_generate_random_key_array();
    if (!rand_key)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "nri_put_some_items(): %s", strerror(ENOMEM));
        exit(-ENOMEM);
    }

    niova_unstable_clock(&ts[0]);

    rc = nri_get_some_items(&nri, rand_key, &err);
    if (rc)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "nri_put_some_items(): %s", err);
        exit(rc);
    }

    niova_unstable_clock(&ts[1]);
    timespecsub(&ts[1], &ts[0], &ts[0]);
    num_nsecs = timespec_2_nsec(&ts[0]);

    fprintf(stdout, "%13.3f\t\t%s\n",
            (float)num_nsecs / (float)testIterations, "rocksdb_get_random");

    free(rand_key);

// DB Close
    niova_unstable_clock(&ts[0]);

    rc = nri_db_close(&nri);
    if (rc)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "nri_db_close(): %s", strerror(-rc));
        exit(rc);
    }

    niova_unstable_clock(&ts[1]);
    timespecsub(&ts[1], &ts[0], &ts[0]);
    num_nsecs = timespec_2_nsec(&ts[0]);

    fprintf(stdout, "%13.3f\t\t%s\n",
            (float)num_nsecs / 1, "rocksdb_close");

    return 0;
}
