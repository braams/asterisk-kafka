CREATE TABLE kafka_asterisk_cel
(
    event_time        DateTime,
    event_name        String,
    user_defined_name String,
    caller_id_name    String,
    caller_id_num     String,
    caller_id_ani     String,
    caller_id_rdnis   String,
    caller_id_dnid    String,
    extension         String,
    context           String,
    channel_name      String,
    application_name  String,
    application_data  String,
    account_code      String,
    peer_account      String,
    unique_id         String,
    linked_id         String,
    amaflag           UInt32,
    user_field        String,
    peer              String,
    extra             String
) ENGINE = Kafka('localhost:9092', 'asterisk_cel', 'group1', 'JSONEachRow');

CREATE TABLE asterisk_cel
(
    event_time        DateTime,
    event_name        String,
    user_defined_name String,
    caller_id_name    String,
    caller_id_num     String,
    caller_id_ani     String,
    caller_id_rdnis   String,
    caller_id_dnid    String,
    extension         String,
    context           String,
    channel_name      String,
    application_name  String,
    application_data  String,
    account_code      String,
    peer_account      String,
    unique_id         String,
    linked_id         String,
    amaflag           UInt32,
    user_field        String,
    peer              String,
    extra             String
) ENGINE = MergeTree ORDER BY event_time;

CREATE MATERIALIZED VIEW asterisk_cel_consumer TO asterisk_cel AS
SELECT *
FROM kafka_asterisk_cel;
