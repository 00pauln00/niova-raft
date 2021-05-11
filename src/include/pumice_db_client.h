/* Copyright (C) NIOVA Systems, Inc - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Paul Nowoczynski <pauln@niova.io> 2020
 */

#ifndef __NIOVA_PUMICE_DB_CLIENT_H_
#define __NIOVA_PUMICE_DB_CLIENT_H_ 1

#include "pumice_db_net.h"

int
PmdbObjGetX(pmdb_t pmdb, const pmdb_obj_id_t *obj_id, const char *key,
            size_t key_size,
            pmdb_request_opts_t *pmdb_req);

int
PmdbObjLookup(pmdb_t pmdb, const pmdb_obj_id_t *obj_id,
              pmdb_obj_stat_t *ret_stat);

int
PmdbObjPut(pmdb_t pmdb, const pmdb_obj_id_t *obj_id, const char *kv,
           size_t kv_size, struct pmdb_obj_stat *user_pmdb_stat);

void *
PmdbObjGet(pmdb_t pmdb, const pmdb_obj_id_t *obj_id, const char *key,
           size_t key_size, size_t *value_size);

int
PmdbObjLookupX(pmdb_t pmdb, const pmdb_obj_id_t *obj_id,
               pmdb_request_opts_t *pmdb_req);

int
PmdbObjPutX(pmdb_t pmdb, const pmdb_obj_id_t *obj_id, const char *kv,
            size_t kv_size, pmdb_request_opts_t *pmdb_req);

#if 0
void *
PmdbObjGetNB(pmdb_t pmdb, const pmdb_obj_id_t *obj_id, const char *key,
             size_t key_size, char *value, size_t value_size,
             bool expand_reply_buff,
             pmdb_user_cb_t user_cb, void *user_arg);

void *
PmdbObjGetXNB(pmdb_t pmdb, const pmdb_obj_id_t *obj_id, const char *key,
              size_t key_size, char *value, size_t value_size,
              bool expand_reply_buff,
              pmdb_user_cb_t user_cb, void *user_arg,
              struct pmdb_obj_stat *user_pmdb_stat);
#endif
pmdb_t
PmdbClientStart(const char *raft_uuid_str, const char *raft_client_uuid_str);

char *
PmdbGetLeaderUUID(pmdb_t pmdb);

int
PmdbClientDestroy(pmdb_t pmdb);

#endif
