//
// Created by braams on 10.08.2021.
//

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


#define CEL_BACKEND_NAME    "cel_kafka"
#define DESCRIPTION         "Kafka CEL Backend"
#define CONF_FILE           "cel_kafka.conf"
#define DEFAULT_KAFKA_TOPIC "asterisk_cel"
#define DEFAULT_DATEFORMAT    "%Y-%m-%d %T"

static int enablecel;

static char *kafka_topic;
static char *dateformat;


static void cel_kafka_put(struct ast_event *event) {
    struct ast_tm timeresult;
    char start_time[80];
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

    t_cel_json = ast_json_pack(
            "{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
            "EventName", S_OR(record.event_name, ""),
            "AccountCode", S_OR(record.account_code, ""),
            "CallerIDnum", S_OR(record.caller_id_num, ""),
            "CallerIDname", S_OR(record.caller_id_name, ""),
            "CallerIDani", S_OR(record.caller_id_ani, ""),
            "CallerIDrdnis", S_OR(record.caller_id_rdnis, ""),
            "CallerIDdnid", S_OR(record.caller_id_dnid, ""),
            "Exten", S_OR(record.extension, ""),
            "Context", S_OR(record.context, ""),
            "Channel", S_OR(record.channel_name, ""),
            "Application", S_OR(record.application_name, ""),
            "AppData", S_OR(record.application_data, ""),
            "EventTime", S_OR(start_time, ""),
            "AMAFlags", S_OR(ast_channel_amaflags2string(record.amaflag), ""),
            "UniqueID", S_OR(record.unique_id, ""),
            "LinkedID", S_OR(record.linked_id, ""),
            "Userfield", S_OR(record.user_field, ""),
            "Peer", S_OR(record.peer_account, ""),
            "PeerAccount", S_OR(record.peer_account, ""),
            "Extra", S_OR(record.extra, "")

    );

    cel_buffer = ast_json_dump_string(t_cel_json);

    ast_json_unref(t_cel_json);


    ast_localtime(&record.event_time, &timeresult, NULL);
    ast_strftime(start_time, sizeof(start_time), dateformat, &timeresult);


    ast_kafka_produce(kafka_topic, cel_buffer);

    ast_json_free(cel_buffer);


}

static int load_config(int reload) {
    const char *cat = NULL;
    struct ast_config *cfg;
    struct ast_flags config_flags = {0};
    struct ast_variable *v;

    cfg = ast_config_load(CONF_FILE, config_flags);

    if (cfg == CONFIG_STATUS_FILEINVALID) {
        ast_log(LOG_WARNING, "Configuration file '%s' is invalid. %s not activated.\n",
                CONF_FILE, DESCRIPTION);
        return -1;
    } else if (!cfg) {
        ast_log(LOG_WARNING, "Failed to load configuration file. %s not activated.\n", DESCRIPTION);
        return -1;
    }

    enablecel = 0;
    kafka_topic = ast_strdup(DEFAULT_KAFKA_TOPIC);
    dateformat = ast_strdup(DEFAULT_DATEFORMAT);


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
            } else {
                ast_log(LOG_NOTICE, "Unknown option '%s' specified for %s.\n", v->name, DESCRIPTION);
            }
        }
    }
    ast_config_destroy(cfg);
    return 0;


}


static int load_module(void) {
    if (load_config(0)) {
        return AST_MODULE_LOAD_DECLINE;
    }else{
            if (ast_cel_backend_register(CEL_BACKEND_NAME, cel_kafka_put)) {
                ast_log(LOG_ERROR, "Unable to register %s\n", DESCRIPTION);
                return AST_MODULE_LOAD_DECLINE;
            }

    }
    return AST_MODULE_LOAD_SUCCESS;

}

static int unload_module(void) {
//    if (!ast_cel_backend_unregister(CEL_BACKEND_NAME)) {
//        ast_log(LOG_ERROR, "Unable to unregister %s\n", DESCRIPTION);
//        return -1;
//    }
    return 0;
}


AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, DESCRIPTION,
    .support_level = AST_MODULE_SUPPORT_EXTENDED,
    .load = load_module,
    .unload = unload_module,
    .load_pri = AST_MODPRI_CDR_DRIVER,
    .requires = "cel,res_kafka",
);