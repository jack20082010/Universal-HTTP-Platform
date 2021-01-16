
# 高性能HUP服务平台设计

##	背景
随着云原生的兴起，微服务在各行各业广泛的应用，http通信协议已经成为微服务通信的主流协议。如果每个应用都自己建设个http服务，存在重复建设，建设难度大和后续维护成本高的问题，所以建设一个通用高性能http服务框架，如何编写少量代码就能实现高性能应用http服务，成为大家的广泛需求。
##	目标
1. 实现高性能通用http服务平台，采用C/C++高性能语言编码。使用linux epoll多路复用，使用reuseport特性，内核版本必须3.9以上，多个子进程可用监听同一个端口，源头上解决由父进程监听子进程继承导致的惊群现象。
2. accept、recv、send 线程分开进一步提升连接、接收、发送数据的性能。
3. 支持多进程+多线程方式启动，解决单进程epoll调度带来的缺陷。
4. 自实现线程池，添加任务的时候，自动唤醒线程池中的一个空闲线程进行业务处理。当任务添加过多时，自动扩展线程直到配置最大线程，空闲时慢慢释放过剩线程直到配置最小线程。
5. 实现高性能数据库访问插件，采用变量绑定执行sql，无需多次解析sql。支持批量获取数据和批量插入。支持数据库连接池访问，支持动态扩展和延迟释放。
6. 输入插件：http通信必要字段检查
7. 输出插件：实现业务处理逻辑
8. 平台对外服务的api插件，方便输入和输出插件与平台交换的调用。
9. 自带序列中心插件，支持多级缓存架构，性能秒杀主流开源序列，服务端可以动态调整客户缓存参数，提高客户端性能。

##	设计思路
###	平台架构
1. 总体架构

![](https://github.com/xulongzhong/Universal-HTTP-Plantform/blob/main/image/uhp.bmp)

2. 模块说明
Uhp server 通用http服务平台
Uhp-sdk：http服务客户端；
Plugin_intput：输入插件，用于业务报文必要元素检查;
Task queue：接收的报文的任务队列。
Thread pool: 业务处理线程池
Plugin_output：输出插件，实现业务处理逻辑
DB: 业务处理相关持久化数据

3. 流程说明
应用通过uhp-sdk 发起http请求，Uhp server通过epoll线程不断接收数据，当请求报文接收完成时，先调用plgin_input插件处理相关业务报文数据，如果报文参数不合法，直接报不合法信息报文响应给uhp-sdk客户端。如果报文合法则调用线程池的addTask接口增加任务到线程池的task-queue队列，当任务列表不为空时，唤醒线程池中的任意线程通过callback函数回调plugin_output输出插件，进行相关业务处理，业务处理完成后把响应报文添加到epoll发送队列，最后由epoll线程把响应报文返回给uhp-sdk客户端。
###     数据库访问插件
使用MySQL C API中Stmt 预处理接口，采用变量绑定执行sql，无需多次解析sql，是目前执行效率最高的方式。支持批量获取数据和批量插入。

1.Select案例

```
int test_select()
{
	int 		nret;
	char		sql[100];
	STSequence      STSeq = {0};
      CDbSqlca * pDbSqlca = new CDbSqlca ;  
        nret = pDbSqlca->Connect( "mysqldb", "mysqldb", "192.168.56.129", "mysqldb", 3306 );
        if( nret )
        {
        	printf("获取数据库连接失败：errCode[%d] errMsg[%s]\n", pDbSqlca->GetDbErrorCode(), pDbSqlca->GetDbErrorMsg() );
		return -1;
        }
     nret = pDbSqlca->BindIn( 1, "seq_test" );
	nret = pDbSqlca->BindOut( 1, "%lld", &STSeq.id );
	nret = pDbSqlca->BindOut( 2, "%s", STSeq.seq_name );
	nret = pDbSqlca->BindOut( 3, "%s", STSeq.seq_desc );
	nret = pDbSqlca->BindOut( 4, "%ld", &STSeq.seq_type );
	nret = pDbSqlca->BindOut( 5, "%lld", &STSeq.min_val );
	nret = pDbSqlca->BindOut( 6, "%lld", &STSeq.max_val );
	nret = pDbSqlca->BindOut( 7, "%lld", &STSeq.cur_val );
	nret = pDbSqlca->BindOut( 8, "%d", &STSeq.step );
	nret = pDbSqlca->BindOut( 9, "%d", &STSeq.cycle );
	nret = pDbSqlca->BindOut( 10, "%d", &STSeq.status );
	nret = pDbSqlca->BindOut( 11, "%s", STSeq.create_time );
	nret = pDbSqlca->BindOut( 12, "%s", STSeq.update_time );
	nret = pDbSqlca->BindOut( 13, "%s", STSeq.remark );
	
	nret = pDbSqlca->ExecSql( "select id, seq_name, seq_desc, seq_type, min_val, max_val, cur_val, step, cycle, \
				status, create_time, update_time, remark   from bs_seq_rule where  seq_name = ? " );
	nret = pDbSqlca->ExecSql( sql );
	printf("id[%lld]\n", STSeq.id);
	printf("seq_name[%s]\n", STSeq.seq_name);
	printf("seq_desc[%s]\n", STSeq.seq_desc);
	printf("seq_type[%d]\n", STSeq.seq_type);
	printf("min_val[%lld]\n", STSeq.min_val);
	printf("max_val[%lld]\n", STSeq.max_val);
	printf("cur_val[%lld]\n", STSeq.cur_val);
	printf("step[%d]\n", STSeq.step);
	printf("cycle[%d]\n", STSeq.cycle);
	printf("status[%d]\n", STSeq.status);
	printf("create_time[%s]\n", STSeq.create_time);
	printf("update_time[%s]\n", STSeq.update_time);
	printf("remark[%s]\n", STSeq.remark);
	
	if( nret < 0 )
	{
		printf("ExecSql[%s] failed errCode[%d] errMsg[%s]\n", sql,  pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		return -1;
	}
	
	delete pDbSqlca;
	
	return 0;
}

```
2.Insert案例
```
int test_insert( char* seq_name, unsigned long long seq_val )
{
	char			sql[200];
	CDbSqlca 		*pDbSqlca = NULL ;
	int 			nret;
	
	pDbSqlca = m_pdbpool->GetIdleDbSqlca( );
	if( !pDbSqlca )
	{
		printf("获取数据库连接失败：errCode[%d] errMsg[%s]", m_pdbpool->GetLastErrCode(), m_pdbpool->GetLastErrMsg() );
		return -1;
	}
	
	strncpy( sql, "insert into bs_seq_test( id, seq_name ) values( ?,? ) ", sizeof(sql)-1 );
	pDbSqlca->BindIn( 1,  seq_val );
	pDbSqlca->BindIn( 2, seq_name );
	nret = pDbSqlca->ExecSql( sql );
	if( nret < 0 )
	{
		printf("Sql[%s] seq_name[%s] seq_val[%llu] failed errCode[%d] errMsg[%s]\n", sql,  seq_name, seq_val, pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
	
	pDbSqlca->Commit();
	m_pdbpool->ReleaseDbsqlca( pDbSqlca );
	
	return 0;
}

```
3.Update案例
```
int test_updte( char *name, StrSeqAtrrMap *p_seqMap )
{
	int 			nret;
	char			update_sql[100];
	unsigned long long 	update_val = 0;
	
	int			errCode;
	CDbSqlca 		*pDbSqlca = NULL ;
	
	pDbSqlca = m_pdbpool->GetIdleDbSqlca( );
	if( !pDbSqlca )
	{
		printf("获取数据库连接失败：errCode[%d] errMsg[%s]", m_pdbpool->GetLastErrCode(), m_pdbpool->GetLastErrMsg() );
		return -1;
	}
        
        /*持久化到数据库*/
	memset( update_sql, 0, sizeof(update_sql) );
	strncpy( update_sql, "update bs_seq_rule set cur_val = ? where seq_name = ? ", sizeof(update_sql)-1 );
	pDbSqlca->BindIn( 1,  update_val );
	pDbSqlca->BindIn( 2,  p_seq->name );
	nret = pDbSqlca->ExecSql( update_sql );
	if( nret < 0 )
	{
		printf("sql[%s] seq_name[%s] failed errCode[%d] errMsg[%s]\n", update_sql,  p_seq->name, pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
		
	pDbSqlca->Commit();
	m_pdbpool->ReleaseDbsqlca( pDbSqlca );
	
	return 0;
}


```
4.Cursor游标案例
```
int test_cursor( char *name, StrSeqAtrrMap *p_seqMap )
{
	int 			nret;
	char			sql[200];
	char			update_sql[100];
	unsigned long long 	cur_val = 0;
	unsigned long long 	max_val = 0;
	unsigned long long 	min_val = 0;
	unsigned long long 	update_val = 0;
	int			alert_diff = 0;
	int			cache = 0;
	int			step = 0;
	int 			cycle = 0;
	SeqAtrr			atr_seq;
	int			errCode;
	CDbSqlca 		*pDbSqlca = NULL ;
	
	pDbSqlca = m_pdbpool->GetIdleDbSqlca( );
	if( !pDbSqlca )
	{
		printf("获取数据库连接失败：errCode[%d] errMsg[%s]", m_pdbpool->GetLastErrCode(), m_pdbpool->GetLastErrMsg() );
		return -1;
	}
        
      memset( sql, 0, sizeof(sql) );
	strncpy( sql, "select seq_name, status, cycle, cur_val, max_val, min_val, step, cache, alert_diff from bs_seq_rule for update ", sizeof(sql)-1 );
		
	pDbSqlca->BindOut( 1, "%s", atr_seq.name );
	pDbSqlca->BindOut( 2, "%d", &atr_seq.status );
	pDbSqlca->BindOut( 3, "%d", &cycle );
	pDbSqlca->BindOut( 4, "%llu", &cur_val );
	pDbSqlca->BindOut( 5, "%llu", &max_val );
	pDbSqlca->BindOut( 6, "%llu", &min_val );
	pDbSqlca->BindOut( 7, "%d", &step );
	pDbSqlca->BindOut( 8, "%d", &cache );
	pDbSqlca->BindOut( 9, "%d", &alert_diff );
	
	void *pStmthandle = pDbSqlca->OpenCursor( sql );
	if( pStmthandle == NULL )
	{
		printf("Sql[%s] OpenCursor failed errCode[%d] errMsg[%s]\n", sql,  pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
	while( 1 )
	{
		int nAffectRows = pDbSqlca->FetchData( 1, pStmthandle );
		if( nAffectRows < 0 )
		{
			printf("Sql[%s] FetchData failed errCode[%d] errMsg[%s]\n", sql,  pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
			pDbSqlca->Rollback();
			pDbSqlca->CloseCursor( pStmthandle );
			m_pdbpool->ReleaseDbsqlca( pDbSqlca );
			return -1;
		}
		
		errCode = pDbSqlca->GetDbErrorCode();
		if( errCode == SQLNOTFOUND )
		{
			printf("break sql[%s]\n", sql);
			break;
		}
	
		//todo
		
	}
		
	pDbSqlca->CloseCursor( pStmthandle );
	pDbSqlca->Commit();
	m_pdbpool->ReleaseDbsqlca( pDbSqlca );
	
	return 0;
}

```
### 相关服务插件化
1. Uhp是通用的http服务框架，要实现相关服务只需要开发相关输入、输出插件即可。
标准插件接口：
2. int Load();            // uhp启动时动态加载，初始化相关资源
3. int Unload();			// uhp程序退出时卸载调用，释放相关初始化资源
4. int Doworker( struct AcceptedSession *p_session );  //uhp线程池处理请求相关业务

####   序列中心插件
采用服务端、客户端多机缓存模式，每个序列独立一把锁，不同序列访问互不竞争，最大程序提高序列的并发性能。
支持单笔获取和批量获取模式，适应不同业务需求，支持控制台修改缓存数量，客户端无需重启服务就能提高序列的性能。

####  数据模型设计

* 表结构字段说明

|字段名称|中文说明|字段类型|约束|备注|
|---|---|---|---|---|
id		|增值id		|BIGINT		|NOT NULL	|主键
seq_name	|序列名称	|VARCHAR (64) 	|NOT NULL	|唯一索引
seq_desc	|序列描述	|VARCHAR (80)	|	        |
seq_type	|序列类型	|INT		|DEFAULT 0	|默认值0
min_val		|最小值		|BIGINT		|NOT NULL	|
max_val		|最大值		|BIGINT		|NOT NULL	|
cur_val		|当前值		|BIGINT		|DEFAULT 0	|默认值0
step		|增长步长	|INT		|DEFAULT 1	|默认值1
alert_diff	|服务端告警值	|INT		|DEFAULT 500	|默认值500，少于500触发异步获取
cache		|服务端缓存个数	|INT		|DEFAULT 1000	|一次获取个数
client_cache	|客户端缓存个数	|INT		|DEFAULT 100	|一次获取个数
client_alert_diff| 客户端告警值	|INT		|DEFAULT 50	|默认值50，少于100触发异步获取
batch_cache	|cache队列长度	|INT		|DEFAULT 10	|cache队列长度,批量获取使用
batch_fetch	|每次提取cache数|INT		|DEFAULT 3	|每次提取个数方便cache,批量获取使用
status		|状态		|INT		|DEFAULT 0	|	默认启用
cycle		|是否循环	|INT		|DEFAULT 1	|默认循环
create_time	|创建时间	|DATETIME 	|DEFAULT NOW()	|默认值当前时间
update_time	|修改时间	|DATETIME 	|DEFAULT NOW()	|默认值当前时间
remark		|备注		|VARCHAR(100)	|		|

* sql脚本自带性能测试案例
```
DROP TABLE bs_seq_rule;
CREATE TABLE  bs_seq_rule
(
	id  BIGINT NOT NULL AUTO_INCREMENT  COMMENT '增值id' ,
	seq_name VARCHAR(64) NOT NULL COMMENT '序列名称' ,
	seq_desc VARCHAR(80) COMMENT '序列描述' ,
	seq_type INT DEFAULT 0 COMMENT '默认非严格单调递增' ,
	min_val  BIGINT NOT NULL  COMMENT '最小值' ,
	max_val  BIGINT NOT NULL  COMMENT '最大值' ,
	cur_val BIGINT DEFAULT 0 NOT NULL  COMMENT '当前值' ,
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

DROP TABLE bs_seq_test;
CREATE TABLE  bs_seq_test
(
	id  BIGINT NOT NULL   COMMENT 'seqid' ,
	seq_name VARCHAR(32) NOT NULL COMMENT '序列名称' ,
	create_time DATETIME NOT NULL DEFAULT NOW() COMMENT '创建时间' ,
	update_time DATETIME NOT NULL DEFAULT NOW() ON UPDATE NOW() COMMENT '修改时间' ,
	remark  VARCHAR(100)  COMMENT '备注' ,
	PRIMARY KEY (id, seq_name) 
	
);

```	

####	序列中心输入插件
对输入报文进行合法性检查。
```
int Cplugin::Doworker( struct AcceptedSession *p_session )

{	char		*p_content = NULL;
	int		value_len;
	char		nret;
	char		*p_uri = NULL;
	int		uri_len;
	struct HttpEnv  *p_httpEnv = NULL;
	char		*p_response = NULL;
	char		err_msg[256];
	
	memset( err_msg, 0, sizeof(err_msg) );
	p_httpEnv = GetSessionHttpEnv( p_session );
	p_response = GetSessionResponse( p_session );
	p_uri = GetHttpHeaderPtr_URI( p_httpEnv , & uri_len );
	INFOLOGSG( "uri[%.*s]" , uri_len , p_uri );
	
	p_content = QueryHttpHeaderPtr( p_httpEnv , HTTP_HEADER_CONTENT_TYPE , & value_len );
	if( !STRNICMP( p_content , == , HTTP_HEADER_CONTENT_TYPE_JSON , sizeof(HTTP_HEADER_CONTENT_TYPE_JSON) -1 ) )
	{
		snprintf( err_msg, sizeof(err_msg)-1, "content_type[%.*s] Is InCorrect" , value_len , p_content );
		ERRORLOGSG( err_msg );
		SET_ERROR_RESPONSE( p_response, -1, err_msg );
		return -1;
	}

	if( uri_len == sizeof(URI_SEQUENCE)-1 && STRNICMP( p_uri , == , URI_SEQUENCE , uri_len ) )
	{
		//正常处理
	}
	else
	{
		snprintf( err_msg, sizeof(err_msg)-1, "uri unknown uri[%.*s]" , uri_len , p_uri );
		ERRORLOGSG( err_msg  ); 
		SET_ERROR_RESPONSE( p_response, -1, err_msg );
		return -1;
	}
	
	return 0;
}

```
####	序列中心输出插件
1. 单个获取流程

![](https://github.com/xulongzhong/Universal-HTTP-Plantform/blob/main/image/one_seq.bmp)

2. 批量获取流程

![](https://github.com/xulongzhong/Universal-HTTP-Plantform/blob/main/image/bat_seq.bmp)

3.客户端libuhp_sdk.so插件

* SDK加载初始化时设置后台多个序列中心服务地址和端口，自带负载均均衡机制，当某一台服务不可用时，
会进行熔断一段时间，然后自动转连到其他服务。后台服务支持分布式多活部署，保证了应用获取序列的高可用。

* 获取api接口

unsigned long long GetSequence( char* name ) ;

* 获取流程

![](https://github.com/xulongzhong/Universal-HTTP-Plantform/blob/main/image/sdk_seq.bmp)

####	请求报文样例
```
POST /sequence HTTP/1.1^M
Content-Type: application/json^M
User-Agent: PostmanRuntime/7.26.8^M
Accept: */*^M
Postman-Token: d9354e84-a7f6-4ad7-befd-a6644dfdd9a8^M
Host: 192.168.56.129:10608^M
Accept-Encoding: gzip, deflate, br^M
Connection: keep-alive^M
Content-Length: 35^M
^M
{ "name": "seq_test1", "count": 1 } 

```
####	响应报文样例
```
HTTP/1.1 200 OK^M
Connection: keep-alive^M
Content-Type: application/jsonutf-8^M
Content-length: 98^M
^M
{ "errorCode":"0", "value":609705, "count":1, "step":1, "client_cache":0, "client_alert_diff":50 }

```
## 编译
先安装mysql客户端开发包
```
yum install mysql-community-devel.x86_64
sh makeinstall.sh 
```
编译后安装到$HOME/bin、$HOME/shbin、$HOME/include、$HOME/lib等目录
## 安装
* 添加当前用户的环境变量
```
PATH=$PATH:$HOME/.local/bin:$HOME/bin:$HOME/shbin
export PATH

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HOME/lib
export LANG=zh_CN.GB18030
ulimit -c unlimited
```
* 生成初始化配置文件uhp.conf
```
[dep_xlz@localhost etc]$ uhp -a init
WARN : /home/dep_xlz/etc exist !
/home/dep_xlz/etc/uhp.conf created

```

* uhp.conf配置文件说明
```
{
	"httpserver" : 
	{
		"server" : 
		{
			"ip" : "192.168.56.129" ,
			"port" : "10608" ,
			"listenBacklog" : 1024 ,
			"processCount" : 1 ,        //uhp启动进程数
			"restartWhen" : "" ,        //定时重启时间 如："03:58"，防止出入输出插件长时间运行内存泄漏等问题
			"maxTaskcount" : 1000000 ,  //服务端支持最大连接数
			"taskTimeoutPercent" : 95 , //业务处理超时线程占整个线程池的比例，如果超过95，服务自动重启，防止服务处于僵死状态
			"perfmsEnable" : 1 ,        //导出性能日志，编译后续性能分析
			"totalTimeout" : 60 ,       //业务处理超时时间
			"keepAliveIdleTimeout" : 300 , //长连接空闲时保持时间，超过这时间服务端关闭连接
			"keepAliveMaxTime" : 1800 , //长连接最大保持实际，超过这个时间，通知客户端断开连接，重新连接。
			"showStatusInterval" : 10 , //定时线程工作日志输出
			"maxChildProcessExitTime" : 10 , //守护退出时，等待子进程最大时间，超过时间被强杀回收
			"maxHttpResponse" : 1024 , //http响应缓冲区内存大小
			"pluginInputPath" : "/home/dep_xlz/lib/libsequence_input.so" , //输入插件路径
			"pluginOutputPath" : "/home/dep_xlz/lib/libseqmysql_output.so" //输出插件路径
		} ,
		"threadpool" : 
		{
			"minThreads" : 10 ,       //最下线程数
			"maxThreads" : 100 ,      //最大线程数
			"taskTimeout" : 60 ,      //线程处理超时时间
			"threadWaitTimeout" : 2 , //默认2秒，线程池线程最大等待唤醒时间，主要用于日志输出，查看休眠线程是否处于僵死状态。
			"threadSeed" : 5          //添加任务时增加线程因子，任务个数是改因子倍数数才增加线程。
		} ,
		"log" : 
		{
			"rotate_size" : "10MB" ,  //日志转档大小
			"main_loglevel" : "INFO" , 
			"monitor_loglevel" : "INFO" , //守护进程日志等级
			"worker_loglevel" : "INFO"    //工作进程日志等级
		} ,
		"plugin" :   //保留字段，主要用户输入、输出插件配置。以下序列中心插件为例：
		{
			"int1" : 3306 ,         
			"int2" : 10 ,            //异步获取最小线程池大小
			"int3" : 100 ,           //异步获取最大线程池大小
			"int4" : 0 ,
			"int5" : 0 ,
			"int6" : 0 ,
			"str301" : "mysqldb" ,   //数据库用户名
			"str302" : "mysqldb" ,   //数据库密码
			"str501" : "192.168.56.129" , //数据库地址
			"str502" : "mysqldb" ,        //数据库dbname
			"str801" : "" ,
			"str802" : "" ,
			"str1281" : "" ,
			"str1282" : "" ,
			"str2551" : "" ,
			"str2552" : ""
		}
	}
}

```
* uhp服务启动
```
[dep_xlz@localhost etc]$ uhp.sh start
server_conf_pathfilename[/home/dep_xlz/etc/uhp.conf]
uhp start ok
dep_xlz   46598      1  0 12:02 ?        00:00:00 uhp -f uhp.conf -a start
dep_xlz   46600  46598  2 12:02 ?        00:00:00 uhp -f uhp.conf -a start
```
* uhp服务停止
```
[dep_xlz@localhost etc]$ uhp.sh stop
dep_xlz   41061      1  0 01:46 ?        00:00:03 uhp -f uhp.conf -a start
dep_xlz   41063  41061  0 01:46 ?        00:01:23 uhp -f uhp.conf -a start
cost time 1 41061 wait closing
cost time 2 41061 wait closing
uhp end ok
```
## uhp-sdk调用demo
```
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "dbsqlca.h"
#include "dbutil.h"
#include "uhp_sdk.h"

CDbSqlcaPool 		*m_pdbpool = NULL;

int test_insert( char* seq_name, unsigned long long seq_val )
{
	char			sql[200];
	CDbSqlca 		*pDbSqlca = NULL ;
	int 			nret;
	
	pDbSqlca = m_pdbpool->GetIdleDbSqlca( );
	if( !pDbSqlca )
	{
		printf("获取数据库连接失败：errCode[%d] errMsg[%s]", m_pdbpool->GetLastErrCode(), m_pdbpool->GetLastErrMsg() );
		return -1;
	}
	
	strncpy( sql, "insert into bs_seq_test( id, seq_name ) values( ?,? ) ", sizeof(sql)-1 );
	pDbSqlca->BindIn( 1,  seq_val );
	pDbSqlca->BindIn( 2, seq_name );
	nret = pDbSqlca->ExecSql( sql );
	if( nret < 0 )
	{
		printf("Sql[%s] seq_name[%s] seq_val[%llu] failed errCode[%d] errMsg[%s]\n", sql,  seq_name, seq_val, pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
	
	pDbSqlca->Commit();
	m_pdbpool->ReleaseDbsqlca( pDbSqlca );
	
	return 0;
}

void*  Thread_working( void *arg )
{
	int 	i ;
	int	nret;
	int	thread_no=0;                            
	unsigned long long seq_val = 0;
	char	seq_name[65];
	
	for( i = 0; i < 100000; i++ )
	{
	        thread_no = i%10;
	   
		memset( seq_name, 0, sizeof(seq_name) );
		sprintf( seq_name, "seq_test%d", thread_no+1 );
		
		seq_val = GetSequence( seq_name );
		if( seq_val <= 0 )
		{
			printf("GetSequence failed GetSequence[%llu]\n", seq_val);
			return NULL;
		}
	
		nret = test_insert( seq_name, seq_val );
		if( nret  )
		{
			printf("GetBatSequence failed GetSequence[%llu]\n", seq_val);
			return NULL;
		}			

		printf("Tid[%ld] start_val[%llu]\n", pthread_self(), seq_val );
	}

     return 0;
}

int main()
{
	unsigned long long seq_val = 0;
	int nret;
	int i;
	pthread_t ntid;
	CDbSqlcaPool 	*p_pool = NULL;
	
	nret = SetServerUrl("192.168.56.129:10608");
	if( nret )
	{
		printf("SetServerUrl failed nret[%d]\n", nret );
		return -1;
	}
	SetConnectTimeout( 10 );
	
	p_pool = new CDbSqlcaPool( "mysqldb", "mysqldb", "192.168.56.129", "mysqldb", 3306 ); 
	if( !p_pool )
	{
		printf( "new CDbSqlcaPool failed" );
	}
	
	p_pool->SetMinConnections( 1 );
	p_pool->SetMaxConnections( 100 );
	nret = p_pool->ConnectDB( 1 );
	if( nret )
	{
		printf( "ConnectDB failed errCode[%d] errMsg[%s]\n", p_pool->GetLastErrCode(), p_pool->GetLastErrMsg() );
		delete p_pool;
		return -1;
	}
	m_pdbpool = p_pool;
	
	for( i = 0; i < 32; i++ )
	{
		pthread_create( &ntid, NULL, Thread_working, &i );
	}  

	printf("end\n");
	      
	getchar();
		
	return 0;
}

```