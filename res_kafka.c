/*! \file
 *
 * \brief Kafka connection manager
 *
 * \author Max Nesterov <braams@braams.ru>
 *
 */

#define AST_MODULE_SELF_SYM __internal_res_kafka_self
#define AST_MODULE "res_kafka"

#include <asterisk.h>
#include <asterisk/module.h>
#include <asterisk/config.h>
#include <asterisk/cli.h>
#include <asterisk/json.h>
#include <asterisk/sched.h>
#include <librdkafka/rdkafka.h>
#include <unistd.h>
#include "res_kafka.h"


#define CONF_FILE "res_kafka.conf"
#define DEFAULT_KAFKA_BROKERS "127.0.0.1:9092"

static const char name[] = "res_kafka";

static const int poll_interval_ms = 1000;
static char *kafka_brokers;

rd_kafka_t *handle;         /* Producer instance handle */
rd_kafka_conf_t *conf;  /* Temporary configuration object */
char errstr[512];       /* librdkafka API error reporting buffer */
static char *json_stats;
static int enabled;
static struct ast_sched_context *sched;

static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {
    if (rkmessage->err)
        ast_log(LOG_ERROR, "Message delivery failed: %s\n", rd_kafka_err2str(rkmessage->err));
    else
        ast_log(LOG_DEBUG, "Message delivered (%zd bytes, partition %d)\n", rkmessage->len, rkmessage->partition);
    /* The rkmessage is destroyed automatically by librdkafka */
}

static void log_cb(const rd_kafka_t *rk, int level, const char *fac, const char *buf) {
    ast_log(LOG_NOTICE, " %s: %s: %s\n", fac, rk ? rd_kafka_name(rk) : NULL, buf);
}

static void error_cb(rd_kafka_t *rk, int err, const char *reason, void *opaque) {
    ast_log(LOG_ERROR, "%s: %s: %s\n", rk ? rd_kafka_name(rk) : NULL, rd_kafka_err2str(err), reason);
}

static int stats_cb(rd_kafka_t *rk, char *json, size_t json_len, void *opaque) {
//    ast_log(LOG_NOTICE, "%s\n%s\n", rk ? rd_kafka_name(rk) : NULL, json);
    struct ast_json *body;
    body = ast_json_load_buf(json, json_len, NULL);
    json_stats = ast_json_dump_string_format(body, AST_JSON_PRETTY);
    ast_json_unref(body);
    return 0;
}

static int do_poll(const void *unused) {
    rd_kafka_poll(handle, 0);
    return poll_interval_ms;
}

static int kafka_connect(void) {
    conf = rd_kafka_conf_new();

    rd_kafka_conf_set_log_cb(conf, log_cb);
    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
    rd_kafka_conf_set_error_cb(conf, error_cb);
    rd_kafka_conf_set_stats_cb(conf, stats_cb);

    /* Set bootstrap broker(s) as a comma-separated list of
     * host or host:port (default port 9092).
     * librdkafka will use the bootstrap brokers to acquire the full
     * set of brokers from the cluster. */
    if (rd_kafka_conf_set(conf, "bootstrap.servers", kafka_brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        ast_log(LOG_ERROR, "%s\n", errstr);
        return 1;
    }
    if (rd_kafka_conf_set(conf, "statistics.interval.ms", "1000", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        ast_log(LOG_ERROR, "%s\n", errstr);
        return 1;
    }

    ast_log(LOG_NOTICE, "rd_kafka_conf_set done\n");


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
    ast_log(LOG_NOTICE, "rd_kafka_new done\n");
    return 0;
}

int ast_kafka_produce(const char *topic, const char *buffer) {
    if (enabled) {
        static char *buf;
        buf = ast_strdup(buffer);
        rd_kafka_resp_err_t err;
        err = rd_kafka_producev(
                /* Producer handle */
                handle,
                /* Topic name */
                RD_KAFKA_V_TOPIC(topic),
                /* Make a copy of the payload. */
                RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                /* Message value and length */
                RD_KAFKA_V_VALUE(buf, strlen(buf)),
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
            ast_log(LOG_ERROR, "Failed to produce to topic %s: %s\n", topic, rd_kafka_err2str(err));
        } else {
            ast_log(LOG_DEBUG, "Enqueued message (%zd bytes) for topic %s\n", strlen(buf), topic);
        }
        ast_free(buf);
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
        rd_kafka_poll(handle, 0/*non-blocking*/);
    }
    return 0;
}

static int start_sched(void) {
    if (sched) {
        return 0; /* already started */
    }
    if (!(sched = ast_sched_context_create())) {
        ast_log(LOG_ERROR, "Failed to create scheduler context\n");
        return -1;
    }

    if (ast_sched_start_thread(sched)) {
        ast_sched_context_destroy(sched);
        sched = NULL;
        return -1;
    }
    if (ast_sched_add_variable(sched, poll_interval_ms, do_poll, NULL, 1) < 0) {
        ast_log(LOG_ERROR, "Unable to schedule \n");
        ast_sched_context_destroy(sched);
        sched = NULL;
        return -1;
    }
    ast_log(LOG_NOTICE, "Poll task started\n");
    return 0;
}

static int stop_sched(void) {
    if (sched) {
        ast_sched_context_destroy(sched);
        sched = NULL;
        ast_log(LOG_NOTICE, "Poll task stopped\n");
    }
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
        ast_log(LOG_WARNING, "Failed to load configuration file '%s'\n", CONF_FILE);
        return -1;
    }
    enabled = 1;
    /* Bootstrap the default configuration */
    kafka_brokers = ast_strdup(DEFAULT_KAFKA_BROKERS);

    while ((cat = ast_category_browse(cfg, cat))) {
        if (!strcasecmp(cat, "general")) {
            v = ast_variable_browse(cfg, cat);
            while (v) {
                if (!strcasecmp(v->name, "brokers") || !strcasecmp(v->name, "bootstrap.servers")) {
                    ast_free(kafka_brokers);
                    kafka_brokers = ast_strdup(v->value);
                }
                v = v->next;
            }
        }
    }

    ast_log(LOG_NOTICE, "Using kafka brokers %s\n", kafka_brokers);
    return 0;
}

static char *handle_cli_kafka_produce(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a) {
    const char *topic, *message;
    switch (cmd) {
        case CLI_INIT:
            e->command = "kafka produce";
            e->usage = "Usage: kafka produce <topic> <message>\n"
                       "       Produce the Kafka message.\n";
            return NULL;
        case CLI_GENERATE:
            return NULL;
    }

    if (a->argc != 4)
        return CLI_SHOWUSAGE;
    if (ast_strlen_zero(a->argv[2]) || ast_strlen_zero(a->argv[3]))
        return CLI_SHOWUSAGE;

    topic = a->argv[2];
    message = a->argv[3];

    ast_kafka_produce(topic, message);

}

static char *handle_cli_kafka_stats(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a) {
    switch (cmd) {
        case CLI_INIT:
            e->command = "kafka stats";
            e->usage =
                    "Usage: kafka stats\n"
                    "       Displays the Kafka stats.\n";
            return NULL;
        case CLI_GENERATE:
            return NULL;
    }

    if (a->argc > 2) {
        return CLI_SHOWUSAGE;
    }
    rd_kafka_poll(handle, 0);
    ast_cli(a->fd, "stats: %s\n", json_stats);

    return CLI_SUCCESS;
}

static struct ast_cli_entry cli_stats = AST_CLI_DEFINE(handle_cli_kafka_stats, "Display the Kafka stats");
static struct ast_cli_entry cli_produce = AST_CLI_DEFINE(handle_cli_kafka_produce, "Publish the Kafka message");

static int load_module(void) {
    if (load_config()) {
        return AST_MODULE_LOAD_DECLINE;
    }
    kafka_connect();
    start_sched();
    ast_cli_register(&cli_stats);
    ast_cli_register(&cli_produce);
    return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void) {

    ast_log(LOG_NOTICE, "Flushing final messages...\n");
    rd_kafka_flush(handle, 10 * 1000 /* wait for max 10 seconds */);
    ast_log(LOG_NOTICE, "...done\n");
    /* If the output queue is still not empty there is an issue
     * with producing messages to the clusters. */
    if (rd_kafka_outq_len(handle) > 0)
        ast_log(LOG_NOTICE, "%d message(s) were not delivered\n", rd_kafka_outq_len(handle));

    stop_sched();
    ast_free(kafka_brokers);
    /* Destroy the producer instance */
    rd_kafka_destroy(handle);
    ast_cli_unregister(&cli_stats);
    ast_cli_unregister(&cli_produce);
    return 0;
}


AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_GLOBAL_SYMBOLS | AST_MODFLAG_LOAD_ORDER, "Kafka Support",
    .support_level = AST_MODULE_SUPPORT_EXTENDED,
    .load = load_module,
    .unload = unload_module,
    .load_pri = AST_MODPRI_APP_DEPEND,
    .requires = "",
);