 /*
 * Currently copied from Mysql
 * Keywords. Pending.
 */
#include <sybasedriver.h>

namespace KexiDB {
  const char* SybaseDriver::keywords[] = {
		"ACTION",
		"ADD",
		"AGAINST",
		"AGGREGATE",
		"ALTER",
		"ANALYZE",
		"ANY",
		"ASCII",
		"AUTOINCREMENT",
		"AVG",
		"AVG_ROW_LENGTH",
		"BACKUP",
		"BDB",
		"BERKELEYDB",
		"BIGINT",
		"BINARY",
		"BINLOG",
		"BIT",
		"BLOB",
		"BOOL",
		"BOOLEAN",
		"BOTH",
		"BTREE",
		"BYTE",
		"CACHE",
		"CHANGE",
		"CHANGED",
		"CHAR",
		"CHARACTER",
		"CHARSET",
		"CHECKSUM",
		"CIPHER",
		"CLIENT",
		"CLOSE",
		"COLLATION",
		"COLUMN",
		"COLUMNS",
		"COMMENT",
		"COMMITTED",
		"COMPRESSED",
		"CONCURRENT",
		"CONVERT",
		"CUBE",
		"CURRENT_DATE",
		"CURRENT_TIME",
		"CURRENT_TIMESTAMP",
		"CURRENT_USER",
		"DATA",
		"DATABASES",
		"DATE",
		"DATETIME",
		"DAY",
		"DAY_HOUR",
		"DAY_MICROSECOND",
		"DAY_MINUTE",
		"DAY_SECOND",
		"DEALLOCATE",
		"DEC",
		"DECIMAL",
		"DELAYED",
		"DELAY_KEY_WRITE",
		"DESCRIBE",
		"DES_KEY_FILE",
		"DIRECTORY",
		"DISABLE",
		"DISCARD",
		"DISTINCTROW",
		"DIV",
		"DO",
		"DOUBLE",
		"DUAL",
		"DUMPFILE",
		"DUPLICATE",
		"DYNAMIC",
		"ENABLE",
		"ENCLOSED",
		"ENGINE",
		"ENGINES",
		"ENUM",
		"ERRORS",
		"ESCAPE",
		"ESCAPED",
		"EVENTS",
		"EXECUTE",
		"EXISTS",
		"EXPANSION",
		"EXTENDED",
		"FALSE",
		"FAST",
		"FIELDS",
		"FILE",
		"FIRST",
		"FIXED",
		"FLOAT",
		"FLOAT4",
		"FLOAT8",
		"FLUSH",
		"FORCE",
		"FULLTEXT",
		"FUNCTION",
		"GEOMETRY",
		"GEOMETRYCOLLECTION",
		"GET_FORMAT",
		"GLOBAL",
		"GRANT",
		"GRANTS",
		"HANDLER",
		"HASH",
		"HELP",
		"HIGH_PRIORITY",
		"HOSTS",
		"HOUR",
		"HOUR_MICROSECOND",
		"HOUR_MINUTE",
		"HOUR_SECOND",
		"IDENTIFIED",
		"IF",
		"IMPORT",
		"INDEXES",
		"INFILE",
		"INNOBASE",
		"INNODB",
		"INSERT_METHOD",
		"INT",
		"INT1",
		"INT2",
		"INT3",
		"INT4",
		"INT8",
		"INTERVAL",
		"IO_THREAD",
		"ISOLATION",
		"ISSUER",
		"KEYS",
		"KILL",
		"LAST",
		"LEADING",
		"LEAVES",
		"LEVEL",
		"LINES",
		"LINESTRING",
		"LOAD",
		"LOCAL",
		"LOCALTIME",
		"LOCALTIMESTAMP",
		"LOCK",
		"LOCKS",
		"LOGS",
		"LONG",
		"LONGBLOB",
		"LONGTEXT",
		"LOW_PRIORITY",
		"MASTER",
		"MASTER_CONNECT_RETRY",
		"MASTER_HOST",
		"MASTER_LOG_FILE",
		"MASTER_LOG_POS",
		"MASTER_PASSWORD",
		"MASTER_PORT",
		"MASTER_SERVER_ID",
		"MASTER_SSL",
		"MASTER_SSL_CA",
		"MASTER_SSL_CAPATH",
		"MASTER_SSL_CERT",
		"MASTER_SSL_CIPHER",
		"MASTER_SSL_KEY",
		"MASTER_USER",
		"MAX_CONNECTIONS_PER_HOUR",
		"MAX_QUERIES_PER_HOUR",
		"MAX_ROWS",
		"MAX_UPDATES_PER_HOUR",
		"MEDIUM",
		"MEDIUMBLOB",
		"MEDIUMINT",
		"MEDIUMTEXT",
		"MICROSECOND",
		"MIDDLEINT",
		"MINUTE",
		"MINUTE_MICROSECOND",
		"MINUTE_SECOND",
		"MIN_ROWS",
		"MOD",
		"MODE",
		"MODIFY",
		"MONTH",
		"MULTILINESTRING",
		"MULTIPOINT",
		"MULTIPOLYGON",
		"NAMES",
		"NATIONAL",
		"NDB",
		"NDBCLUSTER",
		"NCHAR",
		"NEW",
		"NEXT",
		"NO",
		"NONE",
		"NO_WRITE_TO_BINLOG",
		"NUMERIC",
		"NVARCHAR",
		"OLD_PASSWORD",
		"ONE_SHOT",
		"OPEN",
		"OPTIMIZE",
		"OPTION",
		"OPTIONALLY",
		"OUTFILE",
		"PACK_KEYS",
		"PARTIAL",
		"PASSWORD",
		"POINT",
		"POLYGON",
		"PRECISION",
		"PREPARE",
		"PREV",
		"PRIVILEGES",
		"PROCEDURE",
		"PROCESS",
		"PROCESSLIST",
		"PURGE",
		"QUERY",
		"QUICK",
		"RAID0",
		"RAID_CHUNKS",
		"RAID_CHUNKSIZE",
		"RAID_TYPE",
		"READ",
		"REAL",
		"REGEXP",
		"RELAY_LOG_FILE",
		"RELAY_LOG_POS",
		"RELAY_THREAD",
		"RELOAD",
		"RENAME",
		"REPAIR",
		"REPEATABLE",
		"REPLICATION",
		"REQUIRE",
		"RESET",
		"RESTORE",
		"RETURNS",
		"REVOKE",
		"RLIKE",
		"ROLLUP",
		"ROWS",
		"ROW_FORMAT",
		"RTREE",
		"SAVEPOINT",
		"SECOND",
		"SECOND_MICROSECOND",
		"SEPARATOR",
		"SERIAL",
		"SERIALIZABLE",
		"SESSION",
		"SHARE",
		"SHOW",
		"SHUTDOWN",
		"SIGNED",
		"SIMPLE",
		"SLAVE",
		"SMALLINT",
		"SOME",
		"SONAME",
		"SOUNDS",
		"SPATIAL",
		"SQL_BIG_RESULT",
		"SQL_BUFFER_RESULT",
		"SQL_CACHE",
		"SQL_CALC_FOUND_ROWS",
		"SQL_NO_CACHE",
		"SQL_SMALL_RESULT",
		"SQL_THREAD",
		"SSL",
		"START",
		"STARTING",
		"STATUS",
		"STOP",
		"STORAGE",
		"STRAIGHT_JOIN",
		"STRING",
		"STRIPED",
		"SUBJECT",
		"SUPER",
		"TABLES",
		"TABLESPACE",
		"TERMINATED",
		"TEXT",
		"TIME",
		"TIMESTAMP",
		"TINYBLOB",
		"TINYINT",
		"TINYTEXT",
		"TRAILING",
		"TRUE",
		"TRUNCATE",
		"TYPE",
		"TYPES",
		"UNCOMMITTED",
		"UNICODE",
		"UNLOCK",
		"UNSIGNED",
		"UNTIL",
		"USAGE",
		"USE",
		"USER",
		"USER_RESOURCES",
		"USE_FRM",
		"UTC_DATE",
		"UTC_TIME",
		"UTC_TIMESTAMP",
		"VALUE",
		"VARBINARY",
		"VARCHAR",
		"VARCHARACTER",
		"VARIABLES",
		"VARYING",
		"WARNINGS",
		"WITH",
		"WORK",
		"WRITE",
		"X509",
		"YEAR",
		"YEAR_MONTH",
		"ZEROFILL",
		0
  };
}
