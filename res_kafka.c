//
// Created by braams on 07.08.2021.
//

#define AST_MODULE_SELF_SYM __internal_res_kafka_self
#define AST_MODULE "res_kafka"

#include <asterisk.h>
#include <stdio.h>

#include <librdkafka/rdkafka.h>
#include <asterisk/cdr.h>
#include <asterisk/module.h>
#include <asterisk/config.h>
#include <asterisk/json.h>


#define CONF_FILE    "res_kafka.conf"
#define DEFAULT_KAFKA_BROKERS "127.0.0.1:9092"

static const char name[] = "res_kafka";


static char *kafka_brokers;

rd_kafka_t *handle;         /* Producer instance handle */
rd_kafka_conf_t *conf;  /* Temporary configuration object */
char errstr[512];       /* librdkafka API error reporting buffer */


static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {
    if (rkmessage->err)
        ast_log(LOG_ERROR, "Message delivery failed: %s\n", rd_kafka_err2str(rkmessage->err));
    else
        ast_log(LOG_NOTICE, "Message delivered (%zd bytes, partition %" PRId32 ")\n",
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
    handle = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!handle) {
        ast_log(LOG_ERROR, "Failed to create new producer: %s\n", errstr);
        return 1;
    }
    ast_log(LOG_NOTICE, "rd_kafka_new done");
    return 0;
}


static int load_config() {
    char *cat = NULL;
    struct ast_config *cfg;
    struct ast_variable *v;
    struct ast_flags config_flags = {0};

    cfg = ast_config_load(CONF_FILE, config_flags);

    if (cfg == CONFIG_STATUS_FILEINVALID) {
        ast_log(LOG_ERROR, "Config file '%s' could not be parsed\n", CONF_FILE);
        return -1;
    }

    if (!cfg) {
        ast_log(LOG_WARNING, "Failed to load configuration file. Module not activated.\n");
        return -1;
    }

    /* Bootstrap the default configuration */
    kafka_brokers = ast_strdup(DEFAULT_KAFKA_BROKERS);

    while ((cat = ast_category_browse(cfg, cat))) {
        if (!strcasecmp(cat, "general")) {
            v = ast_variable_browse(cfg, cat);
            while (v) {

                if (!strcasecmp(v->name, "brokers")) {
                    ast_free(kafka_brokers);
                    kafka_brokers = ast_strdup(v->value);
                }
                v = v->next;

            }
        }
    }

    ast_log(LOG_NOTICE, "Using kafka brokers %s", kafka_brokers);
    return 0;
}


static int load_module(void) {
    if (load_config()) {
        return AST_MODULE_LOAD_DECLINE;
    }
    kafka_connect();
    return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void) {
    ast_free(kafka_brokers);

    ast_log(LOG_NOTICE, "Flushing final messages..\n");
    rd_kafka_flush(handle, 10 * 1000 /* wait for max 10 seconds */);

    /* If the output queue is still not empty there is an issue
     * with producing messages to the clusters. */
    if (rd_kafka_outq_len(handle) > 0)
        ast_log(LOG_NOTICE, "%d message(s) were not delivered\n", rd_kafka_outq_len(handle));

    /* Destroy the producer instance */
    rd_kafka_destroy(handle);
    return 0;
}


AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "Kafka Support",
                .support_level = AST_MODULE_SUPPORT_CORE,
                .load = load_module,
                .unload = unload_module,
                .load_pri = AST_MODPRI_CDR_DRIVER,
                .requires = "cdr",
);