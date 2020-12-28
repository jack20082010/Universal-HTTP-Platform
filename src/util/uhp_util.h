/*
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */
 
#ifndef _H_UTIL_IN_
#define _H_UTIL_IN_
#include <netinet/in.h>
#include "LOG.h"
#include "LOGS.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * 通讯基础信息结构
 */
#define UTIL_MAXLEN_HOST_IP		20
#define UTIL_MAXLEN_HOST_PORT		10
#define	UTIL_MAXLEN_FILENAME		255


struct NetAddress
{
	char			ip[ UTIL_MAXLEN_HOST_IP + 1 ] ;
	int			port ;
	int			sock ;
	struct sockaddr_in	addr ;
	
	struct sockaddr_in	local_addr ;
	char			local_ip[ UTIL_MAXLEN_HOST_IP + 1 ] ;
	int			local_port ;
	
	struct sockaddr_in	remote_addr ;
	char			remote_ip[ UTIL_MAXLEN_HOST_IP + 1 ] ;
	int			remote_port ;
} ;

/* 从NetAddress中设置、得到IP、PORT宏 */
#define SETNETADDRESS(_netaddr_) \
	memset( & ((_netaddr_).addr) , 0x00 , sizeof(struct sockaddr_in) ); \
	(_netaddr_).addr.sin_family = AF_INET ; \
	if( (_netaddr_).ip[0] == '\0' ) \
		(_netaddr_).addr.sin_addr.s_addr = INADDR_ANY ; \
	else \
		(_netaddr_).addr.sin_addr.s_addr = inet_addr((_netaddr_).ip) ; \
	(_netaddr_).addr.sin_port = htons( (unsigned short)((_netaddr_).port) );

#define GETNETADDRESS(_netaddr_) \
	strcpy( (_netaddr_).ip , inet_ntoa((_netaddr_).addr.sin_addr) ); \
	(_netaddr_).port = (int)ntohs( (_netaddr_).addr.sin_port ) ;

#define GETNETADDRESS_LOCAL(_netaddr_) \
	{ \
	socklen_t	socklen = sizeof(struct sockaddr) ; \
	int		nret = 0 ; \
	nret = getsockname( (_netaddr_).sock , (struct sockaddr*)&((_netaddr_).local_addr) , & socklen ) ; \
	if( nret == 0 ) \
	{ \
		strcpy( (_netaddr_).local_ip , inet_ntoa((_netaddr_).local_addr.sin_addr) ); \
		(_netaddr_).local_port = (int)ntohs( (_netaddr_).local_addr.sin_port ) ; \
	} \
	}

#define GETNETADDRESS_REMOTE(_netaddr_) \
	{ \
	socklen_t	socklen = sizeof(struct sockaddr) ; \
	int		nret = 0 ; \
	nret = getpeername( (_netaddr_).sock , (struct sockaddr*)&((_netaddr_).remote_addr) , & socklen ) ; \
	if( nret == 0 ) \
	{ \
		strcpy( (_netaddr_).remote_ip , inet_ntoa((_netaddr_).remote_addr.sin_addr) ); \
		(_netaddr_).remote_port = (int)ntohs( (_netaddr_).remote_addr.sin_port ) ; \
	} \
	}

int StringNToInt( char *str , int len );
int SetLogRotate( long rotate_size );
long ConvertFileSizeString( char *file_size_str );
int ConvertLogLevel( char *log_level_str );
char *StrdupEntireFile( char *pathfilename , int *p_file_len );
int WriteEntireFile( char *pathfilename , char *file_content , int file_len );
int QueryNetPortByServiceName( char *servname , char *proto , int *port );
int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv , int close_flag );
int QueryNetIpByHostName( char *hostname , char *ip , int ip_bufsize );
int GetLocalIp( char *ifa_name_prefix , char *ifa_name , int ifa_name_bufsize , char *ip , int ip_bufsize );


//--------------------------------------字符串操作相关的操作函数------------------------------

char *UpperStr( char *src );


/***************************************************************************
函数名称: AllTrim
函数功能: 去字符串的空格函数
输出参数: sStr--原输入的字符串，返回处理后的字符串
		 
返 回 值: 返回处理后的字符串
***************************************************************************/
char* AllTrim( char *sStr );


/***************************************************************************
函数名称: strReplace
函数功能: 字符串替换函数
输入参数: str--原输入的字符串
		  sOld--需要替换的字符串
		  sNew--替换新的字符串
输出参数: sResult
		 
返 回 值: true --成功
		  false--失败
***************************************************************************/

char* strReplace(char *src, const char *sOld, char cValue);

char *GetHostnamePtr();
char *GetUsernamePtr();

#ifdef __cplusplus
}
#endif

#endif
