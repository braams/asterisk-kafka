/*
 */

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

/*** MODULEINFO
	<support_level>core</support_level>
 ***/


#include <asterisk.h>
#include <stdio.h>

#include <librdkafka/rdkafka.h>
#include <asterisk/cdr.h>
#include <asterisk/module.h>
#include <asterisk/config.h>
#include <asterisk/json.h>


#define CONF_FILE    "cdr_kafka.conf"
#define DEFAULT_KAFKA_BROKERS "127.0.0.1:9092"
#define DEFAULT_KAFKA_TOPIC "asterisk-cdr"

static const char name[] = "cdr_kafka";

static int enablecdr = 0;
static char *kafka_brokers;
static char *kafka_topic;
rd_kafka_t *rk;         /* Producer instance handle */
rd_kafka_conf_t *conf;  /* Temporary configuration object */
char errstr[512];       /* librdkafka API error reporting buffer */

AST_RWLOCK_DEFINE_STATIC(config_lock);

static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {
    if (rkmessage->err)
        ast_log(LOG_ERROR, "Message delivery failed: %s\n", rd_kafka_err2str(rkmessage->err));
    else
        ast_log(LOG_NOTICE, "Message delivered (%zd bytes, partition %"
    PRId32
    ")\n",
            rkmessage->len, rkmessage->partition);
    /* The rkmessage is destroyed automatically by librdkafka */
}

static void log_cb(const rd_kafka_t *rk, int level, const char *fac, const char *buf) {
    ast_log(LOG_NOTICE, " %s: %s: %s\n", fac, rk ? rd_kafka_name(rk) : NULL, buf);
}

static void error_cb(rd_kafka_t *rk, int err, const char *reason, void *opaque) {
    ast_log(LOG_ERROR, "%s: %s: %s\n", rk ? rd_kafka_name(rk) : NULL, rd_kafka_err2str(err), reason);
}

static int kafka_connect(void) {

    conf = rd_kafka_conf_new();

    rd_kafka_conf_set_log_cb(conf, log_cb);
    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
    rd_kafka_conf_set_error_cb(conf, error_cb);

    /* Set bootstrap broker(s) as a comma-separated list of
     * host or host:port (default port 9092).
     * librdkafka will use the bootstrap brokers to acquire the full
     * set of brokers from the cluster. */
    if (rd_kafka_conf_set(conf, "bootstrap.servers", kafka_brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        ast_log(LOG_ERROR, "%s\n", errstr);
        return 1;
    }
    ast_log(LOG_NOTICE, "rd_kafka_conf_set done");

    /* Set the delivery report callback.
     * This callback will be called once per message to inform
     * the application if delivery succeeded or failed.
     * See dr_msg_cb() above.
     * The callback is only triggered from rd_kafka_poll() and
     * rd_kafka_flush(). */


    /*
     * Create producer instance.
     *
     * NOTE: rd_kafka_new() takes ownership of the conf object
     *       and the application must not reference it again after
     *       this call.
     */
    rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk) {
        ast_log(LOG_ERROR, "Failed to create new producer: %s\n", errstr);
        return 1;
    }
    ast_log(LOG_NOTICE, "rd_kafka_new done");
    return 0;
}

static int load_config(int reload) {
    char *cat = NULL;
    struct ast_config *cfg;
    struct ast_variable *v;
    struct ast_flags config_flags = {reload ? CONFIG_FLAG_FILEUNCHANGED : 0};
    int newenablecdr = 0;

    cfg = ast_config_load(CONF_FILE, config_flags);
    if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
        return 0;
    }

    if (cfg == CONFIG_STATUS_FILEINVALID) {
        ast_log(LOG_ERROR, "Config file '%s' could not be parsed\n", CONF_FILE);
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
        ast_free(kafka_brokers);
        ast_free(kafka_topic);
    }

    /* Bootstrap the default configuration */
    kafka_brokers = ast_strdup(DEFAULT_KAFKA_BROKERS);
    kafka_topic = ast_strdup(DEFAULT_KAFKA_TOPIC);

    while ((cat = ast_category_browse(cfg, cat))) {
        if (!strcasecmp(cat, "general")) {
            v = ast_variable_browse(cfg, cat);
            while (v) {

                if (!strcasecmp(v->name, "enabled")) {
                    newenablecdr = ast_true(v->value);
                } else if (!strcasecmp(v->name, "brokers")) {
                    ast_free(kafka_brokers);
                    kafka_brokers = ast_strdup(v->value);
                } else if (!strcasecmp(v->name, "topic")) {
                    ast_free(kafka_topic);
                    kafka_topic = ast_strdup(v->value);
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
        ast_log(LOG_NOTICE, "Using kafka brokers %s with topic %s", kafka_brokers, kafka_topic);
    }
    enablecdr = newenablecdr;

    return 0;
}

static int kafka_put(struct ast_cdr *cdr) {
    rd_kafka_resp_err_t err;

    struct ast_tm timeresult;
    char strAnswerTime[80] = "";
    char strStartTime[80];
    char strEndTime[80];
    char *cdr_buffer;

    struct ast_json *t_cdr_json;

    if (!enablecdr) {
        return 0;
    }

    ast_localtime(&cdr->start, &timeresult, NULL);
    ast_strftime(strStartTime, sizeof(strStartTime), DATE_FORMAT, &timeresult);

    if (cdr->answer.tv_sec) {
        ast_localtime(&cdr->answer, &timeresult, NULL);
        ast_strftime(strAnswerTime, sizeof(strAnswerTime), DATE_FORMAT, &timeresult);
    }

    ast_localtime(&cdr->end, &timeresult, NULL);
    ast_strftime(strEndTime, sizeof(strEndTime), DATE_FORMAT, &timeresult);

    t_cdr_json = ast_json_pack(
            "{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:i, s:i, s:s, s:s, s:s, s:s}",
            "AccountCode", S_OR(cdr->accountcode, ""),
            "Source", S_OR(cdr->src, ""),
            "Destination", S_OR(cdr->dst, ""),
            "DestinationContext", S_OR(cdr->dcontext, ""),
            "CallerID", S_OR(cdr->clid, ""),
            "Channel", S_OR(cdr->channel, ""),
            "DestinationChannel", S_OR(cdr->dstchannel, ""),
            "LastApplication", S_OR(cdr->lastapp, ""),
            "LastData", S_OR(cdr->lastdata, ""),
            "StartTime", S_OR(strStartTime, ""),
            "AnswerTime", S_OR(strAnswerTime, ""),
            "EndTime", S_OR(strEndTime, ""),
            "Duration", cdr->duration,
            "Billsec", cdr->billsec,
            "Disposition", S_OR(ast_cdr_disp2str(cdr->disposition), ""),
            "AMAFlags", S_OR(ast_channel_amaflags2string(cdr->amaflags), ""),
            "UniqueID", S_OR(cdr->uniqueid, ""),
            "UserField", S_OR(cdr->userfield, ""));

    cdr_buffer = ast_json_dump_string(t_cdr_json);

    ast_json_unref(t_cdr_json);

    err = rd_kafka_producev(
            /* Producer handle */
            rk,
            /* Topic name */
            RD_KAFKA_V_TOPIC(kafka_topic),
            /* Make a copy of the payload. */
            RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
            /* Message value and length */
            RD_KAFKA_V_VALUE(cdr_buffer, strlen(cdr_buffer)),
            /* Per-Message opaque, provided in
             * delivery report callback as
             * msg_opaque. */
            RD_KAFKA_V_OPAQUE(NULL),
            /* End sentinel */
            RD_KAFKA_V_END);

    if (err) {
        /*
         * Failed to *enqueue* message for producing.
         */
        ast_log(LOG_ERROR, "Failed to produce to topic %s: %s\n", kafka_topic, rd_kafka_err2str(err));
    } else {
        ast_log(LOG_NOTICE, "Enqueued message (%zd bytes) for topic %s\n", strlen(cdr_buffer), kafka_topic);
    }

    ast_json_free(cdr_buffer);

    /* A producer application should continually serve
     * the delivery report queue by calling rd_kafka_poll()
     * at frequent intervals.
     * Either put the poll call in your main loop, or in a
     * dedicated thread, or call it after every
     * rd_kafka_produce() call.
     * Just make sure that rd_kafka_poll() is still called
     * during periods where you are not producing any messages
     * to make sure previously produced messages have their
     * delivery report callback served (and any other callbacks
     * you register). */
    rd_kafka_poll(rk, 0/*non-blocking*/);
    return 0;
}

static int unload_module(void) {
    if (ast_cdr_unregister(name)) {
        return -1;
    }

    ast_free(kafka_brokers);
    ast_free(kafka_topic);

    ast_log(LOG_NOTICE, "Flushing final messages..\n");
    rd_kafka_flush(rk, 10 * 1000 /* wait for max 10 seconds */);

    /* If the output queue is still not empty there is an issue
     * with producing messages to the clusters. */
    if (rd_kafka_outq_len(rk) > 0)
        ast_log(LOG_NOTICE, "%d message(s) were not delivered\n", rd_kafka_outq_len(rk));

    /* Destroy the producer instance */
    rd_kafka_destroy(handle);
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
    kafka_connect();
    return AST_MODULE_LOAD_SUCCESS;
}

static int reload(void) {
    //    return load_config(1);
    ast_log(LOG_NOTICE, "Reload isn't implemented yet...\n");
    return -1;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "Kafka CDR Backend",
                .support_level = AST_MODULE_SUPPORT_CORE,
                .load = load_module,
                .unload = unload_module,
                .reload = reload,
                .load_pri = AST_MODPRI_CDR_DRIVER,
                .requires = "cdr",
                );