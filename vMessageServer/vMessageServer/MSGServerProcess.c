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

#define MSGServerProcessThreadBufferSize    10240

#define REQUEST_TIMEOUT     120
#define RESPONSE_TIMEOUT     120


#define MSGServerProcessDefaultMaxContentLength    102400

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
                    
                    contentLength = MSGHttpRequestHeaderIntValue(&request, &sbuf, "Content-Type", 0);
                    
                    if(contentLength == -0){
                        
                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                        
                        if(state == StreamStateOK){
                            
                            sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 411 Length Required\r\n\r\n");
                            
                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                            
                        }
                        
                    }
                    else if(contentLength > MSGServerProcess.maxContentLength){
                        
                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                        
                        if(state == StreamStateOK){
                            
                            sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 413 Request Entity Too Large\r\n\r\n");
                            
                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                            
                        }
                    }
                    else {
                        
                        auth = (* MSGServerProcess.authClass->create)(MSGServerProcess.authClass,& request,& sbuf);
                        
                        if(auth){
                        
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
                            
                            if(state == StreamStateOK){
                                
                                database = (* MSGServerProcess.databaseClass->open)(MSGServerProcess.databaseClass);
                                
                                if(database){
                                 
                                    dbResult = ( * MSGServerProcess.databaseClass->write)(database,auth,& request,& sbuf,offset,&dbuf);
                                    
                                    
                                    (* MSGServerProcess.databaseClass->close)(database);
                                    
                                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                    
                                    if(state == StreamStateOK){
                                        
                                        sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 %d %s\r\n\r\n",dbResult.statusCode,dbResult.status);
                                        
                                        stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                        
                                    }
                                }
                                else{
                                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                    
                                    if(state == StreamStateOK){
                                        
                                        sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 507 Insufficient Storage\r\n\r\n");
                                        
                                        stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                        
                                    }
                                }
                            }
                            else{
                                
                                state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                
                                if(state == StreamStateOK){
                                    
                                    sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 408 Request Timeout\r\n\r\n");
                                    
                                    stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                    
                                }
                                
                            }
                            
                            (* MSGServerProcess.authClass->release)(auth);
                        }
                        else{
                            
                            state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                            
                            if(state == StreamStateOK){
                                
                                sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 401 Authorization Required\r\nWWW-Authenticate: Basic\r\n\r\n");
                                
                                stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                
                            }
                            
                        }
                        
                    }
                    while(state == StreamStateOK){
                        
                        state = stream_socket_has_data(thread->client, REQUEST_TIMEOUT);
                        
                        if(state != StreamStateOK){
                            break;
                        }
                        
                        stream_socket_read(thread->client,sbuf.data , sbuf.size);
                    }
                    
                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                    
                    if(state == StreamStateOK){
                        
                        sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nPOST OK");
                        
                        stream_socket_write(thread->client, sbuf.data, sbuf.length);
                        
                    }
                    
                }
                else if(MSGStringEqual(&sbuf, request.method, "GET")){
                    
                    //write(STDOUT_FILENO, sbuf.data, sbuf.length);
                    
                    auth = (* MSGServerProcess.authClass->create)(MSGServerProcess.authClass,& request,& sbuf);
                    
                    if(auth){

                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                        
                        if(state == StreamStateOK){
                            
                            if(* auth->cookie != 0){
                                sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nSet-Cookie: %s\r\nTransfer-Encoding: chunked\r\n\r\n"
                                                       ,auth->cookie);
                            }
                            else{
                                sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nTransfer-Encoding: chunked\r\n\r\n");
                            }
                            
                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                            
                        }
                        
                        database = (* MSGServerProcess.databaseClass->open) (MSGServerProcess.databaseClass);
                        
                        if(database){
                        
                            MSGDatabaseCursor * cursor = ( * MSGServerProcess.databaseClass->cursorOpen)(database,auth,&request,&sbuf);
                            MSGDatabaseEntity * entity;
    
                            if(cursor){
                            
                                while ((entity = ( * MSGServerProcess.databaseClass->cursorNext)(database,cursor,&dbuf))) {
                                    
        
                                    sbuf.length = snprintf(sbuf.data, sbuf.size,"Content-Type: %s\r\nTimestamp: %f\r\n\r\n"
                                                           ,entity->type,entity->timestamp);
                                    
                                    sbuf.length = snprintf(sbuf.data, sbuf.size, "%x\r\nContent-Type: %s\r\nTimestamp: %f\r\n\r\n"
                                                           ,sbuf.length + entity->length, entity->type,entity->timestamp);
                                    
                                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                    
                                    if(state == StreamStateOK){
                                        stream_socket_write(thread->client, sbuf.data, sbuf.length);
                                    }
                                    else{
                                        break;
                                    }
                                    
                                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                    
                                    if(state == StreamStateOK){
                                        stream_socket_write(thread->client, dbuf.data + sizeof(MSGDatabaseEntity), sbuf.length - sizeof(MSGDatabaseEntity));
                                    }
                                    else{
                                        break;
                                    }
                                    
                                    sbuf.length = snprintf(sbuf.data, sbuf.size, "\r\n");
                                    
                                    state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                                    
                                    if(state == StreamStateOK){
                                        stream_socket_write(thread->client, dbuf.data + sizeof(MSGDatabaseEntity), sbuf.length - sizeof(MSGDatabaseEntity));
                                    }
                                    else{
                                        break;
                                    }
                                    
                                }
                                
                                ( * MSGServerProcess.databaseClass->cursorClose)(database,cursor);
                            }

                            
                            (* MSGServerProcess.databaseClass->close) (database);
                        }
                        
                        sbuf.length = snprintf(sbuf.data, sbuf.size,"0\r\n\r\n");
                        
                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                        
                        if(state == StreamStateOK){
                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                        }
                        
                        (* MSGServerProcess.authClass->release)(auth);
                    }
                    else{
                        
                        state = stream_socket_has_space(thread->client, RESPONSE_TIMEOUT);
                        
                        if(state == StreamStateOK){
                            
                            sbuf.length = snprintf(sbuf.data, sbuf.size,"HTTP/1.1 401 Authorization Required\r\nWWW-Authenticate: Basic\r\n\r\n");
                            
                            stream_socket_write(thread->client, sbuf.data, sbuf.length);
                            
                        }
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
    
    while(i < MSGServerProcessClientMaxCount
          && MSGServerProcess.startIndex != MSGServerProcess.endIndex){
        
        while(i < MSGServerProcessClientMaxCount){
            thread = MSGServerProcess.threads + i;
            if(thread->pthread ==0){
                pthread_create(&thread->pthread, NULL, MSGServerProcessThreadRun, thread);
            }
            
            if(thread->client ==0){
                thread->client = MSGServerProcess.clients[MSGServerProcess.startIndex];
                MSGServerProcess.startIndex = (MSGServerProcess.startIndex + 1) % MSGServerProcessClientMaxCount;
                MSGServerProcess.sleep_v = 0.0;
                break;
            }
            i++;
        }
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

