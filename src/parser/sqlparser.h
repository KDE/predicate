#include <field.h>
#include "parser.h"
#include "sqltypes.h"

bool parseData(KexiDB::Parser *p, const char *data);
typedef union {
	char stringValue[255];
	int integerValue;
	struct realType realValue;
	KexiDB::Field::Type colType;
	KexiDB::Field *field;
	KexiDB::BaseExpr *expr;
	KexiDB::NArgExpr *exprList;
	KexiDB::ConstExpr *constExpr;
	KexiDB::QuerySchema *querySchema;
} YYSTYPE;
#define	SQL_TYPE	257
#define	SQL_ABS	258
#define	ACOS	259
#define	AMPERSAND	260
#define	SQL_ABSOLUTE	261
#define	ADA	262
#define	ADD	263
#define	ADD_DAYS	264
#define	ADD_HOURS	265
#define	ADD_MINUTES	266
#define	ADD_MONTHS	267
#define	ADD_SECONDS	268
#define	ADD_YEARS	269
#define	ALL	270
#define	ALLOCATE	271
#define	ALTER	272
#define	AND	273
#define	ANY	274
#define	ARE	275
#define	AS	276
#define	ASIN	277
#define	ASC	278
#define	ASCII	279
#define	ASSERTION	280
#define	ATAN	281
#define	ATAN2	282
#define	AUTHORIZATION	283
#define	AUTO_INCREMENT	284
#define	AVG	285
#define	BEFORE	286
#define	SQL_BEGIN	287
#define	BETWEEN	288
#define	BIGINT	289
#define	BINARY	290
#define	BIT	291
#define	BIT_LENGTH	292
#define	BREAK	293
#define	BY	294
#define	CASCADE	295
#define	CASCADED	296
#define	CASE	297
#define	CAST	298
#define	CATALOG	299
#define	CEILING	300
#define	CENTER	301
#define	SQL_CHAR	302
#define	CHAR_LENGTH	303
#define	CHARACTER_STRING_LITERAL	304
#define	CHECK	305
#define	CLOSE	306
#define	COALESCE	307
#define	COBOL	308
#define	COLLATE	309
#define	COLLATION	310
#define	COLUMN	311
#define	COMMIT	312
#define	COMPUTE	313
#define	CONCAT	314
#define	CONNECT	315
#define	CONNECTION	316
#define	CONSTRAINT	317
#define	CONSTRAINTS	318
#define	CONTINUE	319
#define	CONVERT	320
#define	CORRESPONDING	321
#define	COS	322
#define	COT	323
#define	COUNT	324
#define	CREATE	325
#define	CURDATE	326
#define	CURRENT	327
#define	CURRENT_DATE	328
#define	CURRENT_TIME	329
#define	CURRENT_TIMESTAMP	330
#define	CURTIME	331
#define	CURSOR	332
#define	DATABASE	333
#define	SQL_DATE	334
#define	DATE_FORMAT	335
#define	DATE_REMAINDER	336
#define	DATE_VALUE	337
#define	DAY	338
#define	DAYOFMONTH	339
#define	DAYOFWEEK	340
#define	DAYOFYEAR	341
#define	DAYS_BETWEEN	342
#define	DEALLOCATE	343
#define	DEC	344
#define	DECLARE	345
#define	DEFAULT	346
#define	DEFERRABLE	347
#define	DEFERRED	348
#define	SQL_DELETE	349
#define	DESC	350
#define	DESCRIBE	351
#define	DESCRIPTOR	352
#define	DIAGNOSTICS	353
#define	DICTIONARY	354
#define	DIRECTORY	355
#define	DISCONNECT	356
#define	DISPLACEMENT	357
#define	DISTINCT	358
#define	DOMAIN_TOKEN	359
#define	SQL_DOUBLE	360
#define	DOUBLE_QUOTED_STRING	361
#define	DROP	362
#define	ELSE	363
#define	END	364
#define	END_EXEC	365
#define	EQUAL	366
#define	ESCAPE	367
#define	EXCEPT	368
#define	SQL_EXCEPTION	369
#define	EXEC	370
#define	EXECUTE	371
#define	EXISTS	372
#define	EXP	373
#define	EXPONENT	374
#define	EXTERNAL	375
#define	EXTRACT	376
#define	SQL_FALSE	377
#define	FETCH	378
#define	FIRST	379
#define	SQL_FLOAT	380
#define	FLOOR	381
#define	FN	382
#define	FOR	383
#define	FOREIGN	384
#define	FORTRAN	385
#define	FOUND	386
#define	FOUR_DIGITS	387
#define	FROM	388
#define	FULL	389
#define	GET	390
#define	GLOBAL	391
#define	GO	392
#define	GOTO	393
#define	GRANT	394
#define	GREATER_OR_EQUAL	395
#define	GREATER_THAN	396
#define	HAVING	397
#define	HOUR	398
#define	HOURS_BETWEEN	399
#define	IDENTITY	400
#define	IFNULL	401
#define	SQL_IGNORE	402
#define	IMMEDIATE	403
#define	SQL_IN	404
#define	INCLUDE	405
#define	INDEX	406
#define	INDICATOR	407
#define	INITIALLY	408
#define	INNER	409
#define	INPUT	410
#define	INSENSITIVE	411
#define	INSERT	412
#define	INTEGER	413
#define	INTERSECT	414
#define	INTERVAL	415
#define	INTO	416
#define	IS	417
#define	ISOLATION	418
#define	JOIN	419
#define	JUSTIFY	420
#define	KEY	421
#define	LANGUAGE	422
#define	LAST	423
#define	LCASE	424
#define	LEFT	425
#define	LENGTH	426
#define	LESS_OR_EQUAL	427
#define	LESS_THAN	428
#define	LEVEL	429
#define	LIKE	430
#define	LINE_WIDTH	431
#define	LOCAL	432
#define	LOCATE	433
#define	LOG	434
#define	SQL_LONG	435
#define	LOWER	436
#define	LTRIM	437
#define	LTRIP	438
#define	MATCH	439
#define	SQL_MAX	440
#define	MICROSOFT	441
#define	SQL_MIN	442
#define	MINUS	443
#define	MINUTE	444
#define	MINUTES_BETWEEN	445
#define	MOD	446
#define	MODIFY	447
#define	MODULE	448
#define	MONTH	449
#define	MONTHS_BETWEEN	450
#define	MUMPS	451
#define	NAMES	452
#define	NATIONAL	453
#define	NCHAR	454
#define	NEXT	455
#define	NODUP	456
#define	NONE	457
#define	NOT	458
#define	NOT_EQUAL	459
#define	NOW	460
#define	SQL_NULL	461
#define	NULLIF	462
#define	NUMERIC	463
#define	OCTET_LENGTH	464
#define	ODBC	465
#define	OF	466
#define	SQL_OFF	467
#define	SQL_ON	468
#define	ONLY	469
#define	OPEN	470
#define	OPTION	471
#define	OR	472
#define	ORDER	473
#define	OUTER	474
#define	OUTPUT	475
#define	OVERLAPS	476
#define	PAGE	477
#define	PARTIAL	478
#define	SQL_PASCAL	479
#define	PERSISTENT	480
#define	CQL_PI	481
#define	PLI	482
#define	POSITION	483
#define	PRECISION	484
#define	PREPARE	485
#define	PRESERVE	486
#define	PRIMARY	487
#define	PRIOR	488
#define	PRIVILEGES	489
#define	PROCEDURE	490
#define	PRODUCT	491
#define	PUBLIC	492
#define	QUARTER	493
#define	QUIT	494
#define	RAND	495
#define	READ_ONLY	496
#define	REAL	497
#define	REFERENCES	498
#define	REPEAT	499
#define	REPLACE	500
#define	RESTRICT	501
#define	REVOKE	502
#define	RIGHT	503
#define	ROLLBACK	504
#define	ROWS	505
#define	RPAD	506
#define	RTRIM	507
#define	SCHEMA	508
#define	SCREEN_WIDTH	509
#define	SCROLL	510
#define	SECOND	511
#define	SECONDS_BETWEEN	512
#define	SELECT	513
#define	SEQUENCE	514
#define	SETOPT	515
#define	SET	516
#define	SHOWOPT	517
#define	SIGN	518
#define	INTEGER_CONST	519
#define	REAL_CONST	520
#define	SIN	521
#define	SQL_SIZE	522
#define	SMALLINT	523
#define	SOME	524
#define	SPACE	525
#define	SQL	526
#define	SQL_TRUE	527
#define	SQLCA	528
#define	SQLCODE	529
#define	SQLERROR	530
#define	SQLSTATE	531
#define	SQLWARNING	532
#define	SQRT	533
#define	STDEV	534
#define	SUBSTRING	535
#define	SUM	536
#define	SYSDATE	537
#define	SYSDATE_FORMAT	538
#define	SYSTEM	539
#define	TABLE	540
#define	TAN	541
#define	TEMPORARY	542
#define	THEN	543
#define	THREE_DIGITS	544
#define	TIME	545
#define	TIMESTAMP	546
#define	TIMEZONE_HOUR	547
#define	TIMEZONE_MINUTE	548
#define	TINYINT	549
#define	TO	550
#define	TO_CHAR	551
#define	TO_DATE	552
#define	TRANSACTION	553
#define	TRANSLATE	554
#define	TRANSLATION	555
#define	TRUNCATE	556
#define	GENERAL_TITLE	557
#define	TWO_DIGITS	558
#define	UCASE	559
#define	UNION	560
#define	UNIQUE	561
#define	SQL_UNKNOWN	562
#define	UPDATE	563
#define	UPPER	564
#define	USAGE	565
#define	USER	566
#define	IDENTIFIER	567
#define	IDENTIFIER_DOT_ASTERISK	568
#define	ERROR_DIGIT_BEFORE_IDENTIFIER	569
#define	USING	570
#define	VALUE	571
#define	VALUES	572
#define	VARBINARY	573
#define	VARCHAR	574
#define	VARYING	575
#define	VENDOR	576
#define	VIEW	577
#define	WEEK	578
#define	WHEN	579
#define	WHENEVER	580
#define	WHERE	581
#define	WHERE_CURRENT_OF	582
#define	WITH	583
#define	WORD_WRAPPED	584
#define	WORK	585
#define	WRAPPED	586
#define	YEAR	587
#define	YEARS_BETWEEN	588
#define	ILIKE	589
#define	SIMILAR	590


extern YYSTYPE yylval;
