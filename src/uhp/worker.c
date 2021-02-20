/*
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */
#include <sys/prctl.h>
#include "uhp_in.h"
#include "uhp_api.h"

#define			SESSION_TIMEOUT_SEED		1.5	/*超时因子*/

int RecvEpollWorker( void *arg );
int SendEpollWorker( void *arg );

void* AcceptWorker( void *arg )
{
	HttpserverEnv 	*p_env = NULL;
	struct pollfd 		poll_fd;
	
	int			nret;
	
	p_env = ( HttpserverEnv* )arg;
	p_env->thread_accept.cmd = 'I';
	prctl( PR_SET_NAME, "Accept" );
	
	while( ! g_exit_flag )
	{
		if( p_env->thread_accept.cmd == 'I' ||  p_env->thread_accept.cmd == 'L' ) /*线程内应用初始化*/
		{
			char	module_name[50];
			
			memset( module_name, 0, sizeof(module_name) );
			snprintf( module_name, sizeof(module_name) - 1, "worker_%d", g_process_index+1 );
			/* 设置日志环境 */
			InitLogEnv( p_env, module_name, SERVER_AFTER_LOADCONFIG );
			
			if( p_env->thread_accept.cmd == 'I' )
			{
				INFOLOGSG(" InitLogEnv module_name[%s] ok", module_name );
			}
			else
			{
				INFOLOGSG(" reload config module_name[%s] ok", module_name );
			}
			
			p_env->thread_accept.cmd = 0;
		}
		
		if( getppid() == 1 )
		{
			INFOLOGSG("parent process is exit");
			break;
		}
		
		/*检测是否有新连接建立*/
		poll_fd.fd = p_env->listen_session.netaddr.sock;
		poll_fd.events = POLLIN;
		nret = poll( &poll_fd , 1 , 1000 );
		if( nret < 0 )
		{
			ERRORLOGSG( "poll failed[%d]" , nret );
			g_exit_flag = 1;
			break;
		}
		else if( nret == 0 )
		{
			INFOLOGSG( "accept poll timeout" );
			continue;
		}
		else
		{
			INFOLOGSG( "accept poll ok nret[%d]", nret );
		}

		if( poll_fd.revents & ( POLLERR | POLLNVAL | POLLHUP ) )
		{
			INFOLOGSG("parent process is exit");
			break;
		}
		else if( poll_fd.revents & POLLIN )
		{
			nret = OnAcceptingSocket( p_env, &( p_env->listen_session ) );
			if( nret < 0 )
			{
				g_exit_flag = 1;
				FATALLOGSG( "OnAcceptingSocket failed[%d]" , nret );
				break;
			}
			else if( nret > 0 )
			{
				INFOLOGSG( "OnAcceptingSocket return[%d]" , nret );
			}
			else
			{
				INFOLOGSG( "OnAcceptingSocket ok" );
			}
			
		}
		else 
		{
			DEBUGLOGSG( "revents[%d] POLLIN[%d] POLLOUT[%d] nret[%d]", poll_fd.revents, POLLIN, POLLOUT, nret );
		}
		
	}
	
	CleanLogEnv();
	INFOLOGSG(" AccepteWorker thread exit" );
	
	return 0;
}

static int GetSessionCount( HttpserverEnv *p_env )
{
	size_t count = 0;
	
	for( int i = 0; i < p_env->httpserver_conf.httpserver.server.epollThread; i++ )
	{
		count += p_env->thread_epoll[i].p_set_session->size();
	}
	
	return (int)count;
}

static int UpdateWorkingStatus( HttpserverEnv *p_env )
{
	
	struct ProcStatus *p_proc = (struct ProcStatus*)(p_env->p_shmPtr);
	
	p_proc[g_process_index].process_index = g_process_index;
	p_proc[g_process_index].task_count = threadpool_getTaskCount( p_env->p_threadpool );
	p_proc[g_process_index].working_count = threadpool_getWorkingCount( p_env->p_threadpool );
	p_proc[g_process_index].Idle_count = threadpool_getIdleCount( p_env->p_threadpool );
	p_proc[g_process_index].thread_count = threadpool_getThreadCount( p_env->p_threadpool );
	
	p_proc[g_process_index].timeout_count = threadpool_getWorkingTimeoutCount( p_env->p_threadpool );
	p_proc[g_process_index].min_threads = threadpool_getMinThreads( p_env->p_threadpool );
	p_proc[g_process_index].max_threads = threadpool_getMaxThreads( p_env->p_threadpool );
	p_proc[g_process_index].taskTimeoutPercent =( 1.0*p_proc[g_process_index].timeout_count / p_proc[g_process_index].max_threads ) * 100 ;
	p_proc[g_process_index].session_count = GetSessionCount( p_env );
	
	/*当前正在执行任务超时比例大于百分之95，
	当前线程池处于僵死状态，需要重启服务*/
	if( p_proc[g_process_index].taskTimeoutPercent > p_env->httpserver_conf.httpserver.server.taskTimeoutPercent )
	{
		ERRORLOGSG("当前任务超时比例[%d]大于配置阀值[%d],强制重启服务！", p_proc[g_process_index].taskTimeoutPercent, p_env->httpserver_conf.httpserver.server.taskTimeoutPercent );
		exit(7);
	}
	
	INFOLOGSG("process[%d] UpdateWorkingStatus ok", g_process_index );
	
	return 0;
}

static int InitPluginDB( HttpserverEnv *p_env )
{
	int	nret = 0 ;
	char	*error = NULL;
	
	//数据库插件
	if( p_env->httpserver_conf.httpserver.database.path[0] && p_env->httpserver_conf.httpserver.database.ip[0] && p_env->httpserver_conf.httpserver.database.port > 0 )
	{
		p_env->dbpool_handle = dlopen( p_env->httpserver_conf.httpserver.database.path, RTLD_NOW );
		if( p_env->dbpool_handle == NULL )
		{
			error = dlerror();
			ERRORLOGSG( "dlopen failed: errno[%d] error[%s]", errno, error );
			return -1;
		}
		
		dlerror();
		p_env->p_fn_load_dbpool = (fn_void*)dlsym( p_env->dbpool_handle, PLUGIN_LOAD );
		error = dlerror();
		if( p_env->p_fn_load_dbpool == NULL || error )
		{
			ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", p_env->httpserver_conf.httpserver.database.path, PLUGIN_LOAD, errno, error );
			return -1;
		}
	
		dlerror();
		p_env->p_fn_unload_dbpool = (fn_void*)dlsym( p_env->dbpool_handle, PLUGIN_UNLOAD );
		error = dlerror();
		if( p_env->p_fn_unload_dbpool == NULL || error )
		{
			ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", p_env->httpserver_conf.httpserver.database.path, PLUGIN_UNLOAD, errno, error );
			return -1;
		}
		
		dlerror();
		p_env->p_fn_onheartbeat_dbpool = (fn_void*)dlsym( p_env->dbpool_handle, PLUGIN_ONHEARTBEAT );
		error = dlerror();
		if( p_env->p_fn_onheartbeat_dbpool == NULL || error )
		{
			ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", p_env->httpserver_conf.httpserver.database.path, PLUGIN_ONHEARTBEAT, errno, error );
			return -1;
		}
			
		nret = p_env->p_fn_load_dbpool();
		if( nret )
		{
			ERRORLOGSG( "path[%s]插件装载初始化失败[%s] errno[%d] error[%s]", p_env->httpserver_conf.httpserver.database.path, PLUGIN_LOAD, errno, error );
			return -1;
		}
		INFOLOGSG( "DB插件装载初始化成功" );
	}
	
	return 0;
}

static int InitInterceptors( HttpserverEnv *p_env )
{
	int	nret = 0 ;
	char	*error = NULL;
	int	i;
	
	//加载拦截器插件到vector
	for( i = 0; i <  p_env->httpserver_conf.httpserver._interceptors_count; i++ )
	{
		if( p_env->httpserver_conf.httpserver.interceptors[i].path[0] != 0 )
		{
			PluginInfo 	plugin;
			
			plugin.path = p_env->httpserver_conf.httpserver.interceptors[i].path;
			plugin.p_handle = dlopen( plugin.path.c_str(), RTLD_NOW );
			if( plugin.p_handle == NULL )
			{
				error = dlerror();
				ERRORLOGSG( "dlopen failed: errno[%d] error[%s]", errno, error );
				return -1;
			}
			
			dlerror();
			plugin.p_fn_load = (fn_void*)dlsym( plugin.p_handle, PLUGIN_LOAD );
			error = dlerror();
			if( plugin.p_fn_load == NULL || error )
			{
				ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", plugin.path.c_str(), PLUGIN_LOAD, errno, error );
				return -1;
			}
			
			dlerror();
			plugin.p_fn_unload = (fn_void*)dlsym( plugin.p_handle, PLUGIN_UNLOAD );
			error = dlerror();
			if( plugin.p_fn_unload == NULL || error )
			{
				ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", plugin.path.c_str(), PLUGIN_UNLOAD, errno, error );
				return -1;
			}
			
			dlerror();
			plugin.p_fn_onrequest = (fn_doworker*)dlsym( plugin.p_handle, PLUGIN_ONREQUEST );
			error = dlerror();
			if( plugin.p_fn_onrequest == NULL || error )
			{
				ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", plugin.path.c_str(), PLUGIN_ONREQUEST, errno, error );
				return -1;
			}
			
			dlerror();
			plugin.p_fn_onresponse = (fn_doworker*)dlsym( plugin.p_handle, PLUGIN_ONRESPONSE );
			error = dlerror();
			if( plugin.p_fn_onresponse == NULL || error )
			{
				ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", plugin.path.c_str(), PLUGIN_ONRESPONSE, errno, error );
				return -1;
			}
			
			nret = plugin.p_fn_load();
			if( nret )
			{
				ERRORLOGSG( "path[%s]插件装载初始化失败[%s] errno[%d] error[%s]", plugin.path.c_str(), PLUGIN_LOAD, errno, error );
				return -1;
			}
			INFOLOGSG( "path[%s]插件装载初始化成功", plugin.path.c_str() );
			
			p_env->p_vec_interceptors->push_back( plugin );
		}
	}
	
	return 0;
}

static int InitPluginOutput( HttpserverEnv *p_env )
{
	int	nret = 0 ;
	char	*error = NULL;
	int	i;
	
	//加载输出插件到map
	for( i = 0; i < p_env->httpserver_conf.httpserver._outputPlugins_count ; i++ )
	{
		if( p_env->httpserver_conf.httpserver.outputPlugins[i].path[0] != 0 )
		{
			PluginInfo 	plugin; 
			  
			plugin.path = p_env->httpserver_conf.httpserver.outputPlugins[i].path;
			plugin.p_handle = dlopen( plugin.path.c_str(), RTLD_NOW );
			if( plugin.p_handle == NULL )
			{
				error = dlerror();
				ERRORLOGSG( "dlopen failed: errno[%d] error[%s]", errno, error );
				return -1;
			}
			
			dlerror();
			plugin.p_fn_load = (fn_void*)dlsym( plugin.p_handle, PLUGIN_LOAD );
			error = dlerror();
			if( plugin.p_fn_load == NULL || error )
			{
				ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", plugin.path.c_str(), PLUGIN_LOAD, errno, error );
				return -1;
			}
			
			dlerror();
			plugin.p_fn_unload = (fn_void*)dlsym( plugin.p_handle, PLUGIN_UNLOAD );
			error = dlerror();
			if( plugin.p_fn_unload == NULL || error )
			{
				ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", plugin.path.c_str(), PLUGIN_UNLOAD, errno, error );
				return -1;
			}
			
			dlerror();
			plugin.p_fn_doworker = (fn_doworker*)dlsym( plugin.p_handle, PLUGIN_DOWORKER );
			error = dlerror();
			if( plugin.p_fn_doworker == NULL || error )
			{
				ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", plugin.path.c_str(), PLUGIN_DOWORKER, errno, error );
				return -1;
			}
			
			dlerror();
			plugin.p_fn_onexception = (fn_exception*)dlsym( plugin.p_handle, PLUGIN_ONEXCEPTION );
			error = dlerror();
			if( plugin.p_fn_onexception == NULL || error )
			{
				ERRORLOGSG( "path[%s]插件定位函数符号失败[%s] errno[%d] error[%s]", plugin.path.c_str(), PLUGIN_ONEXCEPTION, errno, error );
				return -1;
			}
			
			plugin.uri = p_env->httpserver_conf.httpserver.outputPlugins[i].uri;
			plugin.timeout = p_env->httpserver_conf.httpserver.outputPlugins[i].timeout;
			plugin.content_type = p_env->httpserver_conf.httpserver.outputPlugins[i].contentType;
			
			nret = plugin.p_fn_load();
			if( nret )
			{
				ERRORLOGSG( "path[%s]插件装载初始化失败[%s] errno[%d] error[%s]", plugin.path.c_str(), PLUGIN_LOAD, errno, error );
				return -1;
			}
			INFOLOGSG( "path[%s]插件装载初始化成功", plugin.path.c_str() );
			
			INFOLOGSG( "plugin.uri[%s] plugin.content_type[%s] ", plugin.uri.c_str(), plugin.content_type.c_str() );
			p_env->p_map_plugin_output->insert( pair<string,PluginInfo>( plugin.uri, plugin ) );
		}
	}
			
	return 0;
}

static int InitPlugin( HttpserverEnv *p_env )
{
	int	nret = 0 ;
	
	nret = InitPluginDB( p_env );
	if( nret )
	{
		ERRORLOGSG( "DB插件初始化失败" );
		return -1;
	}
	
	nret = InitInterceptors( p_env );
	if( nret )
	{
		ERRORLOGSG( "拦截器插件初始化失败" );
		return -1;
	}
	
	nret = InitPluginOutput( p_env );
	if( nret )
	{
		ERRORLOGSG( "输出插件初始化失败" );
		return -1;
	}

	return 0;
}

int InitWorkerEnv( HttpserverEnv *p_env )
{
	int			nret = 0 ;
	char 			module_name[50];
	
	ResetAllHttpStatus();
	/* 设置日志环境 */
	memset( module_name, 0, sizeof(module_name) );
	snprintf( module_name, sizeof(module_name)-1, "worker_%d", g_process_index+1 );
	nret = InitLogEnv( p_env, module_name, SERVER_AFTER_LOADCONFIG );
	if( nret )
	{
		// printf( "InitLogEnv failed[%d]\n" , nret );
		return nret;
	}
	
	nret = InitPlugin( p_env );
	if( nret )
	{
		ERRORLOGSG("InitPlugin failed" );
		return nret;
	}
	
	p_env->p_threadpool = threadpool_create( p_env->httpserver_conf.httpserver.threadpool.minThreads, p_env->httpserver_conf.httpserver.threadpool.maxThreads );
	if( !p_env->p_threadpool )
	{
		ERRORLOGSG("threadpool_create error ");
		return -1;
	}
	threadpool_setReserver1( p_env->p_threadpool, p_env );
	threadpool_setBeginCallback( p_env->p_threadpool, &ThreadBegin );
	threadpool_setRunningCallback( p_env->p_threadpool, &ThreadRunning );
	threadpool_setExitCallback( p_env->p_threadpool, &ThreadExit );	
	threadpool_setTaskTimeout( p_env->p_threadpool, p_env->httpserver_conf.httpserver.threadpool.taskTimeout );
	threadpool_setThreadSeed( p_env->p_threadpool, p_env->httpserver_conf.httpserver.threadpool.threadSeed );
	
	nret = threadpool_start( p_env->p_threadpool );
	if( nret )
	{
		ERRORLOGSG("threadpool_start error ");
		return -1;
	}

	/* 创建 epoll */
	for( int i = 0; i < p_env->httpserver_conf.httpserver.server.epollThread ; i++ )
	{
		// 创建 recv epoll
		pthread_mutex_init( &p_env->thread_epoll[i].session_lock, NULL );
		p_env->thread_epoll[i].index = i;
		p_env->thread_epoll[i].p_set_session = new setSession;
		p_env->thread_epoll[i].epoll_fd = epoll_create( 1024 ) ;
		INFOLOGSG( "epoll_index_recv[%d] epoll_fd[%d]" , i, p_env->thread_epoll[i].epoll_fd );
		if( p_env->thread_epoll[i].epoll_fd == -1 )
		{
			ERRORLOGSG( "recv epoll_create failed , errno[%d]" , errno );
			return -1;
		}
		
		// 创建 send epoll
		p_env->thread_epoll_send[i].index = i;
		p_env->thread_epoll_send[i].epoll_fd = epoll_create( 1024 ) ;
		INFOLOGSG( "epoll_index_send[%d] epoll_fd[%d]" , i, p_env->thread_epoll_send[i].epoll_fd );
		if( p_env->thread_epoll_send[i].epoll_fd == -1 )
		{
			ERRORLOGSG( "recv epoll_create failed , errno[%d]" , errno );
			return -1;
		}
		
		nret = pthread_create( &(p_env->thread_epoll[i].thread_id), NULL, (void* (*)(void*) )RecvEpollWorker, &( p_env->thread_epoll[i].index ) );
		if( nret )
		{
			ERRORLOGSG( "pthread_create failed , errno[%d]" , errno );
			g_exit_flag = 1 ;
			return -1;
		}
		INFOLOGSG( "pthread_create SendEpollWorker thread_id[%ld] ok" , p_env->thread_epoll[i].thread_id );
		
		nret = pthread_create( &(p_env->thread_epoll_send[i].thread_id), NULL, (void* (*)(void*) )SendEpollWorker, &( p_env->thread_epoll[i].index ) );
		if( nret )
		{
			ERRORLOGSG( "pthread_create failed , errno[%d]" , errno );
			g_exit_flag = 1 ;
			return -1;
		}
		INFOLOGSG( "pthread_create SendEpollWorker thread_id[%ld] ok" , p_env->thread_epoll_send[i].thread_id );

	}
	
	nret = pthread_create( &(p_env->thread_accept.thread_id), NULL, AcceptWorker, p_env );
	if( nret )
	{
		ERRORLOGSG( "pthread_create failed , errno[%d]" , errno );
		return -1;
	}
	INFOLOGSG( "pthread_create accept_thread_id[%ld] ok" , p_env->thread_accept.thread_id );
	
	return 0;
	
}

static int TravelSessions( HttpserverEnv *p_env, int index )
{
	struct AcceptedSession	*p_session = NULL;
	struct timeval		now_time;
	setSession::iterator 	it;
	
	gettimeofday( &now_time, NULL );
	/*控制1秒钟轮询session,防止cpu高耗*/
	if( ( now_time.tv_sec - p_env->thread_epoll[index].last_loop_session_timestamp ) >= 1  )
	{
		bool 	b_delete = false;
		int	nret;
		
		pthread_mutex_lock( &p_env->thread_epoll[index].session_lock );
		it = p_env->thread_epoll[index].p_set_session->begin();
		while( it != p_env->thread_epoll[index].p_set_session->end() )
		{
			b_delete = false;
			
			p_session = *it;
			DEBUGLOGSG("traversal sessions ip: %s %d now_time[%ld] request_begin_time[%ld] status[%d]", p_session->netaddr.remote_ip, 
					p_session->netaddr.remote_port,now_time.tv_sec, p_session->request_begin_time, p_session->status); 	
				
			if( p_session->status == SESSION_STATUS_PUBLISH )
			{
				++it;
				continue;
			}
			
			if( CheckHttpKeepAlive( p_session->http ) ) 
			{
				/*长连接时,空闲时间超过默认300秒，断开连接*/
				if( ( now_time.tv_sec - p_session->request_begin_time ) > p_env->httpserver_conf.httpserver.server.keepAliveIdleTimeout )
				{
					b_delete = true;
					INFOLOGSG( "keep_alive idling timeout close session fd[%d] p_session[%p]", p_session->netaddr.sock, p_session );
				}
				else if( p_session->status == SESSION_STATUS_HANG )
				{
					/*被挂起的long pulll连接超时30秒，发送 Not Modified 消息状态码为304*/
					if( ( now_time.tv_sec - p_session->perfms.tv_receive_begin.tv_sec ) > p_env->httpserver_conf.httpserver.server.hangUpTimeout )
					{	
						ResetHttpBuffer( GetHttpResponseBuffer(p_session->http) );
						p_session->hang_status = SESSION_HNAG_TIMEOUT;
						p_session->perfms.tv_receive_begin.tv_sec = now_time.tv_sec;
						nret = OnProcessAddTask( p_env, p_session );
						if( nret != HTTP_OK )
						{
							ERRORLOGSG( "OnProcessAddTadk failed[%d]" , nret );
							return HTTP_INTERNAL_SERVER_ERROR;
						}
						INFOLOGSG( "longpull timeout OnProcessAddTadk ok" );
						
					}
				}
				
			}
			else
			{
				/*短连接时,为配置交易时间的1.5倍，断开连接*/
				if( ( now_time.tv_sec - p_session->request_begin_time ) > p_env->httpserver_conf.httpserver.server.totalTimeout*SESSION_TIMEOUT_SEED )
				{
					b_delete = true;
					INFOLOGSG( "idling timeout close session fd[%d] p_session[%p]", p_session->netaddr.sock, p_session );
				}
				
			}

			if( b_delete )
			{
				epoll_ctl( p_session->epoll_fd_send , EPOLL_CTL_DEL , p_session->netaddr.sock , NULL );
				epoll_ctl( p_session->epoll_fd , EPOLL_CTL_DEL , p_session->netaddr.sock , NULL );
				FreeSession( p_session );
				p_env->thread_epoll[index].p_set_session->erase( it++ );
			}
			else
			{
				++it;
			}
		}
			
		pthread_mutex_unlock( &p_env->thread_epoll[index].session_lock );
		p_env->thread_epoll[index].last_loop_session_timestamp = now_time.tv_sec;
		
		DEBUGLOGSG("timer traversal sessions ok" );
	}
	
	return 0;
}

static int IsRun( HttpserverEnv *p_env, int index )
{
	int 			exit = 1;
	int			task_count;
	struct AcceptedSession	*p_session = NULL;
	
	/*清除长时间空闲的session*/
	TravelSessions( p_env, index );
	
	if( !g_exit_flag )
		return 1;
	
	/*接收到系统退出命令,关闭还没有开始业务处理会话，使sdk发送转连*/
	setSession::iterator it = p_env->thread_epoll[index].p_set_session->begin();
	while( it != p_env->thread_epoll[index].p_set_session->end() )
	{	
		p_session = *it;
		INFOLOGSG(" process will exit ip: %s %d", p_session->netaddr.remote_ip, p_session->netaddr.remote_port );
		if( p_session->status < SESSION_STATUS_PUBLISH || p_session->status == SESSION_STATUS_DIE )
		{
			INFOLOGSG( "process will exit close session fd[%d], p_session[%p]", p_session->netaddr.sock, p_session );
			OnClosingSocket( p_env, p_session, FALSE );
		}
		it++;
	}
	
	if( p_env->thread_epoll[index].p_set_session->empty() )
	{
		exit = 0;
	}
	
	task_count = threadpool_getTaskCount( p_env->p_threadpool );
	INFOLOGSG("IsRun g_exit_flag[%d] exit[%d] task_count[%d]", g_exit_flag, exit, task_count );
	
	return exit  ;

}

static int ClosePipeAndDestroyThreadpool( HttpserverEnv *p_env )
{
	int		nret;
	
	/*接收到退出命令，关闭监听*/
	close( p_env->listen_session.netaddr.sock );
	p_env->listen_session.netaddr.sock = -1;
	
	epoll_ctl( p_env->epoll_fd , EPOLL_CTL_DEL , p_env->p_pipe_session[g_process_index].pipe_fds[0] , NULL );
	FATALLOGSG( "epoll_ctl_recv[%d] del pipe_session[%d] errno[%d]" , p_env->epoll_fd , p_env->p_pipe_session[g_process_index].pipe_fds[0] , errno );
	close( p_env->p_pipe_session[g_process_index].pipe_fds[0] );
	p_env->p_pipe_session[g_process_index].pipe_fds[0] = -1;
		
	/*父进程已退出,等待所有任务完成，线程结束*/
	INFOLOGSG("pid[%ld] begin threadpool_destroy", getpid() );
	nret = threadpool_destroy( p_env->p_threadpool );
	if( nret == THREADPOOL_HAVE_TASKS )
	{
		FATALLOGSG("thread have tasks exception exit" );
		return -1;
	}
	else if( nret )
	{
		ERRORLOGSG("threadpool_destroy error errno[%d]", errno );
		return -1;
	}
	
	p_env->p_threadpool = NULL;
	INFOLOGSG("pid[%ld] threadpool_destroy OK", getpid() );
	
	return 0;
}

int worker( HttpserverEnv *p_env )
{
	struct epoll_event	event ;
	struct epoll_event	events[ MAX_EPOLL_EVENTS ] ;
	int			epoll_nfds = 0 ;
	struct epoll_event	*p_event = NULL ;
	struct PipeSession	*p_pipe_session = NULL ;
	struct ProcStatus 	*p_status = NULL;
	
	int			i = 0 ;
	int			nret = 0 ;
	
	nret = InitWorkerEnv( p_env );
	if( nret )
		return -1;
	INFOLOGSG("InitWorkerEnv ok");
		
	/* 创建 epoll */
	p_env->epoll_fd = epoll_create( 1024 ) ;
	if( p_env->epoll_fd == -1 )
	{
		ERRORLOGSG( "recv epoll_create failed , errno[%d]" , errno );
		g_exit_flag = 1;
		return -1;
	}
	
	/* 加入命令管道可读事件到epoll */
	memset( events , 0x00 , sizeof(events) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = & ( p_env->p_pipe_session[g_process_index] ) ;
	nret = epoll_ctl( p_env->epoll_fd , EPOLL_CTL_ADD , p_env->p_pipe_session[g_process_index].pipe_fds[0] , &event );
	if( nret == -1 )
	{
		ERRORLOGSG( "EPOLL_CTL_ADD pipe_session EPOLLIN failed g_process_index[%d] pipe [%d][%d]  errno[%d]", g_process_index,
			p_env->p_pipe_session[g_process_index].pipe_fds[0], p_env->p_pipe_session[g_process_index].pipe_fds[1], errno );
		close( p_env->epoll_fd );
		p_env->epoll_fd = -1;
		g_exit_flag = 1;
		return -1;
	}
	else
	{
		INFOLOGSG( "g_process_index[%d] epoll_ctl[%d] add pipe_session[%d] EPOLLIN", g_process_index, p_env->epoll_fd , p_env->p_pipe_session[g_process_index].pipe_fds[0] );
	}
	
	while( ! g_exit_flag )
	{
		if( getppid() == 1 )
		{
			INFOLOGSG("parent process is exit");
			break;
		}
		
		errno = 0;
		p_status = (struct ProcStatus*)p_env->p_shmPtr;
		if( p_status[g_process_index].effect_exit_time > 0  )
		{	
			INFOLOGSG( "worker_%d pid[%d] effect_exit_time[%ld] 检测到退出标志，进程将退出", 
				g_process_index+1, p_status[g_process_index].pid, p_status[g_process_index].effect_exit_time );
			g_exit_flag = 1;
		}

		/* 等待epoll事件，或者1秒超时 */
		memset( events , 0x00 , sizeof(events) );
		epoll_nfds = epoll_wait( p_env->epoll_fd , events , MAX_EPOLL_EVENTS , 1000 ) ;		
		if( epoll_nfds == -1 )
		{
			if( errno == EINTR )
			{
				INFOLOGSG( "epoll_wait[%d] interrupted" , p_env->epoll_fd );
				continue;
			}
			else
			{
				FATALLOGSG( "epoll_wait[%d] failed , errno[%d]" , p_env->epoll_fd , ERRNO );
				g_exit_flag = 1;
			}
		}
		
		/* 处理所有事件 */
		DEBUGLOGSG( "epoll_wait epoll_fd[%d] epoll_nfds[%d]" , p_env->epoll_fd , epoll_nfds );
		for( i = 0 , p_event = events ; i < epoll_nfds ; i++ , p_event++ )
		{
			INFOLOGSG( "epoll_wait_recv current_index[%d] p_event->data.ptr[%p] p_env->listen_session[%p] " , i , p_event->data.ptr , & (p_env->listen_session) );

			/* 命令管道事件 */
			if( p_event->data.ptr == &( p_env->p_pipe_session[g_process_index] ) )
			{
				INFOLOGSG( "pipe_session" )

				p_pipe_session = (struct PipeSession *)(p_event->data.ptr) ;

				/* 可读事件 */
				if( ( p_event->events & EPOLLIN ) )
				{
					nret = OnReceivePipe( p_env , p_pipe_session );
					if( nret < 0 )
					{
						FATALLOGSG( "OnProcessPipe failed[%d]", nret );
						nret = ClosePipeAndDestroyThreadpool( p_env );
						if( nret < 0 )
						{
							FATALLOGSG( "ClosePipeAndDestroyThreadpool failed[%d]", nret );
						}
						g_exit_flag = 1 ;
					}
					else if( nret > 0 )
					{
						ERRORLOGSG( "OnProcessPipe return[%d]" , nret );
						continue ;
					}
					else
					{
						DEBUGLOGSG( "OnProcessPipe ok" );
					}
				}
				/* 出错事件 */
				else if( ( p_event->events & EPOLLERR ) || ( p_event->events & EPOLLHUP ) )
				{			
					nret = ClosePipeAndDestroyThreadpool( p_env );
					if( nret < 0 )
					{
						FATALLOGSG( "ClosePipe failed[%d]", nret );
					}
					g_exit_flag = 1 ;
		
				}
				/* 其它事件 */
				else
				{
					FATALLOGSG( "Unknow pipe session event[0x%X]" , p_event->events );
					g_exit_flag = 1 ;
				}
			}
		}
		
		UpdateWorkingStatus( p_env);
		
		//db heartbeat
		if( p_env->p_fn_onheartbeat_dbpool )
		{
			nret = p_env->p_fn_onheartbeat_dbpool();
			if( nret )
			{
				ERRORLOGSG( "DB插件接口调用失败[%s] errno[%d]", p_env->httpserver_conf.httpserver.database.path, PLUGIN_ONHEARTBEAT, errno );
			}
		}
	}
		
	/* 关闭侦听套接字 */
	if( p_env->listen_session.netaddr.sock > 0 )
	{
		INFOLOGSG( "close listen[%d]" , p_env->listen_session.netaddr.sock );
		close( p_env->listen_session.netaddr.sock );
		p_env->listen_session.netaddr.sock = -1;
	}
	 /* 关闭epoll */
        INFOLOGSG( "close epoll_fd[%d]" , p_env->epoll_fd );
        close( p_env->epoll_fd );
	p_env->epoll_fd = -1;
	shmdt( p_env->p_shmPtr );

	return 0;
}

int RecvEpollWorker( void *arg )
{
	struct epoll_event	events[ MAX_EPOLL_EVENTS ] ;
	int			epoll_nfds = 0 ;
	struct epoll_event	*p_event = NULL ;
	struct AcceptedSession	*p_accepted_session = NULL ;
	
	int			i = 0 ;
	int			nret = 0 ;
	int			index = *(int*)arg;
	int			epoll_fd;
	HttpserverEnv 		*p_env = NULL ;
	char			module_name[50];
	
	memset( module_name, 0, sizeof(module_name) );
	snprintf( module_name, 15,"RecvEpoll_%d", index ); 
	prctl( PR_SET_NAME, module_name );
	
	p_env = UHPGetEnv();
	p_env->thread_epoll[index].cmd = 'I';
	epoll_fd = p_env->thread_epoll[index].epoll_fd ;
	INFOLOGSG("epoll_index[%d] epoll_fd ok", index, epoll_fd );	
	/*接收到退出信号,控制所有客户端已经断开连接，本进程才结束*/
	while( IsRun( p_env, index ) )
	{
		if( p_env->thread_epoll[index].cmd == 'I' ||  p_env->thread_epoll[index].cmd == 'L' ) /*线程内应用初始化*/
		{
			char	module_name[50];
			
			memset( module_name, 0, sizeof(module_name) );
			snprintf( module_name, sizeof(module_name) - 1, "worker_%d", g_process_index+1 );
			/* 设置日志环境 */
			InitLogEnv( p_env, module_name, SERVER_AFTER_LOADCONFIG );
			
			if( p_env->thread_epoll[index].cmd == 'I' )
			{
				INFOLOGSG(" InitLogEnv module_name[%s] ok", module_name );
			}
			else
			{
				INFOLOGSG(" reload config module_name[%s] ok", module_name );
			}
			p_env->thread_epoll[index].cmd = 0;

		}
		
		if( getppid() == 1 )
		{
			INFOLOGSG("parent process is exit");
			break;
		}
		
		errno = 0;
		/* 等待epoll事件，或者1秒超时 */
		memset( events , 0x00 , sizeof(events) );
		epoll_nfds = epoll_wait( epoll_fd , events , MAX_EPOLL_EVENTS , 1000 ) ;		
		if( epoll_nfds == -1 )
		{
			if( errno == EINTR )
			{
				INFOLOGSG( "epoll_wait[%d] interrupted" , epoll_fd );
				continue;
			}
			else
			{
				FATALLOGSG( "epoll_wait[%d] failed , errno[%d]" , epoll_fd , ERRNO );
				g_exit_flag = 1;
			}
		}
		
		/* 处理所有事件 */
		DEBUGLOGSG( "epoll_wait epoll_fd[%d] epoll_nfds[%d]" , epoll_fd , epoll_nfds );
		for( i = 0 , p_event = events ; i < epoll_nfds ; i++ , p_event++ )
		{
			INFOLOGSG( "epoll_wait_recv current_index[%d] p_event->data.ptr[%p] p_env->listen_session[%p] " , i , p_event->data.ptr , & (p_env->listen_session) );
			/* 客户端连接会话事件 */
			INFOLOGSG( "accepted_session" )
			p_accepted_session = (struct AcceptedSession*)(p_event->data.ptr) ;
			
			/* 可读事件 */
			if( p_event->events & EPOLLIN )
			{
				nret = OnReceivingSocket( p_env , p_accepted_session ) ;
				if( nret < 0 )
				{
					FATALLOGSG( "OnReceivingSocket failed[%d]" , nret );
					g_exit_flag = 1 ;
				}
				else if( nret > 0 )
				{
					INFOLOGSG( "OnReceivingSocket return[%d] p_accepted_session[%p]" , nret, p_accepted_session );
					OnClosingSocket( p_env , p_accepted_session, TRUE );
				}
				else
				{
					DEBUGLOGSG( "OnReceivingSocket ok" );
				}
			}
			/* 出错事件 */
			else if( ( p_event->events & EPOLLERR ) || ( p_event->events & EPOLLHUP ) || ( p_event->events & EPOLLRDHUP ) )
			{
				int		errCode;
				
				if( p_event->events & EPOLLERR )
					errCode = EPOLLERR;
				else if( p_event->events & EPOLLHUP )
					errCode = EPOLLHUP;
				else
					errCode = EPOLLRDHUP;
					
				ERRORLOGSG( "accepted_session EPOLLERR[%d] EPOLLHUP[%d] EPOLLRDHUP[%d] errno[%d]", EPOLLERR, EPOLLHUP, EPOLLRDHUP, errno );
				
				/*没有被线程池调度的状态可以安全关闭，其他状态需要等待线程执行完毕才能关闭*/
				if( p_accepted_session->status != SESSION_STATUS_PUBLISH )
				{
					ERRORLOGSG( "p_accepted_session[%p] errCode[%d] status[%d] fd[%d] errno[%d] 可以被安全关闭", p_accepted_session, errCode, p_accepted_session->status, p_accepted_session->netaddr.sock , errno );
					OnClosingSocket( p_env , p_accepted_session, TRUE );
				}
				else
				{
					ERRORLOGSG( "accepted_session status[%d] fd[%d] EPOLLERR errno[%d]" , p_accepted_session->status, p_accepted_session->netaddr.sock , errno );
				}
				
				
			}
			/* 其它事件 */
			else
			{
				FATALLOGSG( "Unknow accepted session event[%d]" , p_event->events );
				g_exit_flag = 1 ;
			}
			
		}
	}
	
	INFOLOGSG(" recv epoll Worker thread exit" );
	CleanLogEnv();
	
	return 0;
}

int SendEpollWorker( void *arg )
{
	struct epoll_event	events[ MAX_EPOLL_EVENTS ] ;
	int			epoll_nfds = 0 ;
	struct epoll_event	*p_event = NULL ;
	struct AcceptedSession	*p_accepted_session = NULL ;
	
	int			i = 0 ;
	int			nret = 0 ;
	int			index = *(int*)arg;
	int			epoll_fd;
	HttpserverEnv 		*p_env = NULL ;
	char			module_name[50];
	
	memset( module_name, 0, sizeof(module_name) );
	snprintf( module_name, 15,"SendEpoll_%d", index ); 
	prctl( PR_SET_NAME, module_name );
	
	p_env = UHPGetEnv();
	p_env->thread_epoll_send[index].cmd = 'I';
	epoll_fd = p_env->thread_epoll_send[index].epoll_fd ;
	INFOLOGSG("epoll_index[%d] epoll_fd ok", index, epoll_fd );	
	/*接收到退出信号,控制所有客户端已经断开连接，本进程才结束*/
	while( ! g_exit_flag )
	{
		if( p_env->thread_epoll_send[index].cmd == 'I' ||  p_env->thread_epoll_send[index].cmd == 'L' ) /*线程内应用初始化*/
		{
			char	module_name[50];
			
			memset( module_name, 0, sizeof(module_name) );
			snprintf( module_name, sizeof(module_name) - 1, "worker_%d", g_process_index+1 );
			/* 设置日志环境 */
			InitLogEnv( p_env, module_name, SERVER_AFTER_LOADCONFIG );
			
			if( p_env->thread_epoll_send[index].cmd == 'I' )
			{
				INFOLOGSG(" InitLogEnv module_name[%s] ok", module_name );
			}
			else
			{
				INFOLOGSG(" reload config module_name[%s] ok", module_name );
			}
			p_env->thread_epoll_send[index].cmd = 0;

		}
		
		if( getppid() == 1 )
		{
			INFOLOGSG("parent process is exit");
			break;
		}
		
		errno = 0;
		/* 等待epoll事件，或者1秒超时 */
		memset( events , 0x00 , sizeof(events) );
		epoll_nfds = epoll_wait( epoll_fd , events , MAX_EPOLL_EVENTS , 1000 ) ;		
		if( epoll_nfds == -1 )
		{
			if( errno == EINTR )
			{
				INFOLOGSG( "epoll_wait[%d] interrupted" , epoll_fd );
				continue;
			}
			else
			{
				FATALLOGSG( "epoll_wait[%d] failed , errno[%d]" , epoll_fd , ERRNO );
				g_exit_flag = 1;
			}
		}
		
		/* 处理所有事件 */
		DEBUGLOGSG( "epoll_wait epoll_fd[%d] epoll_nfds[%d]" , epoll_fd , epoll_nfds );
		for( i = 0 , p_event = events ; i < epoll_nfds ; i++ , p_event++ )
		{
			INFOLOGSG( "epoll_wait_recv current_index[%d] p_event->data.ptr[%p] p_env->listen_session[%p] " , i , p_event->data.ptr , & (p_env->listen_session) );
			/* 客户端连接会话事件 */
			INFOLOGSG( "accepted_session" )
			p_accepted_session = (struct AcceptedSession*)(p_event->data.ptr) ;
			
			/* 可写事件 */
			if( p_event->events & EPOLLOUT )
			{
				nret = OnSendingSocket( p_env , p_accepted_session ) ;
				if( nret < 0 )
				{
					FATALLOGSG( "OnSendingSocket failed[%d]" , nret	);
					g_exit_flag = 1 ;
				}
				else if( nret > 0 )
				{
					INFOLOGSG( "OnSendingSocket return[%d], p_accepted_session[%p]", nret, p_accepted_session );
					OnClosingSocket( p_env , p_accepted_session, TRUE );
				}
				else
				{
					DEBUGLOGSG( "OnSendingSocket ok" );
				}
			}
			/* 出错事件 */
			else if( ( p_event->events & EPOLLERR ) || ( p_event->events & EPOLLHUP ) || ( p_event->events & EPOLLRDHUP ) )
			{
				int		errCode;
				
				if( p_event->events & EPOLLERR )
					errCode = EPOLLERR;
				else if( p_event->events & EPOLLHUP )
					errCode = EPOLLHUP;
				else
					errCode = EPOLLRDHUP;
					
				ERRORLOGSG( "accepted_session EPOLLERR[%d] EPOLLHUP[%d] EPOLLRDHUP[%d] errno[%d]", EPOLLERR, EPOLLHUP, EPOLLRDHUP, errno );
				
				/*没有被线程池调度的状态可以安全关闭，其他状态需要等待线程执行完毕才能关闭*/
				if( p_accepted_session->status != SESSION_STATUS_PUBLISH )
				{
					ERRORLOGSG( "p_accepted_session[%p] errCode[%d] status[%d] fd[%d] errno[%d] 可以被安全关闭", p_accepted_session, errCode, p_accepted_session->status, p_accepted_session->netaddr.sock , errno );
					OnClosingSocket( p_env , p_accepted_session, TRUE );
				}
				else
				{
					ERRORLOGSG( "accepted_session status[%d] fd[%d] EPOLLERR errno[%d]" , p_accepted_session->status, p_accepted_session->netaddr.sock , errno );
				}
				
				
			}
			/* 其它事件 */
			else
			{
				FATALLOGSG( "Unknow accepted session event[%d]" , p_event->events );
				g_exit_flag = 1 ;
			}
			
		}
	}
	
	INFOLOGSG(" send epoll Worker thread exit" );
	CleanLogEnv();
	
	return 0;
}
