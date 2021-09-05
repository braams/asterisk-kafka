CREATE TABLE kafka_asterisk_cdr
(
    clid        String,
    src         String,
    dst         String,
    dcontext    String,
    channel     String,
    dstchannel  String,
    lastapp     String,
    lastdata    String,
    start       DateTime,
    answer      DateTime,
    end         DateTime,
    duration    Int32,
    billsec     Int32,
    disposition Int32,
    amaflags    Int32,
    accountcode String,
    peeraccount String,
    flags       UInt32,
    uniqueid    String,
    linkedid    String,
    userfield   String,
    sequence    Int32
) ENGINE = Kafka('localhost:9092', 'asterisk_cdr', 'group1', 'JSONEachRow');

CREATE TABLE asterisk_cdr
(
    clid        String,
    src         String,
    dst         String,
    dcontext    String,
    channel     String,
    dstchannel  String,
    lastapp     String,
    lastdata    String,
    start       DateTime,
    answer      DateTime,
    end         DateTime,
    duration    Int32,
    billsec     Int32,
    disposition Int32,
    amaflags    Int32,
    accountcode String,
    peeraccount String,
    flags       UInt32,
    uniqueid    String,
    linkedid    String,
    userfield   String,
    sequence    Int32
) ENGINE = MergeTree ORDER BY end;

CREATE MATERIALIZED VIEW asterisk_cdr_consumer TO asterisk_cdr AS
SELECT *
FROM kafka_asterisk_cdr;
