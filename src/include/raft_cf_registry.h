/* Copyright (C) NIOVA Systems, Inc - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Paul Nowoczynski <pauln@niova.io> 2020
 */

#ifndef RAFT_CF_REGISTRY_H
#define RAFT_CF_REGISTRY_H 1

#include <rocksdb/c.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "ctor.h"

#define RAFT_ROCKSDB_MAX_CF 32
#define RAFT_ROCKSDB_MAX_CF_NAME_LEN 4096

typedef enum {
    CF_LAYER_RAFT = 0,
    CF_LAYER_APPLICATION,
    CF_LAYER_MAX
} cf_layer_t;

typedef struct cf_schema {
    const char *name;
    const char *namespace_prefix;
    cf_layer_t layer;
    rocksdb_options_t *(*config_fn)(void);
    uint32_t version;
    bool read_only_to_upper_layers;
    const char *description;
} cf_schema_t;

/**
 * Per-layer registry structure
 * Stores all CF schemas for a single layer
 */
struct cf_layer_registry {
    cf_schema_t schemas[RAFT_ROCKSDB_MAX_CF];
    size_t num_schemas;
    /* Note: No mutex needed - registrations happen before main() via constructors */
};

/**
 * Registry API Functions
 */

/**
 * cf_registry_add_schema - Add a CF schema to the registry
 * @layer: Layer to register the CF in
 * @schema: Schema definition (will be copied)
 * Returns: 0 on success, negative error code on failure
 */
int
cf_registry_add_schema(cf_layer_t layer, const cf_schema_t *schema);

/**
 * cf_registry_lookup - Look up a CF schema by name and layer
 * @layer: Layer to search in
 * @name: CF name to look up
 * Returns: Pointer to schema if found, NULL otherwise
 */
const cf_schema_t *
cf_registry_lookup(cf_layer_t layer, const char *name);

/**
 * cf_registry_count - Get the number of CFs registered in a layer
 * @layer: Layer to query
 * Returns: Number of registered CFs
 */
size_t
cf_registry_count(cf_layer_t layer);

/**
 * cf_registry_get - Get a CF schema by index
 * @layer: Layer to query
 * @index: Index (0-based)
 * Returns: Pointer to schema if index is valid, NULL otherwise
 */
const cf_schema_t *
cf_registry_get(cf_layer_t layer, size_t index);

/**
 * cf_registry_override_config - Override the config function for an existing CF
 * @layer: Layer containing the CF
 * @name: CF name to override
 * @new_config_fn: New config function to use
 * Returns: 0 on success, negative error code on failure
 */
int
cf_registry_override_config(cf_layer_t layer, const char *name,
                            rocksdb_options_t *(*new_config_fn)(void));

/**
 * Registration macros for compile-time registration
 * These use constructor attributes to auto-register CFs at program startup
 */

/**
 * REGISTER_RAFT_CF - Register a Raft layer CF at compile time
 * @name: CF name (as string literal, e.g., "default")
 * @cfg_fn: Function that returns RocksDB options for this CF
 * @desc: Human-readable description
 *
 * Example:
 *   REGISTER_RAFT_CF("default", raft_default_cf_config, "Raft log entries");
 */
#define REGISTER_RAFT_CF(name, cfg_fn, desc) \
    __attribute__((constructor(RAFT_SYS_CTOR_PRIORITY + 1))) \
    static void register_raft_cf_impl_##cfg_fn(void) { \
        cf_registry_add_schema(CF_LAYER_RAFT, &(cf_schema_t){ \
            .name = name, \
            .namespace_prefix = "raft:", \
            .layer = CF_LAYER_RAFT, \
            .config_fn = cfg_fn, \
            .read_only_to_upper_layers = true, \
            .version = 1, \
            .description = desc \
        }); \
    }

#endif /* RAFT_CF_REGISTRY_H */
