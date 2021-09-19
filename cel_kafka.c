/*! \file
 *
 * \brief Kafka CEL backend
 *
 * \author Max Nesterov <braams@braams.ru>
 *
 */

#define AST_MODULE_SELF_SYM __internal_cel_kafka_self
#define AST_MODULE "app_kafka"

#include <asterisk.h>
#include <stdio.h>

#include <asterisk/module.h>
#include <asterisk/cel.h>
#include <asterisk/config.h>
#include <asterisk/json.h>
#include <asterisk/channel.h>
#include "res_kafka.h"
#include <sys/time.h>

#define DESCRIPTION         "Kafka CEL Backend"
#define DEFAULT_KAFKA_TOPIC "asterisk_cel"
#define DEFAULT_DATEFORMAT    "%F %T"

static char conf_file[] = "cel_kafka.conf";
static char name[] = "cel_kafka";
static int enablecel;

static char *kafka_topic;
static char *dateformat;
static char *zone;

struct ast_json *json_timeformat(const struct timeval tv, const char *zone, const char *format) {
    char buf[AST_ISO8601_LEN];
    struct ast_tm tm = {};
    ast_localtime(&tv, &tm, zone);
    ast_strftime(buf, sizeof(buf), format, &tm);
    return ast_json_string_create(buf);
}

static struct ast_json *obj_as_is(struct ast_cel_event_record record) {

//    uint32_t version;
//    enum ast_cel_event_type event_type;
//    struct timeval event_time;
//    const char *event_name;
//    const char *user_defined_name;
//    const char *caller_id_name;
//    const char *caller_id_num;
//    const char *caller_id_ani;
//    const char *caller_id_rdnis;
//    const char *caller_id_dnid;
//    const char *extension;
//    const char *context;
//    const char *channel_name;
//    const char *application_name;
//    const char *application_data;
//    const char *account_code;
//    const char *peer_account;
//    const char *unique_id;
//    const char *linked_id;
//    uint amaflag;
//    const char *user_field;
//    const char *peer;
//    const char *extra;

    struct ast_json *payload;
    payload = ast_json_object_create();
    if (!payload) { return NULL; }
    int res = 0;
//    res |= ast_json_object_set(payload, "", ast_json_string_create(record.));
    res |= ast_json_object_set(payload, "event_time", json_timeformat(record.event_time, zone, dateformat));
    res |= ast_json_object_set(payload, "event_name", ast_json_string_create(record.event_name));
    res |= ast_json_object_set(payload, "user_defined_name", ast_json_string_create(record.user_defined_name));
    res |= ast_json_object_set(payload, "caller_id_name", ast_json_string_create(record.caller_id_name));
    res |= ast_json_object_set(payload, "caller_id_num", ast_json_string_create(record.caller_id_num));
    res |= ast_json_object_set(payload, "caller_id_ani", ast_json_string_create(record.caller_id_ani));
    res |= ast_json_object_set(payload, "caller_id_rdnis", ast_json_string_create(record.caller_id_rdnis));
    res |= ast_json_object_set(payload, "caller_id_dnid", ast_json_string_create(record.caller_id_dnid));
    res |= ast_json_object_set(payload, "extension", ast_json_string_create(record.extension));
    res |= ast_json_object_set(payload, "context", ast_json_string_create(record.context));
    res |= ast_json_object_set(payload, "channel_name", ast_json_string_create(record.channel_name));
    res |= ast_json_object_set(payload, "application_name", ast_json_string_create(record.application_name));
    res |= ast_json_object_set(payload, "application_data", ast_json_string_create(record.application_data));
    res |= ast_json_object_set(payload, "account_code", ast_json_string_create(record.account_code));
    res |= ast_json_object_set(payload, "peer_account", ast_json_string_create(record.peer_account));
    res |= ast_json_object_set(payload, "unique_id", ast_json_string_create(record.unique_id));
    res |= ast_json_object_set(payload, "linked_id", ast_json_string_create(record.linked_id));
    res |= ast_json_object_set(payload, "amaflag", ast_json_integer_create(record.amaflag));
    res |= ast_json_object_set(payload, "user_field", ast_json_string_create(record.user_field));
    res |= ast_json_object_set(payload, "peer", ast_json_string_create(record.peer));
    res |= ast_json_object_set(payload, "extra", ast_json_string_create(record.extra));

    return payload;
}

static void cel_kafka_put(struct ast_event *event) {
    char *cel_buffer;
    struct ast_json *t_cel_json;
    struct ast_cel_event_record record = {
            .version = AST_CEL_EVENT_RECORD_VERSION,
    };

    if (!enablecel) {
        return;
    }

    if (ast_cel_fill_record(event, &record)) {
        return;
    }

    t_cel_json = obj_as_is(record);
    if (!t_cel_json) {
        return;
    }
    cel_buffer = ast_json_dump_string(t_cel_json);
    ast_json_unref(t_cel_json);
    ast_kafka_produce(kafka_topic, cel_buffer);
    ast_json_free(cel_buffer);
}

static int load_config() {
    const char *cat = NULL;
    struct ast_config *cfg;
    struct ast_flags config_flags = {0};
    struct ast_variable *v;

    cfg = ast_config_load(conf_file, config_flags);

    if (cfg == CONFIG_STATUS_FILEINVALID) {
        ast_log(LOG_WARNING, "Configuration file '%s' is invalid.\n", conf_file);
        return -1;
    } else if (!cfg) {
        ast_log(LOG_WARNING, "Failed to load configuration file '%s'\n", conf_file);
        return -1;
    }

    enablecel = 0;
    kafka_topic = ast_strdup(DEFAULT_KAFKA_TOPIC);
    dateformat = ast_strdup(DEFAULT_DATEFORMAT);
    zone = NULL;

    while ((cat = ast_category_browse(cfg, cat))) {

        if (strcasecmp(cat, "general")) {
            continue;
        }

        for (v = ast_variable_browse(cfg, cat); v; v = v->next) {
            if (!strcasecmp(v->name, "enabled")) {
                enablecel = ast_true(v->value) ? 1 : 0;
            } else if (!strcasecmp(v->name, "topic")) {
                ast_free(kafka_topic);
                kafka_topic = ast_strdup(v->value);
            } else if (!strcasecmp(v->name, "dateformat")) {
                ast_free(dateformat);
                dateformat = ast_strdup(v->value);
            } else if (!strcasecmp(v->name, "timezone")) {
                ast_free(zone);
                zone = ast_strdup(v->value);
            } else {
                ast_log(LOG_NOTICE, "Unknown option '%s' specified for %s.\n", v->name, DESCRIPTION);
            }
        }
    }
    ast_config_destroy(cfg);

    if (enablecel) {
        ast_log(LOG_NOTICE, "Using kafka topic %s", kafka_topic);
    } else {
        ast_log(LOG_NOTICE, "%s is not enabled", DESCRIPTION);
    }

    return 0;
}


static int load_module(void) {
    if (load_config()) {
        ast_log(LOG_WARNING, "%s is not activated.\n", DESCRIPTION);
        return AST_MODULE_LOAD_DECLINE;
    }
    if (ast_cel_backend_register(DESCRIPTION, cel_kafka_put)) {
        ast_log(LOG_ERROR, "Unable to register %s\n", DESCRIPTION);
        return AST_MODULE_LOAD_DECLINE;
    }

    return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void) {
    ast_cel_backend_unregister(DESCRIPTION);
    ast_free(kafka_topic);
    ast_free(dateformat);
    ast_free(zone);

    return 0;
}


AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, DESCRIPTION,
    .support_level = AST_MODULE_SUPPORT_EXTENDED,
    .load = load_module,
    .unload = unload_module,
    .load_pri = AST_MODPRI_CDR_DRIVER,
    .requires = "cel,res_kafka",
);