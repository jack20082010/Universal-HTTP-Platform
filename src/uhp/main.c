/*
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */
#include <sys/wait.h>
#include <sys/shm.h>
#include "uhp_ver.h"
#include "uhp_in.h"
 
static void version()
{
	printf( "uhp v%s build %s %s  \n" , SERVER_VERSION , __DATE__ , __TIME__  );
	printf( "Copyright by jack 2020.11\n" );
	
	return;
}

static void usage()
{
	printf( "USAGE : -v\n" );
	printf( "USAGE : -a init\n" );
	printf( "USAGE : [ uhp -f uhp.conf ] -a start\n" );
	printf( "USAGE : [ uhp -f uhp.conf ] -s thread_status \n" );
	printf( "USAGE : [ uhp -f uhp.conf ] -s sessions \n" );
	printf( "USAGE : [ uhp -f uhp.conf ] -s config \n" );
	printf( "kill -USR1 (pid) : reload_conf \n" );
	
	return;
}

int main( int argc , char *argv[] )
{
	int			nret = 0 ;
	HttpserverEnv		*p_env = NULL ;
	int			i ;
	
	nret = InitEnvironment( &p_env );
	if( nret )
	{
		printf("InitEnvironment nret[%d] failed errno[%d]\n", nret, errno );
		return 1;
	}
	
	/*设置全局httpserver环境，方便后续api获取操作*/
	UHPSetEnv( p_env );
	
	if( argc > 1 )
	{
		/* 解析命令行参数 */
		for( i = 1 ; i < argc ; i++ )
		{
			if( strcmp( argv[i] , "-v" ) == 0 )
			{
				version();
				CleanEnvironment( p_env );
				exit(0);
			}
			else if( strcmp( argv[i] , "-f" ) == 0 && i + 1 < argc )
			{
				p_env->server_conf_filename = argv[++i] ;
			}
			else if( strcmp( argv[i] , "-a" ) == 0 && i + 1 < argc )
			{
				p_env->cmd_param._action = argv[++i] ;
			}
			else if( strcmp( argv[i] , "-s" ) == 0 && i + 1 < argc )
			{
				p_env->cmd_param._show = argv[++i] ;
			}
			else if( strcmp( argv[i] , "--rotate-size" ) == 0 && i + 1 < argc )
			{
				p_env->cmd_param._rotate_size = argv[++i] ;
			}
			else if( strcmp( argv[i] , "--main-loglevel" ) == 0 && i + 1 < argc )
			{
				p_env->cmd_param._main_loglevel = argv[++i] ;
			}
			else if( strcmp( argv[i] , "--monitor-loglevel" ) == 0 && i + 1 < argc )
			{
				p_env->cmd_param._monitor_loglevel = argv[++i] ;
			}
			else
			{
				printf( "*** ERROR : Invalid opt[%s]\r\n" , argv[i] );
				CleanEnvironment( p_env );
				usage();
				exit(7);
			}
			
		}
		
		/* 主命令为初始化配置文件和目录 */
		if( p_env->cmd_param._action && STRCMP( p_env->cmd_param._action , == , "init" ) )
		{
			nret = InitConfigFiles( p_env ) ;
			CleanEnvironment( p_env );
			if( nret == HTTP_OK )
				return 0;
			else
				return abs(nret);
		}
		
		if( p_env->server_conf_filename == NULL )
			p_env->server_conf_filename =(char*) SERVER_CONF_FILENAME ;
		
		if( p_env->server_conf_filename[0] == '/' || p_env->server_conf_filename[0] == '\\')
			strncpy( p_env->server_conf_pathfilename, p_env->server_conf_filename, sizeof(p_env->server_conf_pathfilename)-1 );
		else
			snprintf( p_env->server_conf_pathfilename , sizeof(p_env->server_conf_pathfilename)-1 , "%s/etc/%s" , secure_getenv("HOME") , p_env->server_conf_filename );
		
		printf("server_conf_pathfilename[%s]\n",p_env->server_conf_pathfilename);
		/* 设置日志初始环境 */ 
		nret = InitLogEnv( p_env, "main", SERVER_BEFORE_LOADCONFIG );
		if( nret )
		{
			printf( "InitLogEnv failed[%d]" , nret );
			CleanEnvironment( p_env );
			return nret;
		}
			
		/* 装载主配置 */
		nret = LoadConfig( p_env, &(p_env->httpserver_conf) ) ;
		if( nret )
		{
			printf( "*** ERROR : LoadConfig[%s] failed[%d]\n" , p_env->server_conf_pathfilename , nret );
			CleanEnvironment( p_env );
			return 1;
		}
		
		/* 设置日志环境 */
		InitLogEnv( p_env, "main",SERVER_AFTER_LOADCONFIG );
		if( nret )
		{
			printf( "InitLogEnv faild[%d]\n" , nret );
			CleanEnvironment( p_env );
			return 1;
		}
			
		/* 主命令为启动 */
		if( p_env->cmd_param._action && STRCMP( p_env->cmd_param._action , == , "start" ) )
		{

			/* 切换为守护进程（管理进程） */
			nret = BindDaemonServer( & _monitor , p_env , 1 ) ;
			if( nret < 0 )
			{
				printf( "*** ERROR : BindDaemonServer failed[%d]\n" , nret );
			}

		}
		/* 主命令为显示所有已连接客户端会话列表 */
		else if( p_env->cmd_param._show && STRCMP( p_env->cmd_param._show , == , "sessions" ) )
		{
			nret = ShowSessions( p_env ) ;
			if( nret != HTTP_OK )
			{
				printf( "ShowSessions failed [%d]\n" , nret );
				CleanEnvironment( p_env );
				return 1;
			}
		}
		/* 主命令为显示线程状态信息 */
		else if( p_env->cmd_param._show && STRCMP( p_env->cmd_param._show , == , "thread_status" ) )
		{
			nret = ShowThreadStatus( p_env ) ;
			if( nret != HTTP_OK )
			{
				printf( "ShowThreadStatus failed [%d]\n" , nret );
				CleanEnvironment( p_env );
				return 1;
			}
		}
		/* 主命令为显示httpserver.conf 配置文件信息 */
		else if( p_env->cmd_param._show && STRCMP( p_env->cmd_param._show , == , "config" ) )
		{
			nret = ShowConfig( p_env ) ;
			if( nret != HTTP_OK )
			{
				printf( "ShowConfig failed [%d]\n" , nret );
				CleanEnvironment( p_env );
				return 1;
			}
		}
		else
		{
			CleanEnvironment( p_env );
			usage();
			exit(7);
		}
		
	}
	else
	{
		CleanEnvironment( p_env );
		usage();
		return 7;
	}
	
	/* 清理主环境 */
	CleanEnvironment( p_env );
	
	return abs(nret);
}

