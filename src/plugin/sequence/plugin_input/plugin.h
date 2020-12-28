/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef _H_PLUGIN_
#define _H_PLUGIN_

#include "uhp_api.h"
#include "fasterhttp.h"

#define URI_SEQUENCE				"/sequence"
#define HTTP_HEADER_CONTENT_TYPE_TEXT           "application/text;charset=gb18030"
#define	HTTP_HEADER_CONTENT_TYPE_JSON		"application/json"

class Cplugin
{
public:
        static Cplugin* Instance();
        static int      Uninstance();
        int 		Doworker( struct AcceptedSession *p_session );
        int 		Load();
private:
	static  Cplugin*      	_this;
        Cplugin();
        ~Cplugin();
        
};


#ifdef __cplusplus
extern "C" {
#endif

int Load();
int Unload();
int Doworker( struct AcceptedSession *p_session );

#ifdef __cplusplus
}
#endif

#endif