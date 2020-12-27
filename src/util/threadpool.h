/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */


#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#ifdef __cplusplus
extern "C" {
#endif


#define	THREADPOOL_SUCCESS			 0
#define	THREADPOOL_INVALID			-1
#define	THREADPOOL_LOCK_FAILURE 		-2
#define	THREADPOOL_UNLOCK_FAILURE 		-3
#define THREADPOOL_MEM_FULL			-4
#define THREADPOOL_SHUTDOWN			-5
#define THREADPOOL_THREAD_FAILURE		-6
#define THREADPOOL_THREAD_FULL			-7
#define THREADPOOL_TASK_CALLBACK_ISNULL		-8
#define THREADPOOL_HAVE_TASKS			-9
#define THREADPOOL_FORBIT_DIMINISH		-10 /*禁止最大线程数减少*/
	
#define	THREADPOOL_TASK_SUCCESS			0
#define	THREADPOOL_TASK_FAILURE			-1
#define	THREADPOOL_TASK_RETRY			1

typedef int  (*fn_process)( void* arg,int threadno );
typedef struct threadpool_t threadpool_t;

#define THREADPOOL_THREAD_RUNNING		1
#define THREADPOOL_THREAD_WAITED		2
#define THREADPOOL_THREAD_ACTIVE		3
#define THREADPOOL_THREAD_EXITED		4

typedef struct _threadinfo_t
{
	pthread_t	id; 
	int 		index;
	threadpool_t	*p_pool;
	int		status;
	long		last_timestamp;
	int		task_timeout;
	char		cmd;			/*用户父子线程命令传递*/
} threadinfo_t;

typedef struct _taskinfo_t
{
	int		retry_times;
	int		timeout;
	fn_process 	fn_callback;
	void 		*arg;
	struct _taskinfo_t *next;

}taskinfo_t;

/*线程处理
*/
threadpool_t *threadpool_create( int min_threads, int max_threads );
int threadpool_addTask( threadpool_t *p_pool, taskinfo_t *p_taskinfo );
int threadpool_start( threadpool_t *p_pool );
int threadpool_destroy( threadpool_t *p_pool );


/*设置线程属性
*/
int threadpool_setTaskTimeout( threadpool_t *p_pool, int timeout );
int threadpool_setTaskRetryTimes( threadpool_t *p_pool, int times );
int threadpool_setThreadWaitTimeout( threadpool_t *p_pool, int timeout );
int threadpool_setThreadIdleTimes( threadpool_t *p_pool, int times );

int threadpool_setThreadSeed( threadpool_t *p_pool, int seed );
int threadpool_setThreads( threadpool_t *p_pool, int min_threads ,int max_threads );
int threadpool_setThreadsCommand( threadpool_t *p_pool, char cmd );
int threadpool_setReserver1( threadpool_t *p_pool, void* arg );
int threadpool_setReserver2( threadpool_t *p_pool, void* arg );

/*设置线程回调函数
*/
int threadpool_setBeginCallback( threadpool_t *p_pool, fn_process  begin_callback );
int threadpool_setRunningCallback( threadpool_t *p_pool, fn_process  run_callback );
int threadpool_setExitCallback( threadpool_t *p_pool, fn_process  exit_callback );


/*获取线程属性
*/
int threadpool_getMinThreads( threadpool_t *p_pool );
int threadpool_getMaxThreads( threadpool_t *p_pool );
int threadpool_getThreadCount( threadpool_t *p_pool );
int threadpool_getWorkingCount( threadpool_t *p_pool );
int threadpool_getIdleCount( threadpool_t *p_pool );
int threadpool_getWorkingTimeoutCount( threadpool_t *p_pool );
int threadpool_getTaskCount( threadpool_t *p_pool );

void* threadpool_getReserver1( threadpool_t *p_pool );
void* threadpool_getReserver2( threadpool_t *p_pool );
int   threadpool_isShutdown( threadpool_t *p_pool );



#ifdef __cplusplus
}
#endif

#endif /* _THREADPOOL_H_ */
