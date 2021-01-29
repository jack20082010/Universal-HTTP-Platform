set names GBK;
DROP TABLE  IF EXISTS bs_seq_rule;
CREATE TABLE  bs_seq_rule
(
	id  BIGINT NOT NULL AUTO_INCREMENT  COMMENT '增值id' ,
	seq_name VARCHAR(64) NOT NULL COMMENT '序列名称' ,
	seq_desc VARCHAR(80) COMMENT '序列描述' ,
	seq_type INT DEFAULT 0 COMMENT '默认非严格单调递增' ,
	min_val  BIGINT NOT NULL  COMMENT '最小值' ,
	max_val  BIGINT NOT NULL  COMMENT '最大值' ,
	cur_val BIGINT DEFAULT 0 NOT NULL  COMMENT '当前值' ,
	alert_val BIGINT DEFAULT 0 NOT NULL  COMMENT '告警值' ,
	step INT DEFAULT 1  COMMENT '步长' ,
	alert_diff INT DEFAULT 500  COMMENT '剩余个数告警',
	CACHE INT DEFAULT 1000 COMMENT '内存缓存个数' ,
	client_cache INT DEFAULT 100 COMMENT '客户端内存缓存个数',
	client_alert_diff INT DEFAULT 50  COMMENT '剩余个数告警',
	batch_cache INT DEFAULT 10  COMMENT 'cache队列长度,批量获取使用' ,
	batch_fetch INT DEFAULT 3   COMMENT '每次提取个数方便cache,批量获取使用' ,
	STATUS  INT DEFAULT 0 COMMENT '默认启用' ,
	cycle INT DEFAULT 1 COMMENT '默认循环' ,
	create_time DATETIME NOT NULL DEFAULT NOW() COMMENT '创建时间' ,
	update_time DATETIME NOT NULL DEFAULT NOW() ON UPDATE NOW() COMMENT '修改时间' ,
	remark  VARCHAR(100)  COMMENT '备注' ,
	PRIMARY KEY (id) 
	
);


ALTER TABLE bs_seq_rule ADD UNIQUE ( seq_name );
insert into bs_seq_rule( seq_name, min_val, max_val, cur_val ) VALUES("seq_test1",1,999999999999999999, 1 );
insert into bs_seq_rule( seq_name, min_val, max_val, cur_val, step ) VALUES("seq_test2",1,999999999999999999, 1,2 );
insert into bs_seq_rule( seq_name, min_val, max_val, cur_val ) VALUES("seq_test3",1,999999999999999999, 1 );
insert into bs_seq_rule( seq_name, min_val, max_val, cur_val ) VALUES("seq_test4",1,999999999999999999, 1 );
insert into bs_seq_rule( seq_name, min_val, max_val, cur_val ) VALUES("seq_test5",1,999999999999999999, 1 );
insert into bs_seq_rule( seq_name, min_val, max_val, cur_val ) VALUES("seq_test6",1,999999999999999999, 1 );
insert into bs_seq_rule( seq_name, min_val, max_val, cur_val ) VALUES("seq_test7",1,999999999999999999, 1 );
insert into bs_seq_rule( seq_name, min_val, max_val, cur_val ) VALUES("seq_test8",1,999999999999999999, 1 );
insert into bs_seq_rule( seq_name, min_val, max_val, cur_val ) VALUES("seq_test9",1,999999999999999999, 1 );
insert into bs_seq_rule( seq_name, min_val, max_val, cur_val ) VALUES("seq_test10",1,999999999999999999, 1 );

DROP TABLE  IF EXISTS bs_seq_test;
CREATE TABLE  bs_seq_test
(
	id  BIGINT NOT NULL   COMMENT 'seqid' ,
	seq_name VARCHAR(32) NOT NULL COMMENT '序列名称' ,
	create_time DATETIME NOT NULL DEFAULT NOW() COMMENT '创建时间' ,
	update_time DATETIME NOT NULL DEFAULT NOW() ON UPDATE NOW() COMMENT '修改时间' ,
	remark  VARCHAR(100)  COMMENT '备注' ,
	PRIMARY KEY (id, seq_name) 
	
);
