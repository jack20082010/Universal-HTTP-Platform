
#ifndef _H_SEQ_SDK_
#define _H_SEQ_SDK_

#define SDK_VERSION     "1.0"

#ifdef __cplusplus
extern "C" {
#endif

int Load();
int Unload();
unsigned long long GetSequence( char* name ) ;
int SetConnectTimeout( int timeout );
int SetServerUrl( char* server_url );

#ifdef __cplusplus
}
#endif

#endif
