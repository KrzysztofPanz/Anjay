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

#include <anjay_init.h>

#include <inttypes.h>

#include <avsystem/commons/avs_utils.h>

#define ANJAY_SERVERS_CONNECTION_SOURCE
#define ANJAY_SERVERS_INTERNALS

#include "anjay_connections_internal.h"
#include "anjay_security.h"

VISIBILITY_SOURCE_BEGIN

int _anjay_connection_security_generic_get_uri(
        anjay_t *anjay,
        anjay_iid_t security_iid,
        avs_url_t **out_uri,
        const anjay_transport_info_t **out_transport_info) {
    assert(out_uri);
    assert(!*out_uri);
    assert(out_transport_info);
    assert(!*out_transport_info);
    char raw_uri[ANJAY_MAX_URL_RAW_LENGTH];

    const anjay_uri_path_t path =
            MAKE_RESOURCE_PATH(ANJAY_DM_OID_SECURITY, security_iid,
                               ANJAY_DM_RID_SECURITY_SERVER_URI);

    if (_anjay_dm_read_resource_string(anjay, &path, raw_uri,
                                       sizeof(raw_uri))) {
        anjay_log(ERROR, _("could not read LwM2M server URI from ") "%s",
                  ANJAY_DEBUG_MAKE_PATH(&path));
        return -1;
    }

    if (!(*out_uri = avs_url_parse_lenient(raw_uri))
            || !(*out_transport_info = _anjay_transport_info_by_uri_scheme(
                         avs_url_protocol(*out_uri)))
            || avs_url_user(*out_uri) || avs_url_password(*out_uri)
            || (avs_url_port(*out_uri) && !*avs_url_port(*out_uri))) {
        if (*out_uri) {
            avs_url_free(*out_uri);
        }
        *out_uri = NULL;
        *out_transport_info = NULL;
        anjay_log(ERROR, _("could not parse LwM2M server URI: ") "%s", raw_uri);
        return -1;
    }
    return 0;
}

static int get_security_mode(anjay_t *anjay,
                             anjay_iid_t security_iid,
                             anjay_security_mode_t *out_mode) {
    int64_t mode;
    const anjay_uri_path_t path =
            MAKE_RESOURCE_PATH(ANJAY_DM_OID_SECURITY, security_iid,
                               ANJAY_DM_RID_SECURITY_MODE);

    if (_anjay_dm_read_resource_i64(anjay, &path, &mode)) {
        anjay_log(ERROR,
                  _("could not read LwM2M server security mode from ") "%s",
                  ANJAY_DEBUG_MAKE_PATH(&path));
        return -1;
    }

    switch (mode) {
    case ANJAY_SECURITY_RPK:
        anjay_log(ERROR, _("unsupported security mode: ") "%s",
                  AVS_INT64_AS_STRING(mode));
        return -1;
    case ANJAY_SECURITY_NOSEC:
    case ANJAY_SECURITY_PSK:
    case ANJAY_SECURITY_CERTIFICATE:
    case ANJAY_SECURITY_EST:
        *out_mode = (anjay_security_mode_t) mode;
        return 0;
    default:
        anjay_log(ERROR, _("invalid security mode: ") "%s",
                  AVS_INT64_AS_STRING(mode));
        return -1;
    }
}

static bool
security_matches_transport(anjay_security_mode_t security_mode,
                           const anjay_transport_info_t *transport_info) {
    assert(transport_info);
    if (transport_info->security == ANJAY_TRANSPORT_SECURITY_UNDEFINED) {
        // URI scheme does not specify security,
        // so it is valid for all security modes
        return true;
    }

    const bool is_secure_transport =
            (transport_info->security == ANJAY_TRANSPORT_ENCRYPTED);
    const bool needs_secure_transport = (security_mode != ANJAY_SECURITY_NOSEC);

    if (is_secure_transport != needs_secure_transport) {
        anjay_log(WARNING,
                  _("security mode ") "%d" _(" requires ") "%s" _(
                          "secure protocol, but '") "%s" _("' was configured"),
                  (int) security_mode, needs_secure_transport ? "" : "in",
                  transport_info->uri_scheme);
        return false;
    }

    return true;
}

static int get_dtls_keys(anjay_t *anjay,
                         anjay_iid_t security_iid,
                         anjay_security_mode_t security_mode,
                         anjay_server_dtls_keys_t *out_keys) {
    if (security_mode == ANJAY_SECURITY_NOSEC) {
        return 0;
    }

    typedef enum { SKIP, OPTIONAL, REQUIRED } value_status_t;

    typedef struct {
        value_status_t status;
        anjay_rid_t rid;
        char *buffer;
        size_t buffer_capacity;
        size_t *buffer_size_ptr;
    } value_def_t;

    value_def_t PK_OR_IDENTITY_VALUE = {
        .status = REQUIRED,
        .rid = ANJAY_DM_RID_SECURITY_PK_OR_IDENTITY,
        .buffer = out_keys->pk_or_identity,
        .buffer_capacity = sizeof(out_keys->pk_or_identity),
        .buffer_size_ptr = &out_keys->pk_or_identity_size
    };
    value_def_t SERVER_PK_OR_IDENTITY_VALUE = {
        .status = (security_mode != ANJAY_SECURITY_PSK) ? REQUIRED : OPTIONAL,
        .rid = ANJAY_DM_RID_SECURITY_SERVER_PK_OR_IDENTITY,
        .buffer = out_keys->server_pk_or_identity,
        .buffer_capacity = sizeof(out_keys->server_pk_or_identity),
        .buffer_size_ptr = &out_keys->server_pk_or_identity_size
    };
    value_def_t SECRET_KEY_VALUE = {
        .status = REQUIRED,
        .rid = ANJAY_DM_RID_SECURITY_SECRET_KEY,
        .buffer = out_keys->secret_key,
        .buffer_capacity = sizeof(out_keys->secret_key),
        .buffer_size_ptr = &out_keys->secret_key_size
    };

    const value_def_t *const values[] = { &PK_OR_IDENTITY_VALUE,
                                          &SERVER_PK_OR_IDENTITY_VALUE,
                                          &SECRET_KEY_VALUE };

    for (size_t i = 0; i < AVS_ARRAY_SIZE(values); ++i) {
        if (values[i]->status == SKIP) {
            continue;
        }
        const anjay_uri_path_t path =
                MAKE_RESOURCE_PATH(ANJAY_DM_OID_SECURITY, security_iid,
                                   values[i]->rid);
        if (_anjay_dm_read_resource(anjay, &path, values[i]->buffer,
                                    values[i]->buffer_capacity,
                                    values[i]->buffer_size_ptr)
                && values[i]->status == REQUIRED) {
            anjay_log(WARNING, _("read ") "%s" _(" failed"),
                      ANJAY_DEBUG_MAKE_PATH(&path));
            return -1;
        }
    }

    return 0;
}

static int
init_cert_security(anjay_t *anjay,
                   anjay_ssid_t ssid,
                   anjay_security_config_t *security,
                   avs_net_socket_dane_tlsa_record_t *dane_tlsa_record,
                   anjay_security_mode_t security_mode,
                   const anjay_server_dtls_keys_t *keys) {
    (void) anjay;
    avs_crypto_client_cert_info_t client_cert =
            avs_crypto_client_cert_info_from_buffer(keys->pk_or_identity,
                                                    keys->pk_or_identity_size);

    avs_crypto_client_key_info_t private_key =
            avs_crypto_client_key_info_from_buffer(keys->secret_key,
                                                   keys->secret_key_size, NULL);

    avs_net_certificate_info_t certificate_info = {
        .ignore_system_trust_store = true,
        .client_cert = client_cert,
        .client_key = private_key
    };

    if (keys->server_pk_or_identity_size > 0) {
        certificate_info.server_cert_validation = true;
        certificate_info.dane = true;

        security->dane_tlsa_record = dane_tlsa_record;
        dane_tlsa_record->association_data = keys->server_pk_or_identity;
        dane_tlsa_record->association_data_size =
                keys->server_pk_or_identity_size;
    }

    (void) ssid;
    (void) security_mode;

    security->security_info =
            avs_net_security_info_from_certificates(certificate_info);

    return 0;
}

static int init_security(anjay_t *anjay,
                         anjay_ssid_t ssid,
                         anjay_security_config_t *security,
                         avs_net_socket_dane_tlsa_record_t *dane_tlsa_record,
                         anjay_security_mode_t security_mode,
                         const anjay_server_dtls_keys_t *keys) {
    switch (security_mode) {
    case ANJAY_SECURITY_NOSEC:
        return 0;
    case ANJAY_SECURITY_PSK:
        return _anjay_connection_init_psk_security(&security->security_info,
                                                   keys);
        break;
    case ANJAY_SECURITY_CERTIFICATE:
    case ANJAY_SECURITY_EST:
        return init_cert_security(anjay, ssid, security, dane_tlsa_record,
                                  security_mode, keys);
    case ANJAY_SECURITY_RPK:
    default:
        anjay_log(ERROR, _("unsupported security mode: ") "%d",
                  (int) security_mode);
        return -1;
    }
}

typedef struct {
    anjay_security_config_t security_config;
    anjay_server_dtls_keys_t dtls_keys;
    avs_net_socket_dane_tlsa_record_t dane_tlsa_record;
} security_config_with_data_t;

anjay_security_config_t *_anjay_connection_security_generic_get_config(
        anjay_t *anjay, anjay_connection_info_t *inout_info) {
    security_config_with_data_t *result = NULL;
    size_t security_config_size = sizeof(security_config_with_data_t);

    if ((result = (security_config_with_data_t *) avs_calloc(
                 1, security_config_size))) {
        result->security_config.tls_ciphersuites =
                anjay->default_tls_ciphersuites;
        result->dane_tlsa_record.certificate_usage =
                AVS_NET_SOCKET_DANE_DOMAIN_ISSUED_CERTIFICATE;
    }

    anjay_security_mode_t security_mode;
    if (!result
            || get_security_mode(anjay, inout_info->security_iid,
                                 &security_mode)
            || (inout_info->transport_info
                && !security_matches_transport(security_mode,
                                               inout_info->transport_info))
            || get_dtls_keys(anjay, inout_info->security_iid, security_mode,
                             &result->dtls_keys)
            || init_security(anjay, inout_info->ssid, &result->security_config,
                             &result->dane_tlsa_record, security_mode,
                             &result->dtls_keys)) {
        goto error;
    }
    inout_info->is_encrypted = (security_mode != ANJAY_SECURITY_NOSEC);
    anjay_log(DEBUG, _("server ") "/%u/%u" _(": security mode = ") "%d",
              ANJAY_DM_OID_SECURITY, inout_info->security_iid,
              (int) security_mode);
    AVS_STATIC_ASSERT(offsetof(security_config_with_data_t, security_config)
                              == 0,
                      security_config_pointers_castable);
    return &result->security_config;
error:
    avs_free(result);
    return NULL;
}
