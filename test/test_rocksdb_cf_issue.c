/**
 * Minimal test program to reproduce the RocksDB CF assertion issue
 * 
 * This program:
 * 1. Opens a RocksDB instance with multiple column families
 * 2. Closes the DB WITHOUT explicitly destroying the CF handles
 * 3. Should trigger: rocksdb::ColumnFamilySet::~ColumnFamilySet(): Assertion `last_ref' failed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rocksdb/c.h>

#define TEST_DB_PATH "/tmp/test_rocksdb_cf_issue"

int
main(int argc, char *argv[])
{
    printf("=== RocksDB CF Handle Assertion Test ===\n\n");
    
    rocksdb_t *db = NULL;
    rocksdb_options_t *options = NULL;
    rocksdb_column_family_handle_t **cf_handles = NULL;
    char *err = NULL;
    int test_mode = 1; // 1 = without releasing CF handles, 2 = with releasing
    
    // Allow override via command line
    if (argc > 1 && argv[1][0] == '2')
        test_mode = 2;
    
    printf("Test Mode: %d (%s CF handles before close)\n\n",
           test_mode, test_mode == 1 ? "WITHOUT releasing" : "WITH releasing");
    
    // Clean up any previous test DB
    system("rm -rf " TEST_DB_PATH);
    
    // Create options
    options = rocksdb_options_create();
    rocksdb_options_set_create_if_missing(options, 1);
    
    printf("[Step 1] Opening DB with default column family...\n");
    
    // For this test, we'll just open default CF but keep handles to it
    // The key is to have CF handles that are NOT destroyed before rocksdb_close()
    const char *cf_names[] = {"default"};
    size_t num_cf = 1;
    rocksdb_options_t *cf_options[1];
    
    cf_options[0] = rocksdb_options_create();
    
    // Allocate CF handles array
    cf_handles = (rocksdb_column_family_handle_t **)malloc(
        num_cf * sizeof(rocksdb_column_family_handle_t *));
    
    // Open DB with column families
    printf("  Opening RocksDB with %zu column family...\n", num_cf);
    db = rocksdb_open_column_families(
        options,
        TEST_DB_PATH,
        num_cf,
        cf_names,
        (const rocksdb_options_t *const *)cf_options,
        cf_handles,
        &err);
    
    if (err) {
        printf("[ERROR] rocksdb_open_column_families() failed: %s\n", err);
        free(err);
        return 1;
    }
    
    printf("[SUCCESS] DB opened with %zu column families\n", num_cf);
    for (size_t i = 0; i < num_cf; i++) {
        printf("  CF[%zu] = %s (handle=%p)\n", i, cf_names[i], (void *)cf_handles[i]);
    }
    
    // // Do some basic operations
    // printf("\n[Step 2] Writing some data...\n");
    // rocksdb_writeoptions_t *write_opts = rocksdb_writeoptions_create();
    
    // for (size_t i = 0; i < num_cf; i++) {
    //     char key[64], val[64];
    //     snprintf(key, sizeof(key), "key_%zu", i);
    //     snprintf(val, sizeof(val), "value_for_cf_%zu", i);
        
    //     rocksdb_put_cf(db, write_opts, cf_handles[i], key, strlen(key), val, strlen(val), &err);
    //     if (err) {
    //         printf("[ERROR] rocksdb_put_cf() failed: %s\n", err);
    //         free(err);
    //     } else {
    //         printf("  Written to CF[%zu]: %s -> %s\n", i, key, val);
    //     }
    // }
    
    // rocksdb_writeoptions_destroy(write_opts);
    
    // printf("\n[Step 3] Closing DB...\n");
    
    if (test_mode == 2) {
        printf("  [CLEANUP] Destroying CF handles first...\n");
        for (size_t i = 0; i < num_cf; i++) {
            printf("    Destroying CF handle[%zu]...\n", i);
            rocksdb_column_family_handle_destroy(cf_handles[i]);
            cf_handles[i] = NULL;
        }
        printf("  [CLEANUP] All CF handles destroyed\n");
    } else {
        printf("  [WARNING] Skipping CF handle destruction - may trigger assertion!\n");
    }
    
    printf("  Calling rocksdb_close()...\n");
    rocksdb_close(db);
    printf("[SUCCESS] DB closed successfully\n");
    
    printf("\n[Step 4] Cleanup...\n");
    if (cf_handles)
        free(cf_handles);
    
    for (size_t i = 0; i < num_cf; i++) {
        rocksdb_options_destroy(cf_options[i]);
    }
    rocksdb_options_destroy(options);
    
    printf("[SUCCESS] All resources freed\n");
    printf("\n=== Test completed successfully! ===\n");
    
    return 0;
}

