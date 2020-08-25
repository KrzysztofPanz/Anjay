/*
 * Copyright 2017-2020 AVSystem <avsystem@avsystem.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 *
 * LwM2M Object: Portfolio
 * ID: 16, URN: urn:oma:lwm2m:oma:16, Optional, Multiple
 *
 * The Portfolio Object allows to extend the data storage capability of
 * other Object Instances in the LWM2M system, as well as the services which
 * may be used to authenticate and to protect privacy of data contained in
 * those extensions. In addition, a service of data encryption is also defined
 */
#include "../objects.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <anjay/anjay.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>

/**
 * Identity: RW, Multiple, Mandatory
 * type: string, range: N/A, unit: N/A
 * Data Storage extension for other Object Instances.
 *  e.g  for [GSMA]  :
 * 0 : Host Device ID,
 * 1:  Host Device Manufacturer
 * 2:  Host Device Model
 * 3:  Host Device Software Version,
 *
 * This Resource contains data that the GetAuthData executable Resource can
 * work with.
 */
#define RID_IDENTITY 0

/**
 * GetAuthData: E, Single, Optional
 * type: N/A, range: N/A, unit: N/A
 * Executable resource to trigger Services described in the portfolio object
 * specification, Section 5.2.2. Arguments definitions are described in
 * Section 5.2.1 as well as in table 2.
 */
#define RID_GETAUTHDATA 1

/**
 * AuthData: R, Multiple, Optional
 * type: string, range: N/A, unit: N/A
 * Buffer which contains the data generated by the  process triggered by a
 * GetAuthData request
 */
#define RID_AUTHDATA 2

/**
 * AuthStatus: R, Single, Optional
 * type: integer, range: [0-2], unit: N/A
 * This Resource contains the state related to the process triggered by
 * GetAuthData request.
 * 0 :  IDLE_STATE :  AuthData doesn’t contain any valid data
 * 1 :  DATA_AVAIL_STATE : AuthData  contains a valid data
 * 2 :  ERROR_STATE :  an error occurred
 * This state is reset to IDLE_STATE, when the executable resource
 * "GetAuthData" is triggered or when the AuthData resource has been
 * returned to the LWM2M Server (READ / NOTIFY).
 */
#define RID_AUTHSTATUS 3

typedef enum {
    HOST_DEVICE_ID = 0,
    HOST_DEVICE_MANUFACTURER = 1,
    HOST_DEVICE_MODEL = 2,
    HOST_DEVICE_SOFTWARE_VERSION = 3,
    _MAX_IDENTITY_TYPE,
} portfolio_identity_type_t;

#define MAX_IDENTITY_VALUE_SIZE 256

typedef struct portfolio_instance_struct {
    anjay_iid_t iid;

    bool has_identity[_MAX_IDENTITY_TYPE];
    char identity_value[_MAX_IDENTITY_TYPE][MAX_IDENTITY_VALUE_SIZE];
} portfolio_instance_t;

typedef struct portfolio_struct {
    const anjay_dm_object_def_t *def;
    AVS_LIST(portfolio_instance_t) instances;
    AVS_LIST(portfolio_instance_t) backup;
} portfolio_t;

static inline portfolio_t *
get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(obj_ptr, portfolio_t, def);
}

static portfolio_instance_t *find_instance(const portfolio_t *obj,
                                           anjay_iid_t iid) {
    AVS_LIST(portfolio_instance_t) it;
    AVS_LIST_FOREACH(it, obj->instances) {
        if (it->iid == iid) {
            return it;
        } else if (it->iid > iid) {
            break;
        }
    }

    return NULL;
}

static int list_instances(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_dm_list_ctx_t *ctx) {
    (void) anjay;

    AVS_LIST(portfolio_instance_t) it;
    AVS_LIST_FOREACH(it, get_obj(obj_ptr)->instances) {
        anjay_dm_emit(ctx, it->iid);
    }
    return 0;
}

static int instance_create(anjay_t *anjay,
                           const anjay_dm_object_def_t *const *obj_ptr,
                           anjay_iid_t iid) {
    (void) anjay;
    portfolio_t *obj = get_obj(obj_ptr);
    assert(obj);

    AVS_LIST(portfolio_instance_t) created =
            AVS_LIST_NEW_ELEMENT(portfolio_instance_t);
    if (!created) {
        return ANJAY_ERR_INTERNAL;
    }

    created->iid = iid;

    AVS_LIST(portfolio_instance_t) *ptr;
    AVS_LIST_FOREACH_PTR(ptr, &obj->instances) {
        if ((*ptr)->iid > created->iid) {
            break;
        }
    }

    AVS_LIST_INSERT(ptr, created);
    return 0;
}

static int instance_remove(anjay_t *anjay,
                           const anjay_dm_object_def_t *const *obj_ptr,
                           anjay_iid_t iid) {
    (void) anjay;
    portfolio_t *obj = get_obj(obj_ptr);
    assert(obj);

    AVS_LIST(portfolio_instance_t) *it;
    AVS_LIST_FOREACH_PTR(it, &obj->instances) {
        if ((*it)->iid == iid) {
            AVS_LIST_DELETE(it);
            return 0;
        } else if ((*it)->iid > iid) {
            break;
        }
    }

    assert(0);
    return ANJAY_ERR_NOT_FOUND;
}

static int instance_reset(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid) {
    (void) anjay;

    portfolio_instance_t *inst = find_instance(get_obj(obj_ptr), iid);
    assert(inst);

    memset(inst->identity_value, 0, sizeof(inst->identity_value));
    memset(inst->has_identity, 0, sizeof(inst->has_identity));
    return 0;
}

static int list_resources(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid,
                          anjay_dm_resource_list_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;
    anjay_dm_emit_res(
            ctx, RID_IDENTITY, ANJAY_DM_RES_RWM, ANJAY_DM_RES_PRESENT);
    return 0;
}

static int resource_read(anjay_t *anjay,
                         const anjay_dm_object_def_t *const *obj_ptr,
                         anjay_iid_t iid,
                         anjay_rid_t rid,
                         anjay_riid_t riid,
                         anjay_output_ctx_t *ctx) {
    (void) anjay;

    portfolio_t *obj = get_obj(obj_ptr);
    assert(obj);
    portfolio_instance_t *inst = find_instance(obj, iid);
    assert(inst);

    switch (rid) {
    case RID_IDENTITY:
        assert(riid < _MAX_IDENTITY_TYPE);
        assert(inst->has_identity[riid]);
        return anjay_ret_string(ctx, inst->identity_value[riid]);
    default:
        AVS_UNREACHABLE("Read called on unknown resource");
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static int resource_write(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid,
                          anjay_rid_t rid,
                          anjay_riid_t riid,
                          anjay_input_ctx_t *ctx) {
    (void) anjay;

    portfolio_t *obj = get_obj(obj_ptr);
    assert(obj);
    portfolio_instance_t *inst = find_instance(obj, iid);
    assert(inst);

    switch (rid) {
    case RID_IDENTITY: {
        if (riid >= _MAX_IDENTITY_TYPE) {
            return ANJAY_ERR_NOT_FOUND;
        }
        char value[MAX_IDENTITY_VALUE_SIZE];
        int result = anjay_get_string(ctx, value, sizeof(value));
        if (!result) {
            inst->has_identity[riid] = true;
            strcpy(inst->identity_value[riid], value);
        }
        return result;
    }

    default:
        AVS_UNREACHABLE("Write called on unknown resource");
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static int resource_reset(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid,
                          anjay_rid_t rid) {
    (void) anjay;
    (void) rid;

    portfolio_t *obj = get_obj(obj_ptr);
    assert(obj);
    portfolio_instance_t *inst = find_instance(obj, iid);
    assert(inst);

    assert(rid == RID_IDENTITY);
    memset(inst->has_identity, 0, sizeof(inst->has_identity));
    return 0;
}

static int list_resource_instances(anjay_t *anjay,
                                   const anjay_dm_object_def_t *const *obj_ptr,
                                   anjay_iid_t iid,
                                   anjay_rid_t rid,
                                   anjay_dm_list_ctx_t *ctx) {
    (void) anjay;

    portfolio_t *obj = get_obj(obj_ptr);
    assert(obj);
    portfolio_instance_t *inst = find_instance(obj, iid);
    assert(inst);

    switch (rid) {
    case RID_IDENTITY: {
        for (anjay_riid_t i = 0; i < _MAX_IDENTITY_TYPE; ++i) {
            if (inst->has_identity[i]) {
                anjay_dm_emit(ctx, i);
            }
        }
        return 0;
    }
    default:
        AVS_UNREACHABLE(
                "Attempted to list instances in a single-instance resource");
        return ANJAY_ERR_INTERNAL;
    }
}

static int transaction_begin(anjay_t *anjay,
                             const anjay_dm_object_def_t *const *obj_ptr) {
    (void) anjay;
    portfolio_t *obj = get_obj(obj_ptr);
    assert(obj);
    assert(!obj->backup);
    obj->backup = AVS_LIST_SIMPLE_CLONE(obj->instances);
    if (!obj->backup && obj->instances) {
        return ANJAY_ERR_INTERNAL;
    }
    return 0;
}

static int transaction_commit(anjay_t *anjay,
                              const anjay_dm_object_def_t *const *obj_ptr) {
    (void) anjay;
    portfolio_t *obj = get_obj(obj_ptr);
    assert(obj);
    AVS_LIST_CLEAR(&obj->backup);
    return 0;
}

static int transaction_rollback(anjay_t *anjay,
                                const anjay_dm_object_def_t *const *obj_ptr) {
    (void) anjay;
    portfolio_t *obj = get_obj(obj_ptr);
    assert(obj);
    AVS_LIST_CLEAR(&obj->instances);
    obj->instances = obj->backup;
    obj->backup = NULL;
    return 0;
}

static const anjay_dm_object_def_t OBJ_DEF = {
    .oid = 16,
    .handlers = {
        .list_instances = list_instances,
        .instance_create = instance_create,
        .instance_remove = instance_remove,
        .instance_reset = instance_reset,

        .list_resources = list_resources,
        .resource_read = resource_read,
        .resource_write = resource_write,
        .resource_reset = resource_reset,
        .list_resource_instances = list_resource_instances,

        .transaction_begin = transaction_begin,
        .transaction_validate = anjay_dm_transaction_NOOP,
        .transaction_commit = transaction_commit,
        .transaction_rollback = transaction_rollback
    }
};

const anjay_dm_object_def_t **portfolio_object_create(void) {
    portfolio_t *obj = (portfolio_t *) avs_calloc(1, sizeof(portfolio_t));
    if (!obj) {
        return NULL;
    }
    obj->def = &OBJ_DEF;
    return &obj->def;
}

void portfolio_object_release(const anjay_dm_object_def_t **def) {
    if (def) {
        portfolio_t *obj = get_obj(def);
        AVS_LIST_CLEAR(&obj->instances);
        avs_free(obj);
    }
}

int portfolio_get_instances(const anjay_dm_object_def_t **def,
                            AVS_LIST(anjay_iid_t) *out) {
    portfolio_t *obj = get_obj(def);
    assert(!*out);
    AVS_LIST(portfolio_instance_t) it;
    AVS_LIST_FOREACH(it, obj->instances) {
        if (!(*out = AVS_LIST_NEW_ELEMENT(anjay_iid_t))) {
            demo_log(ERROR, "out of memory");
            return -1;
        }
        **out = it->iid;
        AVS_LIST_ADVANCE_PTR(&out);
    }
    return 0;
}
