/*
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */
#include <net/if.h>
#include<sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <ifaddrs.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include "util.h"

/* 指定长度的转换字符串到整型 */
int StringNToInt( char *str , int len )
{
	char buf[ 20 + 1 ] ;
	
	memset( buf , 0x00 , sizeof(buf) );
	strncpy( buf , str , len );
	
	return atoi(buf);
}

int ConvertLogLevel( char *log_level_str )
{
	int	log_level ;
	
	log_level = ConvertLogLevel_atoi( log_level_str ) ;
	if( log_level == LOG_LEVEL_INVALID )
		return LOG_LEVEL_NOLOG;
	else
		return log_level;
}


/* 读取文件，内容存放到动态申请内存块中 */
/* 注意：使用完后，应用负责释放内存 */
char *StrdupEntireFile( char *pathfilename , int *p_file_len )
{
	char		*file_content = NULL ;
	int		file_len ;
	FILE		*fp = NULL ;
	
	int		nret = 0 ;
	
	fp = fopen( pathfilename , "rb" ) ;
	if( fp == NULL )
	{
		return NULL;
	}
	
	fseek( fp , 0 , SEEK_END );
	file_len  = ftell( fp ) ;
	fseek( fp , 0 , SEEK_SET );
	
	file_content = (char*)malloc( file_len+1 ) ;
	if( file_content == NULL )
	{
		fclose( fp );
		return NULL;
	}
	memset( file_content , 0x00 , file_len+1 );
	
	nret = fread( file_content , 1 , file_len , fp ) ;
	if( nret != file_len )
	{
		fclose( fp );
		free( file_content );
		return NULL;
	}
	
	fclose( fp );
	
	if( p_file_len )
		(*p_file_len) = file_len ;
	return file_content;
}

/* 写数据覆盖到文件 */
int WriteEntireFile( char *pathfilename , char *file_content , int file_len )
{
	FILE		*fp = NULL ;
	size_t		size ;
	
	fp = fopen( pathfilename , "wb" ) ;
	if( fp == NULL )
	{
		return -1;
	}
	
	size = fwrite( file_content , 1 , file_len , fp ) ;
	if( size != file_len )
	{
		fclose( fp );
		return -2;
	}
	
	fclose( fp );
	
	return 0;
}

int SetLogRotate( long rotate_size )
{
	long		index ;
	char		*g_id = NULL ;
	LOG		*g = NULL ;
	
	int		nret = 0 ;
	
	if( rotate_size < 0 )
		return 0;
	if( rotate_size == 0 )
		rotate_size = 10*1024*1024 ;
	
	index = -1 ;
	while(1)
	{
		nret = TravelLogFromLogsG( & index , & g_id , & g ) ;
		if( nret == LOGS_RETURN_INFO_NOTFOUND )
			break;
		
		if( rotate_size == 0 )
		{
			SetLogRotateMode( g , LOG_ROTATEMODE_NONE );
		}
		else
		{
			SetLogRotateMode( g , LOG_ROTATEMODE_SIZE );
			SetLogRotateSize( g , rotate_size );
			SetLogRotatePressureFactor( g , 20 );
		}
	}
	
	if( rotate_size == 0 )
	{
		SetLogRotateModeG( LOG_ROTATEMODE_NONE );
	}
	else
	{
		SetLogRotateModeG( LOG_ROTATEMODE_SIZE );
		SetLogRotateSizeG( rotate_size );
		SetLogRotatePressureFactorG( 20 );
	}
	
	return 0;
}

/* 转换文件大小字符串 */
long ConvertFileSizeString( char *file_size_str )
{
	char	*endptr = NULL ;
	long	val ;
	
	if( file_size_str == NULL )
		return -1;
	
	val = strtol( file_size_str , & endptr , 10 ) ;
	if( ( errno == ERANGE && ( val == LONG_MAX || val == LONG_MIN ) ) )
		return -1;
	
	if( endptr == file_size_str )
		return 0;
	
	if( endptr[0] == '\0' || STRCMP( endptr , == , "B" ) )
		return val;
	else if( STRCMP( endptr , == , "KB" ) )
		return val*1024;
	else if( STRCMP( endptr , == , "MB" ) )
		return val*1024*1024;
	else if( STRCMP( endptr , == , "GB" ) )
		return val*1024*1024*1024;
	else if( STRCMP( endptr , == , "TB" ) )
		return val*1024*1024*1024*1024;
	else
		return -1;
}


int GetLocalIp( char *ifa_name_prefix , char *ifa_name , int ifa_name_bufsize , char *ip , int ip_bufsize )
{
	int	nret = 0 ;
	struct ifaddrs	*ifa1 = NULL ;
	struct ifaddrs	*ifa = NULL ;
	void		*p_addr = NULL ;
	
	nret = getifaddrs( & ifa1 ) ;
	if( nret == -1 )
		return -1;
	
	ifa = ifa1 ;
	while( ifa )
	{
		if( ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET )
		{
			if( ifa_name_prefix == NULL || ifa_name_prefix[0] == '\0' || strncmp( ifa->ifa_name , ifa_name_prefix , strlen(ifa_name_prefix) ) == 0 )
			{
				if( ifa_name )
				{
					memset( ifa_name , 0x00 , ifa_name_bufsize );
					strncpy( ifa_name , ifa->ifa_name , ifa_name_bufsize-1 );
				}
				
				if( ip )
				{
					memset( ip , 0x00 , ip_bufsize );
					p_addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr ;
					inet_ntop( AF_INET , p_addr , ip , ip_bufsize );
				}
				
				freeifaddrs( ifa1 );
				return 0;
			}
		}
		
		ifa = ifa->ifa_next ;
	}
	
	freeifaddrs( ifa1 );
	
	return 1;	
}

/* 转换当前进程为守护进程 */
int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv , int close_flag )
{
	int	pid;
	int	fd0, fd1, fd2;
	
	pid = fork() ;
	switch( pid )
	{
		case -1:
			return -1;
		case 0:
			break;
		default		:
			return 0;
	}
	
	setsid();
	
	pid = fork() ;
	switch( pid )
	{
		case -1:
			return -2;
		case 0:
			break ;
		default:
			return 0;
	}
	
	if( close_flag )
	{
		close(0);
		close(1);
		close(2);

		/*占用0、1、2*/
		fd0 = open("/dev/null",O_RDWR);
		if( fd0	== -1 )
			return -1;

		fd1 = dup2(fd0,	1);
		if( fd1	== -1 )
			return -1;

		fd2 = dup2(fd0,	2);
		if( fd2	== -1 )
			return -1;
	}
	
	umask( 0 ) ;
	
	chdir( "/tmp" );
	
	ServerMain( pv );
	
	return 0;
}


int QueryNetIpByHostName( char *hostname , char *ip , int ip_bufsize )
{
	struct in_addr	s ;
	
	struct hostent	*phe = NULL ;
	char		**ip_addr = NULL , *ip_ptr = NULL ;
	
	int		nret = 0 ;
	
	memset( ip , 0x00 , ip_bufsize );
	
	/* 如果是合法IP，原样复制，返回0 */
	memset( & s , 0x00 , sizeof(struct in_addr) );
	nret = inet_pton( AF_INET , hostname , (void*) & s ) ;
	if( nret == 1 )
	{
		strncpy( ip , hostname , ip_bufsize-1 );
		return 0;
	}
	
	phe = gethostbyname( hostname ) ;
	if( phe )
	{
		switch( phe->h_addrtype )
		{
			case AF_INET :
			case AF_INET6 :
				/* 如果是IP别名，转换之，返回0 */
				ip_addr = phe->h_addr_list ;
#ifdef inet_ntop
				inet_ntop( phe->h_addrtype, (*ip_addr) , ip , ip_bufsize );
#else
				ip_ptr = inet_ntoa( *(struct in_addr*)(*ip_addr) ) ;
				snprintf( ip , ip_bufsize-1 , "%s" , ip_ptr );
#endif
				break;
			default :
				strncpy( ip , hostname , ip_bufsize-1 );
				break;
		}
		
		return 0;
	}
	
	/* 如果是无效IP或别名，原样复制，返回-1 */
	strncpy( ip , hostname , ip_bufsize-1 );
	return -1;
}

int QueryNetPortByServiceName( char *servname , char *proto , int *port )
{
	char		*ptr = NULL ;
	
	struct servent	*pse = NULL ;
	
	/* 如果是合法PORT，原样复制，返回0 */
	(*port) = (int)strtol( servname , & ptr , 10 ) ;
	if( (*ptr) == '\0' && ( 0 <= (*port) && (*port) <= 65535 ) )
		return 0;
	
	/* 如果是PORT别名，转换之，返回0 */
	pse = getservbyname( servname , proto ) ;
	if( pse )
	{
		(*port) = (int)ntohs(pse->s_port) ;
		return 0;
	}
	
	/* 如果是无效PORT或别名，原样复制，返回-1 */
	(*port) = atoi(servname) ;
	if( *port < 0  )
		return 0;
	
	return -1;
}


/***************************************************************************
函数名称: AllTrim
函数功能: 去字符串的空格函数
输出参数: sStr--原输入的字符串，返回处理后的字符串
		 
返 回 值: 返回处理后的字符串
***************************************************************************/
char* AllTrim( char *sStr )
{
	int i = 0;
	int nLen = 0;

	if( NULL == sStr || 0 == sStr[0] ) return sStr;

	for( i = 0; sStr[i] == ' '; i++ );
	if (i >0) 
	{
		strcpy( sStr, sStr + i ); //没找出内存重叠会发生问题
		//nLen=strlen(sStr);
		//memmove(sStr, sStr + i,nLen);
	}

	nLen = strlen( sStr );
	if( 0 == nLen )  return sStr;
	for (i = nLen - 1; sStr[i] == ' '; i-- );
	sStr[i + 1] = '\0';

	return sStr;
}

char *UpperStr(char *src)
{
	char* p = (char*)src;

	while( *p )
	{
		if( (*p >='a') && (*p <='z') )
		{
			*p = toupper( *p );
		}
		p++;
	}
	
	return src;
}

 char* strReplace( char *src, const char *sOld, char cValue )
{
	char* p = src;
	int nIndex = 0;
	int nDlen = strlen(sOld);

	if ( 1 == cValue)
		cValue='1';
	else if( 0 == cValue)
		cValue='0';

	while( *p ) 
	{

		if( *p == sOld[nIndex] )
		{
			nIndex++;
			if( nIndex == nDlen )
			{
				*p = cValue;
				while( --nIndex )
				{
					*(--p) =' ';
				}

			}

		}
		else
		{
			nIndex = 0;
		}

		p++; 
	}

	return src;
}

char *GetHostnamePtr()
{
        static char     name[ HOST_NAME_MAX + 1 ] ;

        int             nret = 0 ;

        memset( name , 0x00 , sizeof(name) );
        nret = gethostname( name , sizeof(name) ) ;
        if( nret == -1 )
                return NULL;

        return name;
}

char *GetUsernamePtr()
{
        struct passwd   *pwd = NULL ;

        pwd = getpwuid( getuid() ) ;
        if( pwd == NULL )
                return NULL;

        return pwd->pw_name;
}