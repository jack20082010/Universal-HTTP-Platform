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

## 相关服务插件化

Uhp是通用的http服务框架，要实现相关服务只需要开发相关输入、输出插件即可。
标准插件接口：
int Load(); // uhp启动时动态加载，初始化相关资源
int Unload();	// uhp程序退出时卸载调用，释放相关初始化资源
int Doworker( struct AcceptedSession *p_session ); //uhp线程池处理请求相关业务

## 序列中心插件

在复杂分布式系统中，往往需要对大量的数据和消息进行唯一标识。如在美团点评的金融、支付、餐饮、酒店、猫眼电影等产品的系统中，
数据日渐增长，对数据分库分表后需要有一个唯一ID来标识一条数据或消息，数据库的自增ID显然不能满足需求；特别一点的如订单、骑手、优惠券也都需要有唯一ID做标识。
此时一个能够生成全局唯一ID的系统是非常必要的。本次序列设计采用服务端、客户端多机缓存模式，每个序列独立一把锁，不同序列访问互不竞争，最大程序提高序列的并发性能。
支持单笔获取和批量获取模式，适应不同业务需求，支持控制台修改缓存数量，客户端无需重启服务就能提高序列的性能。

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