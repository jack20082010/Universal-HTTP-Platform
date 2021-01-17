STRUCT			httpserver_conf
{
	STRUCT		httpserver
	{
		STRUCT	server
		{
			STRING 30	ip
			STRING 20	port
			INT	4	listenBacklog
			INT	4	processCount
			STRING  10	restartWhen
			INT	4	maxTaskcount
			INT	4	taskTimeoutPercent 
			INT	1	perfmsEnable
			INT	4	totalTimeout
			INT	4	keepAliveIdleTimeout
			INT	4	keepAliveMaxTime
			INT	4	showStatusInterval
			INT	4	maxChildProcessExitTime
			INT	4 	maxHttpResponse
			INT	4	epollThread
		}
		
		STRUCT interceptors ARRAY 100
		{
			STRING	256 path
		}
		
		STRUCT outputPlugins ARRAY 1000
		{
			STRING 64 uri
			STRING 64 contentType
			INT     4  timeout
			STRING 256 path
		}
		
		STRUCT database
		{
			STRING 256      path
			STRING 30	ip
			INT     4 	port
			STRING 32	username
			STRING 32	password
			STRING 32	dbname
			INT 4		minConnections
			INT 4		maxConnections
		}
		
		STRUCT threadpool
		{
			INT 4 		minThreads
			INT 4 		maxThreads
			INT 4 		taskTimeout
			INT 4		threadWaitTimeout
			INT 1		threadSeed
		}
		
		STRUCT	log
		{
			STRING 20	rotate_size
			STRING 6	main_loglevel
			STRING 6	monitor_loglevel
			STRING 6	worker_loglevel
		}

		STRUCT reserve
		{
			INT 4		int1
			INT 4		int2
			INT 4		int3
			INT 4		int4
			INT 4		int5
			INT 4		int6
			STRING 	30	str301
			STRING	30	str302
			STRING 	50	str501
			STRING	50	str502
			STRING 	80	str801
			STRING	80	str802
			STRING 	128	str1281
			STRING	128	str1282
			STRING 	255	str2551
			STRING	255	str2552
		}
	}
}
