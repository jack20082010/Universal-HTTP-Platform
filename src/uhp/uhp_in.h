/*
 * author	: jack
 * email	: xulongzhong2004@163.com
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef _H_UHP_IN_
#define _H_UHP_IN_

#include <dirent.h>
#include <unistd.h>
#include <iconv.h>
#include <stdint.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <vector>
#include <map>
//#include <queue>
#include <set>
using namespace std;

#include "uhp_util.h"
#include "fasterjson.h"
#include "fasterhttp.h"
#include "threadpool.h"
#include "seqerr.h"
#include "IDL_httpserver_conf.dsc.h"
#include "dbsqlca.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _TYPEDEF_BOOL_
#define _TYPEDEF_BOOL_
typedef int BOOL;
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef BOOLNULL
#define BOOLNULL -1
#endif

#define HTTP_HEADER_CONTENT_TYPE_TEXT           "application/text;charset=gb18030"
#define	HTTP_HEADER_CONTENT_TYPE_JSON		"application/json"
#define SERVER_CONF_FILENAME			"uhp.conf"
#define SERVER_BEFORE_LOADCONFIG		1
#define SERVER_AFTER_LOADCONFIG			0
#define SERVER_MAX_PROCESS			1024
#define SERVER_MAX_MESSAGE_SIZE			1024*1024*4
#define SERVER_MAX_TOPIC_LEN			255

#define	SERVER_FULL_FUSING_TIME			10	/*所有机器全熔断时间*/
#define SERVER_FUSING_TIME			60	/*当台机器熔断时间*/
#define MAX_HOST_LEN	 			100

extern char		*g_ibp_httpserver_kernel_version ;
extern signed char 	g_exit_flag;
extern int 		g_process_index;

#define PROCESS_INIT			-2	/*进程初始值*/
#define PROCESS_MONITOR			-1	/*monitor守护进程，其他为子进程*/

/* 每次最大接收epoll事件数 */
#define MAX_EPOLL_EVENTS		50000
#define MAX_RESPONSE_BODY_LEN		300
#define MAX_EPOLL_THREAD		1024
#define RETRY_INDEX			0
#define ACCEPT_INDEX			1
#define SEND_EPOLL_INDEX		2

/* proxy与sdk之间的请求URI */
#define URI_SESSIONS			"/sessions"  
#define URI_THREAD_STATUS		"/thread_status"  
#define URI_CONFIG			"/config"


/*会话状态流程控制*/
#define SESSION_STATUS_RECEIVE		1
#define SESSION_STATUS_PUBLISH		2
#define SESSION_STATUS_HANG		3
#define SESSION_STATUS_SEND		4
#define SESSION_STATUS_DIE		5

#define SESSION_HNAG_TIMEOUT		1
#define SESSION_HNAG_UP			2


typedef int fn_doworker( struct AcceptedSession* );
typedef int fn_void( );
typedef int fn_exception( struct AcceptedSession*, int errcode, char *errmsg );

/* 侦听会话结构	*/
struct ListenSession
{
	struct NetAddress	netaddr	;
} ;

/* 管道会话结构	*/
struct PipeSession
{
	int			pipe_fds[ 2 ] ;
} ;

/*性能导出结构*/
struct Performamce
{
	struct timeval	tv_sdkSend;
	struct timeval	tv_accepted;
	struct timeval	tv_receive_begin;
	struct timeval	tv_receive_end;
	struct timeval	tv_publish_begin;
	struct timeval	tv_publish_end;
	struct timeval	tv_send_begin;
	struct timeval	tv_send_end;
};


/* 命令行参数结构 */
struct CommandParameter
{
	char			*_action ;
	char			*_show ;
	char			*_rotate_size ;
	char			*_main_loglevel	;
	char			*_monitor_loglevel ;
	char			*_worker_loglevel ;
	char			*_sqlcmd;
	char			*_sqlfile;
	
} ;

struct HostInfo
{
	char	ip[15];
	int	port;
	long	activeTimestamp;
};

#define PLUGIN_LOAD				"Load"
#define PLUGIN_UNLOAD				"Unload"
#define PLUGIN_DOWORKER				"Doworker"
#define PLUGIN_ONREQUEST			"OnRequest"
#define PLUGIN_ONRESPONSE			"OnResponse"
#define PLUGIN_ONEXCEPTION			"OnException"
#define PLUGIN_ONHEARTBEAT			"OnHeartbeat"

class PluginInfo
{

public:
	string 		uri;
	string    	content_type;
	int		timeout;
	string		path;
	void		*p_handle;
	fn_void		*p_fn_load;
	fn_void		*p_fn_unload;
	fn_doworker	*p_fn_doworker;	
	fn_doworker	*p_fn_onrequest;
	fn_doworker	*p_fn_onresponse;
	fn_exception	*p_fn_onexception;

	PluginInfo()
	{
		timeout = 0;
		p_handle = NULL;
		p_fn_load = NULL;
		p_fn_unload = NULL;
		p_fn_doworker = NULL;	
		p_fn_onrequest = NULL;
		p_fn_onresponse = NULL;
		p_fn_onexception = NULL;
	}

};

/* 客户端连接会话结构 */
struct AcceptedSession
{
	struct NetAddress	netaddr;
	struct HttpEnv		*http ;
	struct Performamce	perfms;
	int			status;
	int			needFree;	/*需要延迟free*/
	long			request_begin_time;
	long			accept_begin_time;
	char			*http_rsp_body; /*http返回响应体*/
	int			body_len; /*http返回响应体*/
	char			charset[10+1];
	PluginInfo  		*p_plugin;
	int			epoll_fd;
	int			epoll_fd_send;
	int			index;
	char			namespacex[80];
	int			hang_status;  /*挂起状态 hang_timout, hangup*/
	
	struct greater {
		bool operator()( const AcceptedSession *l, const AcceptedSession *r )
		{
			return l->request_begin_time > r->request_begin_time;
		}
	};
	struct less {
		bool operator()( const AcceptedSession *l, const AcceptedSession *r )
		{
			return l->request_begin_time < r->request_begin_time;
		}
	};

} ;

typedef map<string, PluginInfo> 	mapPluginInfo;
typedef vector<PluginInfo>		vecPluginInfo;
//typedef priority_queue < AcceptedSession,vector<AcceptedSession>, AcceptedSession::greater > queueSession;
typedef set < AcceptedSession*, AcceptedSession::less > setSession;

struct ThreadInfo
{
	int		epoll_fd;
	pthread_t	thread_id;
	char		cmd;		/*用于重试线程日志重载等*/
	int		index;
	pthread_mutex_t	session_lock;
	setSession	*p_set_session;
	long		last_loop_session_timestamp; /*最后一次轮询遍历session时间戳*/
};

/* proxy服务端主环境结构 */
struct HttpserverEnv
{

	struct CommandParameter		cmd_param ;
	char				*server_conf_filename ; /* proxy.conf文件名 */
	char				server_conf_pathfilename[ UTIL_MAXLEN_FILENAME + 1 ] ; /* proxy.conf路径文件名 */
	
	httpserver_conf			httpserver_conf ; /* httpserver主配置 */
	int				epoll_fd ; /* 内部IO多路复用epoll描述字 */
	char				*p_shmPtr;   /*共享内存地址*/
	int				shmid;		/*共享内存标识*/

	pthread_mutex_t			session_lock;
	struct ListenSession		listen_session ; /* 侦听会话 */
	struct PipeSession		*p_pipe_session ; /* 管道会话 */
	struct AcceptedSession		accepted_session_list ;	/* 客户端连接会话 */
	threadpool_t			*p_threadpool;
	char				lastDeletedDate[10+1] ;
	struct ThreadInfo		thread_accept; /*accept线程*/
	struct ThreadInfo		thread_epoll[MAX_EPOLL_THREAD]; /*epoll recv线程*/
	struct ThreadInfo		thread_epoll_send[MAX_EPOLL_THREAD]; /*epoll send线程*/
	long				last_loop_session_timestamp; /*最后一次轮询遍历session时间戳*/
	long				last_show_status_timestamp;  /*控制显示状态轮询时间*/
	struct NetAddress		netaddr;

	void				*dbpool_handle;		 /*输入插件句柄*/
	fn_void				*p_fn_onheartbeat_dbpool;	
	fn_void				*p_fn_load_dbpool;
	fn_void				*p_fn_unload_dbpool;
	
	void				*p_reserver1;		/*保留字段*/
	void				*p_reserver2;
	void				*p_reserver3;
	void				*p_reserver4;
	void				*p_reserver5;
	void				*p_reserver6;
	
	void				*p_dbpool;	/*数据库连接池*/
	mapPluginInfo			*p_map_plugin_output;
	vecPluginInfo			*p_vec_interceptors;
	
#if 0	
	HttpserverEnv()
	{
		memset( &cmd_param, 0, sizeof(CommandParameter) );
		server_conf_filename = NULL;
		memset( server_conf_pathfilename, 0, sizeof(server_conf_pathfilename) );
		DSCINIT_httpserver_conf( &httpserver_conf );
		epoll_fd_recv = -1;
		epoll_fd_send = -1;
		p_shmPtr = NULL;
		shmid = 0;
		memset( &listen_session, 0, sizeof(ListenSession) );
		p_pipe_session = NULL;
		memset( &accepted_session_list, 0, sizeof(AcceptedSession) );
		session_count = 0;
		p_threadpool = NULL;
		memset( lastDeletedDate, 0, sizeof(lastDeletedDate) );
		memset( thread_array, 0, sizeof(thread_array));
		last_loop_session_timestamp = 0;
		last_show_status_timestamp = 0;
		memset( &netaddr, 0, sizeof(NetAddress) );
		
		dbpool_handle = NULL;
		p_fn_doworker_dbpool = NULL;
		p_fn_load_dbpool = NULL;
		p_fn_unload_dbpool = NULL;
		
		/*保留字段*/          
		p_reserver1 = NULL;		
		p_reserver2 = NULL;
		p_reserver3 = NULL;
		p_reserver4 = NULL;
		p_reserver5 = NULL;
		p_reserver6 = NULL;
		
		p_dbpool = NULL;	/*数据库连接池*/
	}
#endif

} ;

struct ProcStatus
{
	int		process_index;
	pid_t		pid;
	int 		task_count;
	int 		working_count;
	int 		Idle_count;
	int 		timeout_count;
	int		thread_count;
	int		max_threads;
	int		min_threads;
	int		taskTimeoutPercent;
	int		session_count;
	time_t		last_restarted_time;   /*进程最后一次重启时间*/
	time_t		effect_exit_time;	/*设置进程退出标志时间*/
	
};
	
/*
 * 配置管理模块
 */

int LoadConfig( HttpserverEnv *p_env, httpserver_conf *p_conf );

/*
 * 环境管理模块
 */

int InitEnvironment( HttpserverEnv **pp_env );
int CleanEnvironment( HttpserverEnv *p_env );
int InitLogEnv( HttpserverEnv *p_env, char* module_name, int before_loadConfig );
int CleanLogEnv();
int UHPSetEnv( HttpserverEnv* p_env );


/*
 * 管理进程
 */

int _monitor( void *pv );

/*
 * 工作进程
 */

int worker( HttpserverEnv *p_env );

/*
 * 通讯层
 */

int OnAcceptingSocket( HttpserverEnv *p_env , struct ListenSession *p_listen_session );
void OnClosingSocket( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session, BOOL with_lock );
int OnReceivingSocket( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
int OnSendingSocket( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
int OnReceivePipe( HttpserverEnv *p_env , struct PipeSession *p_pipe_session );

/*
 * 应用层
 */
int RetryProducer( HttpserverEnv *p_env );
int InitWorkerEnv( HttpserverEnv *p_env );
int InitConfigFiles( HttpserverEnv *p_env );
int OnProcess( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
int OnProcessPipeEvent( HttpserverEnv *p_env , struct PipeSession *p_pipe_session );
int ThreadBegin( void *arg, int threadno );
int ThreadRunning( void *arg, int threadno );
int ThreadExit( void *arg, int threadno );
//elogLevel convert_sdkLogLevel( HttpserverEnv *p_env );
int OnProcessShowSessions( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
int OnProcessShowThreadStatus( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
int OnProcessShowConfig( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
int ExportPerfmsFile( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
int OnProcessGetTopicList( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
int ConvertReponseBody( struct AcceptedSession *p_accepted_session, char* body_convert, int body_size );

void FreeSession( struct AcceptedSession *p_accepted_session );
int AddEpollSendEvent( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
int AddEpollRecvEvent( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
int SetSessionResponse( struct AcceptedSession *p_session , int errcode, char *format , ...  );
int OnProcessAddTask( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session );
/*
 * 客户端
 */

int SetMqServers( HttpserverEnv *p_env );
int GetRemoteTopicList( HttpserverEnv *p_env );

int ShowSessions( HttpserverEnv *p_env );
int ShowThreadStatus( HttpserverEnv *p_env );
int ShowConfig( HttpserverEnv *p_env );
int ShowTopicList( HttpserverEnv *p_env );

#ifdef __cplusplus
}
#endif

#endif
