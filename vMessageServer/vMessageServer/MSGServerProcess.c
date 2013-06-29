//
//  MSGServerProcess.c
//  vMessageServer
//
//  Created by zhang hailong on 13-6-21.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#include "hconfig.h"
#include "MSGServerProcess.h"
#include "hstream.h"
#include "MSG.h"
#include "MSGAuth.h"
#include "MSGDatabase.h"

#define MSGServerProcessClientMaxCount     1024
#define MSGServerProcessClientMaxThread    128

#define MSGServerProcessThreadBufferSize    2048

#define REQUEST_TIMEOUT     120
#define RESPONSE_TIMEOUT     120
#define RESOURCE_POST_TIMEOUT   600

#define MSGServerProcessDefaultMaxContentLength    102400

#define MSGAllowOrigin              "*"
#define MSGAllowHeaders             "Authorization"
#define MSGAllowMethod              "GET, POST, OPTIONS"
#define MSGAllowResponseHeaders     "Timestamp"

typedef struct _MSGServerProcessThread{
    pthread_t pthread;
    int client;
} MSGServerProcessThread;

static struct {
    int clients[MSGServerProcessClientMaxCount];
    int startIndex;
    int endIndex;
    MSGServerProcessThread threads[MSGServerProcessClientMaxThread];
    int processExit;
    double sleep_v ;
    int maxContentLength;
    MSGAuthClass * authClass;
    MSGDatabaseClass * databaseClass;
} MSGServerProcess = {0};

static void * MSGServerProcessThreadRun(void * userInfo){
    
    MSGServerProcessThread * thread = (MSGServerProcessThread *) userInfo;
    unsigned long idle = 0;
    unsigned int sleep_v = 0;
    MSGBuffer sbuf = {0,0,0};
    MSGBuffer dbuf = {0,0,0};
    MSGHttpRequest request = {0};
    MSGAuth * auth = NULL;
    MSGDatabase * database = NULL;
    StreamState state;
    huint32 offset;
    hint32 len;
    hint32 contentLength;
    MSGDatabaseResult dbResult;
    MSGHttpHeader * h;
    hint32 POSTDataOK;
    MSGDatabaseResource res;
    hcchar * uri;
    
    while (!MSGServerProcess.processExit) {
        
        if(thread->client){
            
            MSGBufferExpandSize(& sbuf, MSGServerProcessThreadBufferSize);
            offset = 0;
            sbuf.length = 0;
            MSGHTTPRequestReset(& request);
            state = StreamStateOK;
            auth = NULL;
            database = NULL;
            
            while(state == StreamStateOK){
                
                if(request.state.state == MSGHttpRequestStateFail){
                    break;
                }
                
                if(request.state.state == MSGHttpRequestStateOK){
                    break;
                }

                state = stream_socket_has_data(thread->client, REQUEST_TIMEOUT);
                
                if(state != StreamStateOK){
                    break;
                }
                
                MSGBufferExpandSize(& sbuf, MAX(MSGServerProcessThreadBufferSize,sbuf.length + MSGServerProcessThreadBufferSize));
                
                len = stream_socket_read(thread->client,sbuf.data + sbuf.length, sbuf.size - sbuf.length);
                
                if(len <=0){
                    break;
                }
                
                sbuf.length += len;
                
                len = MSGHTTPRequestRead(& request, &sbuf, offset, sbuf.length - offset);
                
                offset += len;
                
            }
            
            if(request.state.state == MSGHttpRequestStateOK){

                if(MSGStringEqual(&sbuf,request.method,"POST")){
                    
                    auth = (* MSGServerProcess.authClass->create)(MSGServerProcess.authClass,& request,& sbuf);
                    
                    if(auth){
                        
                        if(state == StreamStateOK || state == StreamStateNone){
                            
                            database = (* MSGServerProcess.databaseClass->open)(MSGServerProcess.databaseClass,& request,& sbuf);
                            
                            if(database){
                                
                                uri = NULL;
                                
                                contentLength = MSGHttpRequestHeaderIntValue( & request, & sbuf, "Content-Length", 0);
                            
                                if(contentLength == 0){
                                    
                                    h = MSGHttpRequestGetHeader(& request, & sbuf, "Transfer-Encoding");
                                    
                                    if(h && MSGStringEqual(& sbuf, h->value, "chunked")){
                                        
                                        h = MSGHttpRequestGetHeader(& request, & sbuf, "Content-Type");
                                        
                                        if(h){
                                            * (sbuf.data + h->value.location + h->value.length) = 0;
                                        }
                                        
                                        POSTDataOK = 0;
                                        
                                        if(h && (* MSGServerProcess.databaseClass->openResource)(database,auth,&res,NULL,sbuf.data + h->value.location)){
                                    
                                            int s = 0;
                                            char * lenChar = 0;
                                            char * p;
                                            int index = 0;
                                            int uLength = 0;
                                            
                                            dbuf.length = sbuf.length - offset;
                                            
                                            MSGBufferExpandSize(&dbuf, MAX(MSGServerProcessDefaultMaxContentLength, dbuf.length));
                                            
                                            memcpy(dbuf.data, sbuf.data + offset, dbuf.length);
                                            
                                            POSTDataOK = 1;
                                            
                                            while (POSTDataOK == 1) {
                                                
                                                if(dbuf.length == 0){
                                                    state = stream_socket_has_space(thread->client, RESOURCE_POST_TIMEOUT);
                                                    if(state == StreamStateOK){
                                                        if(dbuf.size > index){
                                                            len = stream_socket_read(thread->client, dbuf.data + index, dbuf.size - index);
                                                            if(len <0){
                                                                POSTDataOK = 0;
                                                                break;
                                                            }
                                                            dbuf.length += len;
                                                        }
                                                        else{
                                                            len = stream_socket_read(thread->client, dbuf.data, dbuf.size);
                                                            if(len <0){
                                                                POSTDataOK = 0;
                                                                break;
                                                            }
                                                            dbuf.length = len;
                                                            index = 0;
                                                        }
                                                        if(dbuf.length == 0){
                                                            continue;
                                                        }
                                                    }
                                                    else{
                                                        POSTDataOK = 0;
                                                        break;
                                                    }
                                                }
                                                
                                                p = dbuf.data + index;
                                                
                                                switch (s) {
                                                    case 0:
                                                    {
                                                        // chunked length
                                                        if(*p == '\r'){
                                                            *p =0 ;
                                                        }
                                                        else if(*p == '\n'){
                                                            *p = 0;
                                                            uLength = 0;
                                                            sscanf(lenChar, "%x",&uLength);
                                                            if(uLength == 0){
                                                                s = 3;
                                                            }
                                                            else {
                                                                s = 1;
                                                            }
                                                        }
                                                        else if(lenChar == 0){
                                                            lenChar = p;
                                                        }
                                                    }
                                                        break;
                                                    case 1:
                                                    {
                                                        // chunked body
                                                        if(uLength <= dbuf.length){
                                                            if(uLength != write(res.fno, dbuf.data + index, uLength)){
                                                                POSTDataOK = 0;
                                                                continue;
                                                            }
                                                            index += uLength;
                                                            dbuf.length -= uLength;
                                                            uLength = 0;
                                                            s = 2;
                                                            continue;
                                                        }
                                                        else {
                                                            if(dbuf.length != write(res.fno, dbuf.data + index, dbuf.length)){
                                                                POSTDataOK = 0;
                                                                continue;
                                                            }
                                                            index += dbuf.length;
                                                            uLength -= dbuf.length;
                                                            dbuf.length =0;
                                                            continue;
                                                        }
                                                    }
                                                        break;
                                                    case 2:
                                                    {
                                                        if(*p == '\r'){
                                                            
                                                        }
                                                        else if(*p == '\n'){
                                                            s = 0;
                                                            lenChar = 0;
                                                            break;
                                                        }
                                                        else{
                                                            POSTDataOK = 0;
                                                        }

                                                    }
                                                        break;
                                                    case 3:
                                                    {
                                                        if(*p == '\r'){
                                                            
                                                        }
                                                        else if(*p == '\n'){
                                                            POSTDataOK = 2;
                                                            break;
                                                        }
                                                        else{
                                                            POSTDataOK = 0;
                                                        }
                                                    }
                                                        break;
                                                    default:
                                                        break;
                                                }
                                                
                                                index ++;
                                                dbuf.length --;
                                                
                                            }
                                            
                                            (* MSGServerProcess.databaseClass->closeResource)(database,auth,&res);
                                            
                                            if(!POSTDataOK){
                                                
                                                (* MSGServerProcess.databaseClass->removeResource)(database,auth,res.uri);
                                                
                                            }
                                            else{
                                                
                                                sbuf.length = offset;
                                                
                                                uri = res.uri;
                                            }
                                        }
                                        
                                        if(!POSTDataOK){
                                            
                                            state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                            
                                            if(state == StreamStateOK){
                                                
                                                sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 408 Request Timeout\r\nAccess-Control-Allow-Origin: %s\r\n\r\n",MSGAllowOrigin);
                                                
                                                stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                                
                                            }
                                        }
                                        
                                    }
                                    else{
                                        
                                        POSTDataOK = 0;
                                        
                                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                        
                                        if(state == StreamStateOK){
                                            
                                            sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 411 Length Required\r\nAccess-Control-Allow-Origin: %s\r\n\r\n",MSGAllowOrigin);
                                            
                                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                            
                                        }
                                    }
                                }
                                else if(contentLength > MSGServerProcess.maxContentLength){
                                    
                                    POSTDataOK = 0;
                                    
                                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                    
                                    if(state == StreamStateOK){
                                        
                                        sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 413 Request Entity Too Large\r\nAccess-Control-Allow-Origin: %s\r\n\r\n",MSGAllowOrigin);
                                        
                                        stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                        
                                    }
                    
                                }
                                else{ 
                                
                                    state = stream_socket_has_data(thread->client, 0.0);
                                    
                                    while(state == StreamStateOK){
                                        
                                        if(sbuf.length - offset >= contentLength){
                                            break;
                                        }
                                        
                                        state = stream_socket_has_data(thread->client, REQUEST_TIMEOUT);
                                        
                                        if(state == StreamStateOK){
                                            
                                            MSGBufferExpandSize(& sbuf, contentLength + offset);
                                            
                                            len = stream_socket_read(thread->client, sbuf.data + sbuf.length, contentLength + offset - sbuf.length);
                                            
                                            sbuf.length += len;
                                            
                                        }
                                        else{
                                            break;
                                        }
                                    }
                                    
                                    POSTDataOK = state != StreamStateError;
                                    
                                    if(!POSTDataOK){
                                        
                                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                        
                                        if(state == StreamStateOK){
                                            
                                            sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 408 Request Timeout\r\nAccess-Control-Allow-Origin: %s\r\n\r\n",MSGAllowOrigin);
                                            
                                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                            
                                        }
                                    }
                                }
                                
                                if(POSTDataOK){
                                    
                                    dbResult = ( * MSGServerProcess.databaseClass->write)(database,auth,& request,& sbuf,offset,&dbuf,uri);
                                    
                                    (* MSGServerProcess.databaseClass->close)(database);
                                    
                                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                    
                                    if(state == StreamStateOK){
                                        
                                        sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 %d %s\r\nAccess-Control-Allow-Origin: %s\r\n\r\n"
                                                               ,dbResult.statusCode,dbResult.status,MSGAllowOrigin);
                                        
                                        stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                        
                                    }
                                }
                            }
                            else{
                                state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                
                                if(state == StreamStateOK){
                                    
                                    sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 507 Insufficient Storage\r\nAccess-Control-Allow-Origin: %s\r\n\r\n"
                                                           ,MSGAllowOrigin);
                                    
                                    stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                    
                                }
                            }
                        }
                        else{
                            
                            state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                            
                            if(state == StreamStateOK){
                                
                                sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 408 Request Timeout\r\nAccess-Control-Allow-Origin: %s\r\n\r\n",MSGAllowOrigin);
                                
                                stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                
                            }
                            
                        }
                        
                        (* MSGServerProcess.authClass->release)(auth);
                    }
                    else{
                        
                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                        
                        if(state == StreamStateOK){
                            
                            sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 401 Authorization Required\r\nWWW-Authenticate: Basic\r\nAccess-Control-Allow-Origin: %s\r\n\r\n",MSGAllowOrigin);
                            
                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                            
                        }
                        
                    }
                    
                    
                    
                }
                else if(MSGStringEqual(&sbuf, request.method, "GET")){
                    
                    auth = (* MSGServerProcess.authClass->create)(MSGServerProcess.authClass,& request,& sbuf);
                    
                    if(auth){
                    
                        
                        database = (* MSGServerProcess.databaseClass->open) (MSGServerProcess.databaseClass,& request,& sbuf);
                        
                        if(database){
                        
                            if(MSGStringHasPrefix(&sbuf, request.path, "/r/")){
                                
                                {
                                    struct stat s;
                                    hbool isOK = 0;
                                    hchar etag[128];
                                    
                                    * (sbuf.data + request.path.location + request.path.length) = 0;
                                    
                                    if( ( * MSGServerProcess.databaseClass->openResource)(database,auth,&res,sbuf.data + request.path.location,NULL) ){
                                        
                                        if(fstat(res.fno, &s) != -1){
                                            
                                            h = MSGHttpRequestGetHeader(&request, &sbuf, "If-None-Match");
                                            
                                            if(h){
                                                snprintf(etag, sizeof(etag),"W/%lx",s.st_mtimespec.tv_sec);
                                                
                                                if(MSGStringEqual(&sbuf, h->value, etag)){
                                                    
                                                    sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 304 Not Changed\r\nAccess-Control-Allow-Origin: %s\r\nETag: \"%lx\"\r\n\r\n"
                                                                           ,MSGAllowOrigin,s.st_mtimespec.tv_sec);
                                                    
                                                    isOK = 1;
                                                }
                                                    
                                            }
                                            
                                            
                                            if(!isOK){
                                            
                                                sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %llu\r\nAccess-Control-Allow-Origin: %s\r\nETag: %lx\r\n\r\n"
                                                                       ,res.type,s.st_size - lseek(res.fno, 0, SEEK_CUR),MSGAllowOrigin,s.st_mtimespec.tv_sec);
                                                
                                               
                                            
                                                stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                                 
                                                
                                                offset = 0;
                                                sbuf.length = 0;
                                                
                                                while (1) {
                                                    
                                                    if(sbuf.length == 0){
                                                        sbuf.length = read(res.fno, sbuf.data, sbuf.size);
                                                        offset= 0;
                                                        if(sbuf.length ==0){
                                                            break;
                                                        }
                                                    }
                                                    
                                                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                                    
                                                    if(state == StreamStateOK){
                                                        len = stream_socket_write(thread->client, sbuf.data + offset, sbuf.length);
                                                        if(len == -1){
                                                            break;
                                                        }
                                                        offset += len;
                                                        sbuf.length -= len;
                                                    }
                                                    else{
                                                        break;
                                                    }
                                                }
                                                
                                                isOK = sbuf.length = 0;
                                                
                                            }
                                            
                                        }
                                        
                                        ( * MSGServerProcess.databaseClass->closeResource)(database,auth,&res);
                                    }
                                    
                                    if(!isOK){
                                        
                                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                        
                                        if(state == StreamStateOK){
                                            
                                            sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 404 Not Found\r\nAccess-Control-Allow-Origin: %s\r\n\r\n",MSGAllowOrigin);
                                            
                                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                            
                                        }
                                    }
                                }
                                
                            }
                            else{
                            
                                MSGDatabaseCursor * cursor = ( * MSGServerProcess.databaseClass->cursorOpen)(database,auth,&request,&sbuf);
                                MSGDatabaseEntity * entity;
        
                                state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                
                                if(state == StreamStateOK){
                                    
                                    if(* auth->cookie != 0){
                                        sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nSet-Cookie: %s\r\nTransfer-Encoding: chunked\r\nAccess-Control-Allow-Origin: %s\r\nTimestamp: %f\r\nAccess-Control-Expose-Headers: %s\r\n\r\n"
                                                               ,auth->cookie,MSGAllowOrigin,cursor ? cursor->timestamp : 0 ,MSGAllowResponseHeaders);
                                    }
                                    else{
                                        sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nTransfer-Encoding: chunked\r\nAccess-Control-Allow-Origin: %s\r\nTimestamp: %f\r\nAccess-Control-Expose-Headers: %s\r\n\r\n"
                                                               ,MSGAllowOrigin,cursor ? cursor->timestamp : 0,MSGAllowResponseHeaders);
                                    }
                                    
                                    stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                    
                                }
                                
                                if(cursor){
                                
                                    while ((entity = ( * MSGServerProcess.databaseClass->cursorNext)(database,cursor,&dbuf))) {
                                        
            
                                        sbuf.length = snprintf(sbuf.data, sbuf.size,"Content-Type: %s\r\nContent-Length: %u\r\nTimestamp: %f\r\nFrom: %s\r\nResource-URI:%s\r\n\r\n"
                                                               ,entity->type,entity->length,entity->timestamp,entity->user,entity->uri);
                                        
                                        sbuf.length = snprintf(sbuf.data, sbuf.size, "%x\r\nContent-Type: %s\r\nContent-Length: %u\r\nTimestamp: %f\r\nFrom: %s\r\nResource-URI:%s\r\n\r\n"
                                                               ,sbuf.length + entity->length + 2, entity->type,entity->length,entity->timestamp,entity->user,entity->uri);
                                        
                                        MSGBufferExpandSize(&sbuf, sbuf.length + entity->length + 8);
                                        
                                        if(entity->length){
                                            memcpy(sbuf.data + sbuf.length, (hchar *)entity + sizeof(MSGDatabaseEntity), entity->length);
                                            sbuf.length += entity->length;
                                        }
                                        
                                        memcpy(sbuf.data + sbuf.length, "\r\n\r\n", 4);
                                        
                                        sbuf.length += 4;
                                        
                                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                        
                                        if(state == StreamStateOK){
                                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                        }
                                        else{
                                            break;
                                        }
                                    
                                    }
                                    
                                    ( * MSGServerProcess.databaseClass->cursorClose)(database,cursor);
                                }

                                
                                (* MSGServerProcess.databaseClass->close) (database);
                                
                                sbuf.length = snprintf(sbuf.data, sbuf.size,"0\r\n\r\n");
                                
                                state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                
                                if(state == StreamStateOK){
                                    stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                }
                            }
                        }
                        else{
                            
                            state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                            
                            if(state == StreamStateOK){
                                
                                if(* auth->cookie != 0){
                                    sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nSet-Cookie: %s\r\nContent-Length: 0\r\nAccess-Control-Allow-Origin: %s\r\n\r\n"
                                                           ,auth->cookie,MSGAllowOrigin);
                                }
                                else{
                                    sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 0\r\nAccess-Control-Allow-Origin: %s\r\n\r\n",MSGAllowOrigin);
                                }
                                
                                stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                
                            }
                        }
                        

                        
                        (* MSGServerProcess.authClass->release)(auth);
                    }
                    else{
                        
                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                        
                        if(state == StreamStateOK){
                            
                            sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 401 Authorization Required\r\nWWW-Authenticate: Basic\r\nAccess-Control-Allow-Origin: %s\r\n\r\n",MSGAllowOrigin);
                            
                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                            
                        }
                    }
                    
                }
                else if(MSGStringEqual(&sbuf, request.method, "OPTIONS")){
                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                    
                    if(state == StreamStateOK){
                        
                        sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: %s\r\nAccess-Control-Allow-Methods: %s\r\nAccess-Control-Allow-Headers: %s\r\nContent-Length: 0\r\nAccess-Control-Expose-Headers: %s\r\n\r\n",MSGAllowOrigin,MSGAllowMethod,MSGAllowHeaders,MSGAllowResponseHeaders);
                        
                        stream_socket_write(thread->client, sbuf.data, sbuf.length);
                        
                    }
                }
                else {
                    
                    while(state == StreamStateOK){
                        
                        state = stream_socket_has_data(thread->client, REQUEST_TIMEOUT);
                        
                        if(state != StreamStateOK){
                            break;
                        }
                        
                        stream_socket_read(thread->client,sbuf.data , sbuf.size);
                    }
                    
                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                    
                    if(state == StreamStateOK){
                        
                        sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 405 Method Not Allowed\r\n\r\n");
                        
                        stream_socket_write(thread->client, sbuf.data, sbuf.length);
                        
                    }
                }
            }
            else{
                
                state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                
                if(state == StreamStateOK){
                    
                    sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 400 Bad Request\r\n\r\n");
                    
                    stream_socket_write(thread->client, sbuf.data, sbuf.length);
                    
                }
                
            }
            
            stream_socket_close(thread->client);
            thread->client = 0;
            idle = 0;
            sleep_v = 0;
        }
        else if(idle > 300000000){
            break;
        }
        else{
            sleep_v += 200;
            if(sleep_v > 200000){
                sleep_v = 200000;
            }
            usleep(sleep_v);
            idle += sleep_v;
        }
    }
    
    if(sbuf.data){
        free(sbuf.data);
    }
    
    if(dbuf.data){
        free(dbuf.data);
    }
    
    if(request.headers.data){
        free(request.headers.data);
    }
    
    thread->pthread = 0;
    pthread_exit(NULL);
    
    return NULL;
}

static int MSGServerProcessCreate (SRVServer * server,SRVProcess * process){
    
    memset(&MSGServerProcess, 0, sizeof(MSGServerProcess));
    
    MSGServerProcess.maxContentLength  = MSGServerProcessDefaultMaxContentLength;
    
    {
        int i=0;
        for(i=1;i<server->config.arg.argc;i++){
            if(strcmp(server->config.arg.args[i], "-maxContentLength") ==0 && i +1 < server->config.arg.argc){
                i ++;
                MSGServerProcess.maxContentLength = atoi(server->config.arg.args[i]);
                if(MSGServerProcess.maxContentLength == 0){
                    MSGServerProcess.maxContentLength = MSGServerProcessDefaultMaxContentLength;
                }
            }
        }
    }

    process->exit = 0;
    
    return 1;
}

static void MSGServerProcessExit (SRVServer * server,SRVProcess * process){
    printf("MSGServerProcessExit %d %d\n",process->pid,errno);
}

static void MSGServerProcessOpen (SRVServer * server,SRVProcess * process){
    
    MSGServerProcess.authClass = & MSGAuthDefaultClass;
    MSGServerProcess.databaseClass = & MSGDatabaseDefaultClass;
    
}

static double MSGServerProcessTick (SRVServer * server,SRVProcess * process){
    
    int i = 0;
    int length = MSGServerProcess.endIndex >= MSGServerProcess.startIndex
    ? MSGServerProcess.endIndex - MSGServerProcess.startIndex : (MSGServerProcessClientMaxCount - MSGServerProcess.startIndex) + MSGServerProcess.endIndex;
    int client;
    struct sockaddr_in addr;
    socklen_t socklen = sizeof(addr);
    MSGServerProcessThread * thread;
    
    MSGServerProcess.sleep_v += 0.0005;
    
    if(MSGServerProcess.sleep_v > 0.1){
        MSGServerProcess.sleep_v = 0.1;
    }
    
    if(length < MSGServerProcessClientMaxCount){
        socklen = sizeof(struct sockaddr_in );
        client = SRVServerAccept(server,0.002,(struct sockaddr *)&addr,& socklen);
        if(client){
            MSGServerProcess.clients[MSGServerProcess.endIndex] = client;
            MSGServerProcess.endIndex = (MSGServerProcess.endIndex + 1) % MSGServerProcessClientMaxCount;
            MSGServerProcess.sleep_v = 0.0;
        }
    }
    
    i = 0;
    
    while(i < MSGServerProcessClientMaxThread
          && MSGServerProcess.startIndex != MSGServerProcess.endIndex){
        
        thread = MSGServerProcess.threads + i;
        
        if(thread->pthread ==0){
            pthread_create(&thread->pthread, NULL, MSGServerProcessThreadRun, thread);
        }
        
        if(thread->client ==0){
            thread->client = MSGServerProcess.clients[MSGServerProcess.startIndex];
            MSGServerProcess.startIndex = (MSGServerProcess.startIndex + 1) % MSGServerProcessClientMaxCount;
            MSGServerProcess.sleep_v = 0.0;
        }
        
        i++;
    }
    
    return MSGServerProcess.sleep_v;
}

static void MSGServerProcessClose (SRVServer * server,SRVProcess * process){
    
    int i = 0;
    int client;
    pthread_t p;
    
    MSGServerProcess.processExit = 1;
    
    while(i < MSGServerProcessClientMaxCount){
        p = MSGServerProcess.threads[i].pthread;
        if(p){
            pthread_cancel(p);
            pthread_join(p, NULL);
        }
        i++;
    }
    
    while(MSGServerProcess.startIndex != MSGServerProcess.endIndex){
        client = MSGServerProcess.clients[MSGServerProcess.startIndex];
        if(client){
            stream_socket_close(client);
        }
        MSGServerProcess.startIndex = (MSGServerProcess.startIndex + 1 ) % MSGServerProcessClientMaxCount;
    }
    
}

SRVProcessClass MSGServerProcessClass = {
    MSGServerProcessCreate,
    MSGServerProcessExit,
    MSGServerProcessOpen,
    MSGServerProcessTick,
    MSGServerProcessClose
};

