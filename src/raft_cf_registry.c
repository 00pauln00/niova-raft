/* Copyright (C) NIOVA Systems, Inc - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Paul Nowoczynski <pauln@niova.io> 2020
 */

#include "raft_cf_registry.h"
#include "common.h"
#include "log.h"
#include <string.h>
#include <errno.h>

/* Per-layer registries - initialized to zero */
static struct cf_layer_registry cf_registries[CF_LAYER_MAX] = {0};

int
cf_registry_add_schema(cf_layer_t layer, const cf_schema_t *schema)
{
    if (layer >= CF_LAYER_MAX || !schema || !schema->name)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "cf_registry_add_schema(): invalid arguments");
        return -EINVAL;
    }

    struct cf_layer_registry *reg = &cf_registries[layer];

    /* Check for duplicates */
    for (size_t i = 0; i < reg->num_schemas; i++)
    {
        if (strcmp(reg->schemas[i].name, schema->name) == 0)
        {
            SIMPLE_LOG_MSG(LL_WARN,
                          "CF_REGISTRY: CF '%s' already registered in layer %d",
                          schema->name, layer);
            return -EEXIST;
        }
    }

    /* Check capacity */
    if (reg->num_schemas >= RAFT_ROCKSDB_MAX_CF)
    {
        SIMPLE_LOG_MSG(LL_ERROR,
                      "CF_REGISTRY: CF registry for layer %d is full (max %d CFs)",
                      layer, RAFT_ROCKSDB_MAX_CF);
        return -ENOSPC;
    }

    /* Validate name length */
    size_t name_len = strlen(schema->name);
    if (name_len == 0 || name_len > RAFT_ROCKSDB_MAX_CF_NAME_LEN)
    {
        SIMPLE_LOG_MSG(LL_ERROR,
                      "CF_REGISTRY: CF name '%s' has invalid length (%zu, max %d)",
                      schema->name, name_len, RAFT_ROCKSDB_MAX_CF_NAME_LEN);
        return -EINVAL;
    }

    /* Add schema (shallow copy - strings are expected to be static/constant) */
    reg->schemas[reg->num_schemas] = *schema;
    reg->num_schemas++;

    SIMPLE_LOG_MSG(LL_WARN,
                  "CF_REGISTRY: Registered CF '%s' in layer %d (description: %s)",
                  schema->name, layer,
                  schema->description ? schema->description : "none");

    return 0;
}

const cf_schema_t *
cf_registry_lookup(cf_layer_t layer, const char *name)
{
    if (layer >= CF_LAYER_MAX || !name)
        return NULL;

    struct cf_layer_registry *reg = &cf_registries[layer];

    for (size_t i = 0; i < reg->num_schemas; i++)
    {
        if (strcmp(reg->schemas[i].name, name) == 0)
            return &reg->schemas[i];
    }

    return NULL;
}

size_t
cf_registry_count(cf_layer_t layer)
{
    if (layer >= CF_LAYER_MAX)
        return 0;

    return cf_registries[layer].num_schemas;
}

const cf_schema_t *
cf_registry_get(cf_layer_t layer, size_t index)
{
    if (layer >= CF_LAYER_MAX)
        return NULL;

    struct cf_layer_registry *reg = &cf_registries[layer];

    if (index >= reg->num_schemas)
        return NULL;

    return &reg->schemas[index];
}

int
cf_registry_override_config(cf_layer_t layer, const char *name,
                            rocksdb_options_t *(*new_config_fn)(void))
{
    if (layer >= CF_LAYER_MAX || !name || !new_config_fn)
    {
        SIMPLE_LOG_MSG(LL_ERROR, "cf_registry_override_config(): invalid arguments");
        return -EINVAL;
    }

    struct cf_layer_registry *reg = &cf_registries[layer];

    /* First, try to find and override existing registration */
    for (size_t i = 0; i < reg->num_schemas; i++)
    {
        if (strcmp(reg->schemas[i].name, name) == 0)
        {
            /* Override the config function */
            reg->schemas[i].config_fn = new_config_fn;
            SIMPLE_LOG_MSG(LL_WARN,
                          "CF_REGISTRY: SUCCESS - Overrode config function for CF '%s' in layer %d",
                          name, layer);
            return 0;
        }
    }

    /* CF not found - for internal CFs (like "default" in RAFT layer),
     * auto-register it with the override config function.
     * This allows applications to override internal CFs without them
     * being pre-registered.
     */
    if (layer == CF_LAYER_RAFT && strcmp(name, "default") == 0)
    {
        SIMPLE_LOG_MSG(LL_WARN,
                      "CF_REGISTRY: CF 'default' not registered, auto-registering with override config");
        
        int rc = cf_registry_add_schema(layer, &(cf_schema_t){
            .name = name,
            .namespace_prefix = "raft:",
            .layer = layer,
            .config_fn = new_config_fn,
            .read_only_to_upper_layers = true,
            .version = 1,
            .description = "Raft log entries and metadata (overridden)"
        });
        
        if (rc)
        {
            SIMPLE_LOG_MSG(LL_ERROR,
                          "CF_REGISTRY: Failed to auto-register CF '%s' for override: %s",
                          name, strerror(-rc));
            return rc;
        }
        
        SIMPLE_LOG_MSG(LL_WARN,
                      "CF_REGISTRY: SUCCESS - Auto-registered and overrode CF '%s' in layer %d",
                      name, layer);
        return 0;
    }

    SIMPLE_LOG_MSG(LL_ERROR,
                  "CF_REGISTRY: CF '%s' not found in layer %d for override (and not auto-registerable)",
                  name, layer);
    return -ENOENT;
}
