/*! \file
 *
 * \brief Kafka produce dialplan application
 *
 * \author Max Nesterov <braams@braams.ru>
 *
 */

#define AST_MODULE_SELF_SYM __internal_app_kafka_self
#define AST_MODULE "app_kafka"

#include <asterisk.h>
#include <stdio.h>

#include <asterisk/module.h>
#include <asterisk/app.h>
#include "res_kafka.h"

static const char app[] = "KafkaProduce";
static const char synopsis[] = "Produce message to Kafka";
static const char descrip[] =
        "KafkaProduce():  Produce message to Kafka\n"
        "Syntax:\n"
        "  KafkaProduce(<topic>,<message>)\n";


static int exec(struct ast_channel *chan, const char *data) {
    char *parse;
    AST_DECLARE_APP_ARGS(args,
                         AST_APP_ARG(topic);
    AST_APP_ARG(message);
    );
    parse = ast_strdupa(data);
    AST_STANDARD_APP_ARGS(args, parse);

    if (ast_strlen_zero(args.topic)) {
        ast_log(LOG_ERROR, "topic is required\n");
        return -1;
    }
    if (ast_strlen_zero(args.message)) {
        ast_log(LOG_ERROR, "message is required\n");
        return -1;
    }
    ast_log(LOG_NOTICE, "KafkaProduce %s %s\n", args.topic, args.message);
    ast_kafka_produce(args.topic, args.message);

}

static int load_module(void) {
    return ast_register_application(app, exec, synopsis, descrip);
}

static int unload_module(void) {
    return ast_unregister_application(app);
}


AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "Kafka Support",
    .support_level = AST_MODULE_SUPPORT_EXTENDED,
    .load = load_module,
    .unload = unload_module,
    .load_pri = AST_MODPRI_DEFAULT,
    .requires = "res_kafka",
);