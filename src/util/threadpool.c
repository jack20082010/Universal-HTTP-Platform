/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>

#include "threadpool.h"

#define THREADPOOL_THREAD_SEED			10
#define THREADPOOL_THREAD_WAIT_TIMEOUT 		2
#define THREADPOOL_TASK_TIMEOUT_SEED		1.5
#define THREADPOOL_TASK_TIMEOUT			60
#define THREADPOOL_TASK_RETRY_TIMES		3

#define THREADPOOL_TRUE				1
#define THREADPOOL_FALSE			0

struct threadpool_t 
{
	pthread_mutex_t		lock;
	pthread_cond_t		notify;
	threadinfo_t		*p_threads_info;

	int			thread_wait_timeout;
	int			thread_count;
	int			thread_seed;
	int			min_threads;
	int			max_threads;
	int			task_retry_times;
	int			task_timeout;
	int			task_count;
	int			shutdown;

	taskinfo_t		*p_taskHead;
	taskinfo_t		*p_taskTail;
	fn_process		begin_callback;
	fn_process		run_callback;
	fn_process		exit_callback;
	void 			*reserver1;
	void 			*reserver2;
	
};

static int add_thread( threadpool_t *p_pool );
static void *thread_working( void *arg );
static int threadpool_free( threadpool_t *p_pool );

threadpool_t *threadpool_create( int min_threads, int max_threads )
{
	threadpool_t	*p_pool;
	
	if( min_threads <= 0 )
		min_threads = 1;
	
	if( max_threads < min_threads )
		max_threads = min_threads;
	
	p_pool = (threadpool_t *)malloc( sizeof(threadpool_t) );
	if( !p_pool ) 
		return NULL;
	
	/* Initialize */
	memset( p_pool, 0, sizeof(threadpool_t) );
	p_pool->min_threads = min_threads; 
	p_pool->max_threads = max_threads;
	p_pool->task_timeout = THREADPOOL_TASK_TIMEOUT;
	p_pool->task_retry_times = THREADPOOL_TASK_RETRY_TIMES;
	p_pool->thread_wait_timeout = THREADPOOL_THREAD_WAIT_TIMEOUT;
	p_pool->thread_seed = THREADPOOL_THREAD_SEED;
	
	/* Allocate thread and task queue */
	p_pool->p_threads_info = (threadinfo_t *)malloc( sizeof(threadinfo_t)* p_pool->max_threads );
	if( !p_pool->p_threads_info )
	{
		free( p_pool );
		return NULL;
	}
	memset( p_pool->p_threads_info, 0, sizeof(threadinfo_t)*p_pool->max_threads );
	
	/* Initialize mutex and	conditional variable first */
	if( ( pthread_mutex_init( &(p_pool->lock), NULL) != 0 ) ||
		( pthread_cond_init(&(p_pool->notify), NULL) != 0 )
	   )
	{
		threadpool_free( p_pool );
		return NULL;
	}

	return p_pool;
}

int threadpool_start( threadpool_t *p_pool )
{
	int 	nret;
	int	i;
	
	for( i = 0; i < p_pool->min_threads; i++ )
	{
		nret = add_thread( p_pool );		
		if( nret < 0 )
		{
			return -1;
		}
	}

	return 0;
}
static threadinfo_t *find_unused_threadinfo( threadpool_t *p_pool )
{
	int 		i;
	int 		index = -1 ;
	threadinfo_t 	*p_threadinfo = NULL;
	
	if( !p_pool ) 
		return NULL;
		
	for( i = 0; i < p_pool->max_threads; i++ )
	{
		if( p_pool->p_threads_info[i].id <= 0 )
		{
			index = i;
			break;
		}
		
	}
	
	if( index > -1 )
		p_threadinfo = &( p_pool->p_threads_info[index] );

	return p_threadinfo;
	
}

static int modify_other_threads_timestamp( threadpool_t *p_pool, long timestamp  )
{
	int 		i;
	
	if( !p_pool )
		return THREADPOOL_INVALID;
	
	if( timestamp == 0 )
		timestamp = time(NULL);
	
	for( i = 0; i < p_pool->max_threads; i++ )
	{
		if( p_pool->p_threads_info[i].id >0 &&  
			p_pool->p_threads_info[i].status == THREADPOOL_THREAD_WAITED &&
			p_pool->p_threads_info[i].last_timestamp > 0
		  )
		{
			
			p_pool->p_threads_info[i].last_timestamp =  timestamp;
		}
	}
	
	return THREADPOOL_SUCCESS;
}

static int add_thread( threadpool_t *p_pool )
{
	threadinfo_t	*p_threadinfo = NULL;
	
	if( !p_pool )
		return THREADPOOL_INVALID;
	
	p_threadinfo = find_unused_threadinfo( p_pool );
	if( !p_threadinfo )
		return THREADPOOL_THREAD_FULL;
		
	p_threadinfo->index = p_pool->thread_count;
	p_threadinfo->task_timeout = 0;
	p_threadinfo->p_pool = p_pool;
	
	if( pthread_create( &( p_threadinfo->id ), NULL, thread_working, p_threadinfo ) != 0 )
	{
		return THREADPOOL_THREAD_FAILURE;
	}
	p_pool->thread_count++;

	return p_pool->thread_count;
}


static int threadpool_addTask2( threadpool_t *p_pool, taskinfo_t *p_task_in, int bretry )
{
	int		nret;
	int		task_count;
	taskinfo_t 	*p_task = NULL;
	
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	if( !p_task_in->fn_callback )
		return THREADPOOL_TASK_CALLBACK_ISNULL;
		
	if( !bretry )
	{
		
		p_task =(taskinfo_t*)malloc( sizeof(taskinfo_t) );
		if( !p_task )
			return THREADPOOL_MEM_FULL;
		
		memcpy( p_task , p_task_in , sizeof(taskinfo_t) );
		p_task->fn_callback = p_task->fn_callback;
		p_task->arg = p_task->arg;
	}
	else
	{
		p_task = p_task_in;
	}
	
	if( p_task->timeout <=0 )
		p_task->timeout = p_pool->task_timeout;

	p_task->next = NULL;
	
	
	if( pthread_mutex_lock( &(p_pool->lock) ) != 0 )
		return THREADPOOL_LOCK_FAILURE;
	
	/*将任务加入到等待队列中*/
	if( p_pool->p_taskHead && p_pool->p_taskTail )
	{
		p_pool->p_taskTail->next = p_task;
		p_pool->p_taskTail = p_task;	
	}
	else
	{
		/*空队列*/
		p_pool->p_taskHead = p_task;
		p_pool->p_taskTail = p_task;
	}

	task_count = p_pool->task_count++;
	if ( task_count > p_pool->thread_count && p_pool->thread_count < p_pool->max_threads
		&& ( task_count%p_pool->thread_seed ) == 0 )
	{
		nret = add_thread( p_pool );
		if( nret < 0 && nret != THREADPOOL_THREAD_FULL )
			return nret;
	}
	
	/*等待队列中有任务了，唤醒一个等待线程,注意如果所有线程都在忙碌，这句没有任何作用*/
	pthread_cond_signal( &p_pool->notify );
	pthread_mutex_unlock( &p_pool->lock );
	
	return task_count;
}

int threadpool_addTask( threadpool_t *p_pool, taskinfo_t *p_taskinfo )
{
	return threadpool_addTask2( p_pool, p_taskinfo, THREADPOOL_FALSE );
}

static void *thread_working( void *arg )
{
	int		nret;
	struct timeval	tm_now;
	threadinfo_t	*p_threadinfo = (threadinfo_t*)arg;
	threadpool_t	*p_pool = p_threadinfo->p_pool;
	
	/*线程初始化*/
	if( p_pool->begin_callback )
			p_pool->begin_callback( p_threadinfo , p_threadinfo->index );

	while( 1 )
	{
		/*工作运行中回调,用户执行命令 如：重置配置等*/
		if( p_pool->run_callback )
			p_pool->run_callback( p_threadinfo , p_threadinfo->index );
	
		pthread_mutex_lock( &p_pool->lock );
		gettimeofday( &tm_now, NULL );	
		/*等待队列长度减去1，并取出链表中的头元素*/
		taskinfo_t *p_task = p_pool->p_taskHead;
		if ( p_task )
		{
			p_pool->task_count--;
			p_pool->p_taskHead = p_task->next;
			pthread_mutex_unlock( &p_pool->lock );
			
			p_threadinfo->task_timeout = p_task->timeout;
			p_threadinfo->last_timestamp = tm_now.tv_sec;
			p_threadinfo->status = THREADPOOL_THREAD_RUNNING;
			
			/*调用回调函数，执行任务*/
			nret = p_task->fn_callback( p_task->arg, p_threadinfo->index );
			p_threadinfo->status = THREADPOOL_THREAD_ACTIVE;
			p_threadinfo->last_timestamp = 0;
			
			/*需要重试重新加入任务队列*/
			if( nret == THREADPOOL_TASK_RETRY && p_task->retry_times < p_pool->task_retry_times )
			{
				p_task->retry_times++;
				threadpool_addTask2( p_pool, p_task, THREADPOOL_TRUE );
			}
			else
			{
				free( p_task );
				p_task = NULL;
			}
			
		}
		else
		{
			/*控制一定要队列为空时，才能退出*/
			if( p_pool->shutdown )
			{
				pthread_mutex_unlock(&p_pool->lock);
				break;
			}
			
			/*挂起线程
			 1.pthread_cond_wait是一个原子操作，等待前会解锁，唤醒后会加锁
			 2.空闲次数大于配置次数，线程直接退出
			*/
			
			struct timespec		wait_timeout;
			int			cost_time = 0;
			
			if( p_threadinfo->status == THREADPOOL_THREAD_WAITED
				&& p_threadinfo->last_timestamp >0 )
			{
				cost_time = tm_now.tv_sec - p_threadinfo->last_timestamp;
				/*printf("index[%d] cost_time[%d]\n", p_threadinfo->index, cost_time ); */
			}
			
			if( cost_time > p_pool->thread_wait_timeout 
				&& p_pool->thread_count > p_pool->min_threads 
			  )
			{
				p_pool->thread_count--;
				p_threadinfo->status = THREADPOOL_THREAD_EXITED;
				p_threadinfo->id = 0;
				
				/*
				printf("now [%ld], index[%d] exit \n", tm_now.tv_sec, p_threadinfo->index);
				printf("thread_count[%d] exit \n",p_pool->thread_count);
				printf("min_threads[%d] exit \n",p_pool->min_threads);
				printf("cost_time[%d] exit \n", cost_time);
				fflush(stdout);
				*/
				
				/*修改其他线程状态，控制不要同一时间大量线程退出*/
				p_threadinfo->index = -1;
				modify_other_threads_timestamp( p_pool , tm_now.tv_sec );
				pthread_mutex_unlock(&p_pool->lock);
				pthread_detach( pthread_self() );

				break;

			}
			else
			{
				wait_timeout.tv_sec = tm_now.tv_sec + p_pool->thread_wait_timeout;
				wait_timeout.tv_nsec = tm_now.tv_usec*1000;
				
				if( p_threadinfo->status != THREADPOOL_THREAD_WAITED )
					p_threadinfo->last_timestamp = tm_now.tv_sec;
				
				p_threadinfo->status = THREADPOOL_THREAD_WAITED;
				nret = pthread_cond_timedwait(&p_pool->notify, &p_pool->lock, &wait_timeout );
				if( nret )
				{
					if( nret == ETIMEDOUT )
					{
						/*
						printf("ETIMEDOUT timedwait index[%d] \n", p_threadinfo->index );
						printf(" config thread_wait_timeout [%d]\n", p_pool->thread_wait_timeout );
						printf("begin_timestamp[%ld] \n",p_threadinfo->last_timestamp);
						printf("now_timestamp[%ld] \n",tm_now.tv_sec );
						printf("cost_time[%d]\n", cost_time);
						printf("thread_count[%d] \n",p_pool->thread_count);
						printf("----------------------------\n");
						*/

					}
					else
					{
						/*printf("pthread_cond_timedwait error nret[%d] errno[%d]",nret, errno);*/
					}
				}
				else
				{
					p_threadinfo->last_timestamp = 0;
					p_threadinfo->status = THREADPOOL_THREAD_ACTIVE;
				}
				
				pthread_mutex_unlock(&p_pool->lock);
				
			}
			
		}

	}
	/*线程退出回调，应用资源清理*/
	if( p_pool->run_callback )
			p_pool->exit_callback( p_threadinfo , p_threadinfo->index );
	
	return 0;
}

static int threadpool_free( threadpool_t *p_pool )
{
	taskinfo_t *p_task = NULL;
	
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	
	while(1)
	{
		p_task = p_pool->p_taskHead;
		if( !p_task )
			break;
			
		p_pool->p_taskHead = p_task->next;
		free(p_task);
	}
	
	pthread_mutex_destroy( &(p_pool->lock) );
	pthread_cond_destroy( &(p_pool->notify) );
	
	if( p_pool->p_threads_info ) 
		free( p_pool->p_threads_info );
	
	
	free(p_pool);
		   
	return 0;
}


int threadpool_destroy( threadpool_t *p_pool )
{
	int 	i; 
	int	err = 0;
	
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	if( pthread_mutex_lock( &(p_pool->lock)) != 0)
	{
		threadpool_free( p_pool );
		return THREADPOOL_LOCK_FAILURE;
	}
	
	if( p_pool->shutdown == THREADPOOL_SHUTDOWN )
	{
		if( ( pthread_mutex_unlock( &(p_pool->lock) ) != 0 ) )
		{
			threadpool_free( p_pool );
			return  THREADPOOL_UNLOCK_FAILURE;
		}
		return THREADPOOL_SUCCESS;
	}
		
	p_pool->shutdown = THREADPOOL_SHUTDOWN;
	
	/* Wake	up all worker threads */
	if( ( pthread_cond_broadcast( &(p_pool->notify) ) != 0 ) )
	{
		threadpool_free( p_pool );
		return  THREADPOOL_LOCK_FAILURE;
	}
	
	if( ( pthread_mutex_unlock( &(p_pool->lock) ) != 0 ) )
	{
		threadpool_free( p_pool  );
		return  THREADPOOL_LOCK_FAILURE;
	}
		
	/* Join	all worker thread */
	for( i = 0; i < p_pool->max_threads ; i++ )
	{
		if(p_pool->p_threads_info[i].id > 0 )
		{
			if( pthread_join( p_pool->p_threads_info[i].id, NULL ) != 0 )
				err = THREADPOOL_THREAD_FAILURE;
		}
	}
	
	/*所有线程退出后，队列还有任务，返回异常*/
	if( p_pool->p_taskHead )
	{
		return THREADPOOL_HAVE_TASKS;
	}
	
	threadpool_free( p_pool );
	
	return err;
}


int threadpool_setTaskTimeout( threadpool_t *p_pool, int timeout )
{
	if( !p_pool || timeout <= 0) 
		return THREADPOOL_INVALID;
	
	if( p_pool->task_timeout == timeout )
		return THREADPOOL_SUCCESS;
	
	p_pool->task_timeout = timeout;
		
	return THREADPOOL_SUCCESS;
}

int threadpool_setTaskRetryTimes( threadpool_t *p_pool, int times )
{
	if( !p_pool || times > THREADPOOL_TASK_RETRY_TIMES ) 
		return THREADPOOL_INVALID;
	
	if( p_pool->task_retry_times == times )
		return THREADPOOL_SUCCESS;
	
	p_pool->task_retry_times = times;
	
	return THREADPOOL_SUCCESS;
}

int threadpool_setThreadWaitTimeout( threadpool_t *p_pool, int timeout )
{
	if( !p_pool || timeout <= 0 ) 
		return THREADPOOL_INVALID;
	
	if( p_pool->thread_wait_timeout == timeout )
		return THREADPOOL_SUCCESS;
		
	p_pool->thread_wait_timeout = timeout;
	
	return THREADPOOL_SUCCESS;
}

int threadpool_setThreadSeed( threadpool_t *p_pool, int seed )
{
	if( !p_pool || seed <= 0 ) 
		return THREADPOOL_INVALID;
	
	if( p_pool->thread_seed == seed )
		return THREADPOOL_SUCCESS;
		
	p_pool->thread_seed = seed;
	
	return THREADPOOL_SUCCESS;
}

int threadpool_getWorkingCount( threadpool_t *p_pool )
{
	int	i, count = 0;
	
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	for( i = 0; i < p_pool->max_threads; i++ )
	{
		if( p_pool->p_threads_info[i].id > 0 && 
			p_pool->p_threads_info[i].status == THREADPOOL_THREAD_RUNNING )
		{
			count++;
		}
		
	}
	
	return count;
}

int threadpool_getIdleCount( threadpool_t *p_pool )
{
	int 	working_count ;
	int	Idle_count ;
	
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	working_count = threadpool_getWorkingCount( p_pool );
	
	Idle_count = p_pool->thread_count - working_count;
	
	return Idle_count;
}

int threadpool_getWorkingTimeoutCount( threadpool_t *p_pool )
{
	int 		timeout_count = 0;
	struct timeval	now_time;	
	int		i;
	
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	gettimeofday( &now_time, NULL );
	for( i = 0; i < p_pool->max_threads; i++ )
	{
		if( p_pool->p_threads_info[i].id > 0 && 
			p_pool->p_threads_info[i].status == THREADPOOL_THREAD_RUNNING
			&& p_pool->p_threads_info[i].last_timestamp >0
		  )
		{
			int timeout = p_pool->p_threads_info[i].task_timeout * THREADPOOL_TASK_TIMEOUT_SEED ;
			if( now_time.tv_sec - p_pool->p_threads_info[i].last_timestamp > timeout )
				timeout_count++;
		}
		
	}
	
	return timeout_count;
	
}

int threadpool_getThreadCount( threadpool_t *p_pool )
{
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	return p_pool->thread_count;
}

int threadpool_getMinThreads( threadpool_t *p_pool )
{
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	return p_pool->min_threads;
}

int threadpool_getMaxThreads( threadpool_t *p_pool )
{
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	return p_pool->max_threads;
}

int threadpool_getTaskCount( threadpool_t *p_pool )
{
	if( !p_pool ) 
		return THREADPOOL_INVALID;
		
	return p_pool->task_count;
}

int threadpool_isShutdown( threadpool_t *p_pool )
{
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	return p_pool->shutdown;
}

int threadpool_setThreads( threadpool_t *p_pool, int min_threads, int max_threads )
{
	threadinfo_t	*p_tmp = NULL;
	int 		i;
	int		max_index;
	
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	if( p_pool->min_threads == min_threads && p_pool->max_threads == max_threads )
		return THREADPOOL_SUCCESS;
		
	max_index = 0;
	for( i = 0; i < p_pool->max_threads; i++ )
	{
		if( p_pool->p_threads_info[i].id > 0 && i > max_index )
		{
			max_index = i;
		}
		
	}

	if( max_threads <  min_threads )
		max_threads = min_threads;
	
	/**最大线程数缩小,如果占用最大值超过当前使用最大值,
	  返回错误,需要人工重启才能配置生效
	*/
	if( max_threads < p_pool->max_threads && max_threads < max_index +1 );
	{
		return THREADPOOL_FORBIT_DIMINISH;	
	}
		
	pthread_mutex_lock( &p_pool->lock );
	
	p_tmp = (threadinfo_t*)realloc( p_pool->p_threads_info , max_threads );
	if( !p_tmp )
	{
		pthread_mutex_unlock( &p_pool->lock );
		return THREADPOOL_THREAD_FAILURE;
	}
	p_pool->p_threads_info = p_tmp;
	p_pool->min_threads = min_threads;
	p_pool->max_threads = max_threads;
	
	pthread_mutex_unlock( &p_pool->lock );
			
	return THREADPOOL_SUCCESS;
}

int threadpool_setReserver1( threadpool_t *p_pool, void* arg )
{
	if( !p_pool ) 
		return THREADPOOL_INVALID;
		
	p_pool->reserver1 = arg;
	
	return THREADPOOL_SUCCESS;
}

void* threadpool_getReserver1( threadpool_t *p_pool )
{
	if( !p_pool ) 
		return NULL;
		
	return p_pool->reserver1;
}

int threadpool_setReserver2( threadpool_t *p_pool, void* arg )
{
	if( !p_pool ) 
		return THREADPOOL_INVALID;
		
	p_pool->reserver2 = arg;
	
	return THREADPOOL_SUCCESS;
}

void* threadpool_getReserver2( threadpool_t *p_pool )
{
	if( !p_pool ) 
		return NULL;
		
	return p_pool->reserver2;
}

int threadpool_setThreadsCommand( threadpool_t *p_pool, char cmd )
{
	int 	i;
	
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	for( i = 0; i < p_pool->max_threads; i++ )
	{
		if( p_pool->p_threads_info[i].id > 0 )
		{
			p_pool->p_threads_info[i].cmd = cmd;
		}
		
	}
	
	return 0;
}


int threadpool_setBeginCallback( threadpool_t *p_pool, fn_process  begin_callback )
{
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	p_pool->begin_callback = begin_callback;
	
	return 0;
}

int threadpool_setRunningCallback( threadpool_t *p_pool, fn_process  run_callback )
{
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	p_pool->run_callback = run_callback;
	
	return 0;
}

int threadpool_setExitCallback( threadpool_t *p_pool, fn_process  exit_callback )
{
	if( !p_pool ) 
		return THREADPOOL_INVALID;
	
	p_pool->exit_callback = exit_callback;
	
	return 0;
}


#if 0
static int print_threads_info( threadpool_t *p_pool )
{
	int	i;
	
	if( !p_pool ) 
		return THREADPOOL_INVALID;
		

	for( i = 0; i < p_pool->max_threads; i++ )
	{
		printf("index[%d]\n" , p_pool->p_threads_info[i].index );
		printf("id[%ld]\n" , p_pool->p_threads_info[i].id );
		printf("status[%d]\n" , p_pool->p_threads_info[i].status );
		printf("last_timestamp[%ld]\n" , p_pool->p_threads_info[i].last_timestamp );
		printf("task_timeout[%d]\n" , p_pool->p_threads_info[i].task_timeout );
		printf("---------------------------------\n");
	}
	
	
	return THREADPOOL_SUCCESS;
}

int call_back( void *arg, int threadno )
{
	printf("threaid[%ld] threadno[%d]\n", pthread_self(), threadno);
	usleep(10);
	if( threadno < 1 )
		return THREADPOOL_TASK_RETRY;
	
	return 0;
}

int main()
{
	int	i, nret;
	
	int task_count =0;
	int working_count=0;
	int Idle_count=0;
	int thread_count=0;
	int timeout_count=0;
	
	threadpool_t *p_pool=NULL;
	taskinfo_t  task ;
	
	pthread_mutex_t		lock;
	pthread_cond_t		notify;
	
	nret = pthread_mutex_init( &lock, NULL);
	printf("01 pthread_mutex_init nret[%d] errno[%d]\n", nret, errno);
	
	//nret = pthread_mutex_init( &lock, NULL);
	//printf("02 pthread_mutex_init nret[%d] errno[%d]\n", nret, errno);
	
	nret = pthread_cond_init(&notify, NULL);
	printf("01 pthread_cond_init nret[%d]\n", nret);
	
	//nret = pthread_cond_init(&notify, NULL);
	//printf("02 pthread_cond_init nret[%d]\n", nret);
	
	nret=pthread_mutex_lock( &lock );
	printf("01 pthread_mutex_lock nret[%d] errno[%d]\n", nret, errno);
	
	//nret=pthread_mutex_lock( &lock );
	//printf("02 pthread_mutex_lock nret[%d] errno[%d]\n", nret, errno);
	
	
	nret=pthread_mutex_unlock( &lock );
	printf("01 pthread_mutex_unlock nret[%d] errno[%d]\n", nret, errno);
	
	//nret=pthread_mutex_unlock( &lock );
	//printf("02 pthread_mutex_unlock nret[%d] errno[%d]\n", nret, errno);
	
	nret=pthread_mutex_destroy( &lock );
	//nret=pthread_cond_destroy( &notify );
	printf("01 pthread_mutex_destroy nret[%d] errno[%d]\n", nret, errno);
	
	nret=pthread_mutex_destroy( &lock );
	//pthread_cond_destroy( &notify );
	printf("02 pthread_mutex_destroy nret[%d] errno[%d]\n", nret, errno);
	
	
	p_pool = threadpool_create( 3, 10);
	if( !p_pool )
	{
		printf("threadpool_create error\n");
	}
	
	//threadpool_setThreadWaitTimeout(p_pool, 3);
	print_threads_info(p_pool);
	
	memset( &task, 0, sizeof(taskinfo_t) );
	task.arg = p_pool;
	task.fn_callback = call_back;
	
	
	for( i= 0; i < 1000 ; i++ )
	{
		nret = threadpool_addTask( p_pool, &task );
		printf("threadpool_addTask nret[%d]\n", nret);
		
	}
	
	printf("ETIMEDOUT[%d]\n", ETIMEDOUT);
	printf("EINVAL[%d]\n", EINVAL);
	printf("EPERM[%d]\n", EPERM);
	printf("EINTR[%d]\n", EINTR);

	i = 0;

	while(1)
	{
		task_count = threadpool_getTaskCount( p_pool );
		working_count = threadpool_getWorkingCount( p_pool );
		Idle_count = threadpool_getIdleCount( p_pool );
		thread_count = threadpool_getThread_count( p_pool );
		timeout_count = threadpool_getWorkingTimeoutCount( p_pool );
			
		printf("thread_count[%d]\n", thread_count);
		printf("working_count[%d]\n", working_count);
		printf("Idle_count[%d]\n", Idle_count);
		printf("timeout_count[%d]\n", timeout_count);
		printf("task_count[%d]\n", task_count);
	
		
		printf("-------------------------------------------\n");
		
		
		if(!task_count)
			break;
	
		/*
		for( i = 0; i < p_pool->max_threads; i++ )
		{
			if( p_pool->p_threads_info[i].index > 0 )
			{
				printf("being SIGKILL \n");
				pthread_kill( p_pool->p_threads_info[i].id, SIGSTOP);
			}
			
		}*/
		
		//print_threads_info( p_pool );
		
		
		sleep(1);
		i++;
	}
	
	
	printf("按回车键退出....\n" );
	nret = threadpool_destroy(p_pool);
	printf("threadpool_destroy nret[%d]\n", nret);
	
	
	return 0;
}

#endif

