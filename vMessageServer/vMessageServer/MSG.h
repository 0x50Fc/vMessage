//
//  MSG.h
//  vMessageServer
//
//  Created by zhang hailong on 13-6-21.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#ifndef vMessageServer_MSG_h
#define vMessageServer_MSG_h


#ifdef __cplusplus
extern "C" {
#endif
    
    
    typedef struct _MSGBuffer {
        hchar * data;
        huint32 size;
        huint32 length;
    } MSGBuffer;
    
    huint32 MSGBufferExpandSize(MSGBuffer * buf,huint32 expandSize);
    
    typedef struct _MSGString {
        huint32 location;
        huint32 length;
    } MSGString;
    
    hbool MSGStringEqual(MSGBuffer * buf,MSGString string,hcchar * cString);
    
    extern MSGString MSGStringZero;
    
    typedef struct _MSGHttpHeader {
        MSGString key;
        MSGString value;
    } MSGHttpHeader;
    
    typedef enum _MSGHttpRequestState {
        MSGHttpRequestStateNone
        ,MSGHttpRequestStateMethod
        ,MSGHttpRequestStatePath
        ,MSGHttpRequestStateVersion
        ,MSGHttpRequestStateHeaderKey
        ,MSGHttpRequestStateHeaderValue
        , MSGHttpRequestStateOK
        ,MSGHttpRequestStateFail
    } MSGHttpRequestState;
    
    typedef struct _MSGHttpRequest {
        MSGString method;
        MSGString path;
        MSGString version;
        struct {
            MSGHttpHeader * data;
            huint32 size;
            huint32 length;
        } headers;
        struct {
            MSGHttpRequestState state;
            MSGString key;
            MSGString value;
        } state;
    } MSGHttpRequest;

    void MSGHTTPRequestReset(MSGHttpRequest * request);
    
    huint32 MSGHTTPRequestRead(MSGHttpRequest * request,MSGBuffer * buf,huint32 offset,huint32 length);
    
    hint32 MSGHttpRequestHeaderIntValue(MSGHttpRequest * request,MSGBuffer * buf,hcchar * key,hint32 defaultValue);
    
    MSGHttpHeader * MSGHttpRequestGetHeader(MSGHttpRequest * request,MSGBuffer * buf,hcchar * key);
    
    
#ifdef __cplusplus
}
#endif


#endif
