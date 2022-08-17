/**
 * Generated by anjay_codegen.py on 2020-03-19 14:13:34
 *
 * LwM2M Object: Time
 * ID: 3333, URN: urn:oma:lwm2m:ext:3333, Optional, Multiple
 *
 * This IPSO object is used to report the current time in seconds since
 * January 1, 1970 UTC. There is also a fractional time counter that has
 * a range of less than one second.
 */

#include <assert.h>
#include <stdbool.h>

#include <anjay/anjay.h>
#include <anjay/lwm2m_send.h>
#include <avsystem/commons/avs_defs.h>
#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_log.h>
#include <avsystem/commons/avs_memory.h>

#include "time_object.h"

/**
 * Current Time: RW, Single, Mandatory
 * type: time, range: N/A, unit: N/A
 * Unix Time. A signed integer representing the number of seconds since
 * Jan 1st, 1970 in the UTC time zone.
 */
#define RID_CURRENT_TIME 5506

/**
 * Fractional Time: RW, Single, Optional
 * type: float, range: 0..1, unit: s
 * Fractional part of the time when sub-second precision is used (e.g.,
 * 0.23 for 230 ms).
 */
#define RID_FRACTIONAL_TIME 5507

/**
 * Application Type: RW, Single, Optional
 * type: string, range: N/A, unit: N/A
 * The application type of the sensor or actuator as a string depending
 * on the use case.
 */
#define RID_APPLICATION_TYPE 5750

typedef struct time_instance_struct {
    anjay_iid_t iid;
    char application_type[64];
    char application_type_backup[64];
    int64_t last_notify_timestamp;
} time_instance_t;

typedef struct time_object_struct {
    const anjay_dm_object_def_t *def;
    AVS_LIST(time_instance_t) instances;
} time_object_t;

static inline time_object_t *
get_obj(const anjay_dm_object_def_t *const *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(obj_ptr, time_object_t, def);
}

static time_instance_t *find_instance(const time_object_t *obj,
                                      anjay_iid_t iid) {
    AVS_LIST(time_instance_t) it;
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

    AVS_LIST(time_instance_t) it;
    AVS_LIST_FOREACH(it, get_obj(obj_ptr)->instances) {
        anjay_dm_emit(ctx, it->iid);
    }

    return 0;
}

static int init_instance(time_instance_t *inst, anjay_iid_t iid) {
    assert(iid != ANJAY_ID_INVALID);

    inst->iid = iid;
    inst->application_type[0] = '\0';

    return 0;
}

static void release_instance(time_instance_t *inst) {
    (void) inst;
}

static time_instance_t *add_instance(time_object_t *obj, anjay_iid_t iid) {
    assert(find_instance(obj, iid) == NULL);

    AVS_LIST(time_instance_t) created = AVS_LIST_NEW_ELEMENT(time_instance_t);
    if (!created) {
        return NULL;
    }

    int result = init_instance(created, iid);
    if (result) {
        AVS_LIST_CLEAR(&created);
        return NULL;
    }

    AVS_LIST(time_instance_t) *ptr;
    AVS_LIST_FOREACH_PTR(ptr, &obj->instances) {
        if ((*ptr)->iid > created->iid) {
            break;
        }
    }

    AVS_LIST_INSERT(ptr, created);
    return created;
}

static int instance_create(anjay_t *anjay,
                           const anjay_dm_object_def_t *const *obj_ptr,
                           anjay_iid_t iid) {
    (void) anjay;
    time_object_t *obj = get_obj(obj_ptr);
    assert(obj);

    return add_instance(obj, iid) ? 0 : ANJAY_ERR_INTERNAL;
}

static int instance_remove(anjay_t *anjay,
                           const anjay_dm_object_def_t *const *obj_ptr,
                           anjay_iid_t iid) {
    (void) anjay;
    time_object_t *obj = get_obj(obj_ptr);
    assert(obj);

    AVS_LIST(time_instance_t) *it;
    AVS_LIST_FOREACH_PTR(it, &obj->instances) {
        if ((*it)->iid == iid) {
            release_instance(*it);
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

    time_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    time_instance_t *inst = find_instance(obj, iid);
    assert(inst);

    inst->application_type[0] = '\0';

    return 0;
}

static int list_resources(anjay_t *anjay,
                          const anjay_dm_object_def_t *const *obj_ptr,
                          anjay_iid_t iid,
                          anjay_dm_resource_list_ctx_t *ctx) {
    (void) anjay;
    (void) obj_ptr;
    (void) iid;

    anjay_dm_emit_res(ctx, RID_CURRENT_TIME, ANJAY_DM_RES_RW,
                      ANJAY_DM_RES_PRESENT);
    anjay_dm_emit_res(ctx, RID_FRACTIONAL_TIME, ANJAY_DM_RES_RW,
                      ANJAY_DM_RES_ABSENT);
    anjay_dm_emit_res(ctx, RID_APPLICATION_TYPE, ANJAY_DM_RES_RW,
                      ANJAY_DM_RES_PRESENT);
    return 0;
}

static int resource_read(anjay_t *anjay,
                         const anjay_dm_object_def_t *const *obj_ptr,
                         anjay_iid_t iid,
                         anjay_rid_t rid,
                         anjay_riid_t riid,
                         anjay_output_ctx_t *ctx) {
    (void) anjay;

    time_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    time_instance_t *inst = find_instance(obj, iid);
    assert(inst);

    switch (rid) {
    case RID_CURRENT_TIME: {
        assert(riid == ANJAY_ID_INVALID);
        int64_t timestamp;
        if (avs_time_real_to_scalar(&timestamp, AVS_TIME_S,
                                    avs_time_real_now())) {
            return -1;
        }
        return anjay_ret_i64(ctx, timestamp);
    }

    case RID_APPLICATION_TYPE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_ret_string(ctx, inst->application_type);

    default:
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

    time_object_t *obj = get_obj(obj_ptr);
    assert(obj);
    time_instance_t *inst = find_instance(obj, iid);
    assert(inst);

    switch (rid) {
    case RID_APPLICATION_TYPE:
        assert(riid == ANJAY_ID_INVALID);
        return anjay_get_string(ctx, inst->application_type,
                                sizeof(inst->application_type));

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

int transaction_begin(anjay_t *anjay,
                      const anjay_dm_object_def_t *const *obj_ptr) {
    (void) anjay;

    time_object_t *obj = get_obj(obj_ptr);

    time_instance_t *element;
    AVS_LIST_FOREACH(element, obj->instances) {
        strcpy(element->application_type_backup, element->application_type);
    }
    return 0;
}

int transaction_rollback(anjay_t *anjay,
                         const anjay_dm_object_def_t *const *obj_ptr) {
    (void) anjay;

    time_object_t *obj = get_obj(obj_ptr);

    time_instance_t *element;
    AVS_LIST_FOREACH(element, obj->instances) {
        strcpy(element->application_type, element->application_type_backup);
    }
    return 0;
}

static const anjay_dm_object_def_t OBJ_DEF = {
    .oid = 3333,
    .handlers = {
        .list_instances = list_instances,
        .instance_create = instance_create,
        .instance_remove = instance_remove,
        .instance_reset = instance_reset,

        .list_resources = list_resources,
        .resource_read = resource_read,
        .resource_write = resource_write,

        .transaction_begin = transaction_begin,
        .transaction_validate = anjay_dm_transaction_NOOP,
        .transaction_commit = anjay_dm_transaction_NOOP,
        .transaction_rollback = transaction_rollback
    }
};

const anjay_dm_object_def_t **time_object_create(void) {
    time_object_t *obj = (time_object_t *) avs_calloc(1, sizeof(time_object_t));
    if (!obj) {
        return NULL;
    }
    obj->def = &OBJ_DEF;

    time_instance_t *inst = add_instance(obj, 0);
    if (inst) {
        strcpy(inst->application_type, "Clock 0");
    } else {
        avs_free(obj);
        return NULL;
    }

    return &obj->def;
}

void time_object_release(const anjay_dm_object_def_t **def) {
    if (def) {
        time_object_t *obj = get_obj(def);
        AVS_LIST_CLEAR(&obj->instances) {
            release_instance(obj->instances);
        }

        avs_free(obj);
    }
}

void time_object_notify(anjay_t *anjay, const anjay_dm_object_def_t **def) {
    if (!anjay || !def) {
        return;
    }
    time_object_t *obj = get_obj(def);

    int64_t current_timestamp;
    if (avs_time_real_to_scalar(&current_timestamp, AVS_TIME_S,
                                avs_time_real_now())) {
        return;
    }

    AVS_LIST(time_instance_t) it;
    AVS_LIST_FOREACH(it, obj->instances) {
        if (it->last_notify_timestamp != current_timestamp) {
            if (!anjay_notify_changed(anjay, 3333, it->iid, RID_CURRENT_TIME)) {
                it->last_notify_timestamp = current_timestamp;
            }
        }
    }
}

static void send_finished_handler(anjay_t *anjay,
                                  anjay_ssid_t ssid,
                                  const anjay_send_batch_t *batch,
                                  int result,
                                  void *data) {
    (void) anjay;
    (void) ssid;
    (void) batch;
    (void) data;

    if (result != ANJAY_SEND_SUCCESS) {
        avs_log(time_object, ERROR, "Send failed, result: %d", result);
    } else {
        avs_log(time_object, TRACE, "Send successful");
    }
}

void time_object_send(anjay_t *anjay, const anjay_dm_object_def_t **def) {
    if (!anjay || !def) {
        return;
    }
    time_object_t *obj = get_obj(def);
    const anjay_ssid_t server_ssid = 1;

    // Allocate new batch builder.
    anjay_send_batch_builder_t *builder = anjay_send_batch_builder_new();

    if (!builder) {
        avs_log(time_object, ERROR, "Failed to allocate batch builder");
        return;
    }

    int res = 0;

    AVS_LIST(time_instance_t) it;
    AVS_LIST_FOREACH(it, obj->instances) {
        // Add current values of resources from Time Object.
        if (anjay_send_batch_data_add_current(builder, anjay, obj->def->oid,
                                              it->iid, RID_CURRENT_TIME)
                || anjay_send_batch_data_add_current(builder, anjay,
                                                     obj->def->oid, it->iid,
                                                     RID_APPLICATION_TYPE)) {
            anjay_send_batch_builder_cleanup(&builder);
            avs_log(time_object, ERROR, "Failed to add batch data, result: %d",
                    res);
            return;
        }
    }
    // After adding all values, compile our batch for sending.
    anjay_send_batch_t *batch = anjay_send_batch_builder_compile(&builder);

    if (!batch) {
        anjay_send_batch_builder_cleanup(&builder);
        avs_log(time_object, ERROR, "Batch compile failed");
        return;
    }

    // Schedule our send to be run on next `anjay_sched_run()` call.
    res = anjay_send(anjay, server_ssid, batch, send_finished_handler, NULL);

    if (res) {
        avs_log(time_object, ERROR, "Failed to send, result: %d", res);
    }

    // After scheduling, we can release our batch.
    anjay_send_batch_release(&batch);
}