/*!
 * \file
 * \brief Kakfka CDR records.
 * This module requires the librdkafka library, avaialble from
 * https://github.com/edenhill/librdkafka
 *
 *
 * \author Max Nesterov <braams@braams.ru>
 *
 * \arg See also \ref AstCDR
 *
 * Writes in Kafka broker
 * \ingroup cdr_drivers
 */

/*! \li \ref cdr_kafka.c uses the configuration file \ref cdr_kafka.conf
 * \addtogroup configuration_file Configuration Files
 */

/*!
 * \page cdr_kafka.conf cdr_kafka.conf
 * \verbinclude cdr_kafka.conf.sample
 */


#define AST_MODULE_SELF_SYM __internal_cdr_kafka_self
#define AST_MODULE "cdr_kafka"

#include <asterisk.h>
#include <stdio.h>

#include <librdkafka/rdkafka.h>
#include <asterisk/cdr.h>
#include <asterisk/module.h>
#include <asterisk/config.h>
#include <asterisk/json.h>
#include "res_kafka.h"


#define DESCRIPTION         "Kafka CDR Backend"
#define DEFAULT_KAFKA_TOPIC "asterisk-cdr"
#define DEFAULT_DATE_FORMAT    "%F %T"

static const char name[] = "cdr_kafka";
static const char conf_file[] = "cdr_kafka.conf";


static int enablecdr = 0;
static char *kafka_topic;
static char *dateformat;
static char *zone;

AST_RWLOCK_DEFINE_STATIC(config_lock);

static int load_config(int reload) {
    char *cat = NULL;
    struct ast_config *cfg;
    struct ast_variable *v;
    struct ast_flags config_flags = {reload ? CONFIG_FLAG_FILEUNCHANGED : 0};
    int newenablecdr = 0;

    cfg = ast_config_load(conf_file, config_flags);
    if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
        return 0;
    }

    if (cfg == CONFIG_STATUS_FILEINVALID) {
        ast_log(LOG_ERROR, "Config file '%s' could not be parsed\n", conf_file);
        return -1;
    }

    if (!cfg) {
        ast_log(LOG_WARNING, "Failed to load configuration file. Module not activated.\n");
        if (enablecdr) {
            ast_cdr_backend_suspend(name);
        }
        enablecdr = 0;
        return -1;
    }

    if (reload) {
        ast_rwlock_wrlock(&config_lock);
        ast_free(kafka_topic);
    }

    /* Bootstrap the default configuration */
    kafka_topic = ast_strdup(DEFAULT_KAFKA_TOPIC);
    dateformat = ast_strdup(DEFAULT_DATE_FORMAT);
    zone = NULL;

    while ((cat = ast_category_browse(cfg, cat))) {
        if (!strcasecmp(cat, "general")) {
            v = ast_variable_browse(cfg, cat);
            while (v) {

                if (!strcasecmp(v->name, "enabled")) {
                    newenablecdr = ast_true(v->value);
                } else if (!strcasecmp(v->name, "topic")) {
                    ast_free(kafka_topic);
                    kafka_topic = ast_strdup(v->value);
                } else if (!strcasecmp(v->name, "dateformat")) {
                    ast_free(dateformat);
                    dateformat = ast_strdup(v->value);
                } else if (!strcasecmp(v->name, "timezone")) {
                    ast_free(zone);
                    zone = ast_strdup(v->value);
                }
                v = v->next;

            }
        }
    }

    if (reload) {
        ast_rwlock_unlock(&config_lock);
    }

    ast_config_destroy(cfg);

    if (!newenablecdr) {
        ast_cdr_backend_suspend(name);
    } else if (newenablecdr) {
        ast_cdr_backend_unsuspend(name);
        ast_log(LOG_NOTICE, "Using kafka topic %s", kafka_topic);
    }
    enablecdr = newenablecdr;

    return 0;
}

struct ast_json *json_timeformat(const struct timeval tv, const char *zone, const char *format) {
    char buf[AST_ISO8601_LEN];
    struct ast_tm tm = {};
    ast_localtime(&tv, &tm, zone);
    ast_strftime(buf, sizeof(buf), format, &tm);
    return ast_json_string_create(buf);
}


static struct ast_json *obj_as_is(struct ast_cdr *cdr) {
//    char clid[AST_MAX_EXTENSION];
//    /*! Caller*ID number */
//    char src[AST_MAX_EXTENSION];
//    /*! Destination extension */
//    char dst[AST_MAX_EXTENSION];
//    /*! Destination context */
//    char dcontext[AST_MAX_EXTENSION];
//
//    char channel[AST_MAX_EXTENSION];
//    /*! Destination channel if appropriate */
//    char dstchannel[AST_MAX_EXTENSION];
//    /*! Last application if appropriate */
//    char lastapp[AST_MAX_EXTENSION];
//    /*! Last application data */
//    char lastdata[AST_MAX_EXTENSION];
//
//    struct timeval start;
//
//    struct timeval answer;
//
//    struct timeval end;
//    /*! Total time in system, in seconds */
//    long int duration;
//    /*! Total time call is up, in seconds */
//    long int billsec;
//    /*! What happened to the call */
//    long int disposition;
//    /*! What flags to use */
//    long int amaflags;
//    /*! What account number to use */
//    char accountcode[AST_MAX_ACCOUNT_CODE];
//    /*! Account number of the last person we talked to */
//    char peeraccount[AST_MAX_ACCOUNT_CODE];
//    /*! flags */
//    unsigned int flags;
//    /*! Unique Channel Identifier */
//    char uniqueid[AST_MAX_UNIQUEID];
//    /*! Linked group Identifier */
//    char linkedid[AST_MAX_UNIQUEID];
//    /*! User field */
//    char userfield[AST_MAX_USER_FIELD];
//    /*! Sequence field */
//    int sequence;
//
//    /*! A linked list for variables */
//    struct varshead varshead;
//
//    struct ast_cdr *next;
    struct ast_json *payload;
    payload = ast_json_object_create();
    if (!payload) { return NULL; }
    int res = 0;
//res |= ast_json_object_set(payload, "", ast_json_string_create(cdr->));
    res |= ast_json_object_set(payload, "clid", ast_json_string_create(cdr->clid));
    res |= ast_json_object_set(payload, "src", ast_json_string_create(cdr->src));
    res |= ast_json_object_set(payload, "dst", ast_json_string_create(cdr->dst));
    res |= ast_json_object_set(payload, "dcontext", ast_json_string_create(cdr->dcontext));
    res |= ast_json_object_set(payload, "channel", ast_json_string_create(cdr->channel));
    res |= ast_json_object_set(payload, "dstchannel", ast_json_string_create(cdr->dstchannel));
    res |= ast_json_object_set(payload, "lastapp", ast_json_string_create(cdr->lastapp));
    res |= ast_json_object_set(payload, "lastdata", ast_json_string_create(cdr->lastdata));

    res |= ast_json_object_set(payload, "start", json_timeformat(cdr->start, zone, dateformat));
    res |= ast_json_object_set(payload, "answer", json_timeformat(cdr->answer, zone, dateformat));
    res |= ast_json_object_set(payload, "end", json_timeformat(cdr->end, zone, dateformat));

    res |= ast_json_object_set(payload, "duration", ast_json_integer_create(cdr->duration));
    res |= ast_json_object_set(payload, "billsec", ast_json_integer_create(cdr->billsec));

    res |= ast_json_object_set(payload, "disposition", ast_json_integer_create(cdr->disposition));
    res |= ast_json_object_set(payload, "amaflags", ast_json_integer_create(cdr->amaflags));

    res |= ast_json_object_set(payload, "accountcode", ast_json_string_create(cdr->accountcode));
    res |= ast_json_object_set(payload, "peeraccount", ast_json_string_create(cdr->peeraccount));

    res |= ast_json_object_set(payload, "flags", ast_json_integer_create(cdr->flags));

    res |= ast_json_object_set(payload, "uniqueid", ast_json_string_create(cdr->uniqueid));
    res |= ast_json_object_set(payload, "linkedid", ast_json_string_create(cdr->linkedid));

    res |= ast_json_object_set(payload, "userfield", ast_json_string_create(cdr->userfield));
    res |= ast_json_object_set(payload, "sequence", ast_json_integer_create(cdr->sequence));

    return payload;
}

static int kafka_put(struct ast_cdr *cdr) {
    char *cdr_buffer;
    struct ast_json *t_cdr_json;
    if (!enablecdr) {
        return 0;
    }
    t_cdr_json = obj_as_is(cdr);
    if (!t_cdr_json) {
        return 0;
    }

    cdr_buffer = ast_json_dump_string(t_cdr_json);

    ast_json_unref(t_cdr_json);

    ast_kafka_produce(kafka_topic, cdr_buffer);
    ast_json_free(cdr_buffer);

    return 0;
}


static int unload_module(void) {
    if (ast_cdr_unregister(name)) {
        return -1;
    }

    ast_free(kafka_topic);
    ast_free(dateformat);
    ast_free(zone);

    ast_log(LOG_NOTICE, "Flushing final messages..\n");
    return 0;
}

static int load_module(void) {
    if (ast_cdr_register(name, "Asterisk CDR Kafka Backend", kafka_put)) {
        return AST_MODULE_LOAD_DECLINE;
    }

    if (load_config(0)) {
        ast_cdr_unregister(name);
        return AST_MODULE_LOAD_DECLINE;
    }
    return AST_MODULE_LOAD_SUCCESS;
}

static int reload(void) {
    //    return load_config(1);
    ast_log(LOG_NOTICE, "Reload isn't implemented yet...\n");
    return -1;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, DESCRIPTION,
    .support_level = AST_MODULE_SUPPORT_EXTENDED,
    .load = load_module,
    .unload = unload_module,
    .reload = reload,
    .load_pri = AST_MODPRI_CDR_DRIVER,
    .requires = "cdr,res_kafka",
);