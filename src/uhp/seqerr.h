/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */
  
#ifndef _H_SEQERR_
#define _H_SEQERR_
#define		SERVER_HEAD_VERSION_NULL		-5001	/*Version字段不能为空*/
#define		SERVER_HTTPBODY_NULL			-5005	/*HttpBody不能为空*/
#define		SERVER_HTTPCONTEXT_NULL			-5006	/*HttpContext不能为空*/
#define		SERVER_MALLOC_FAILED			-5007	/*申请内存失败*/

#define		SERVER_POLL_ERROR			-5013	/*poll错误*/
#define		SERVER_POLL_TIMEOUT			-5014	/*poll超时*/
#define		SERVER_SOCKET_CLOSED			-5015	/*socket端对关闭*/

#endif
