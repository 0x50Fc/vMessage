//
//  MSGAuth.c
//  vMessageServer
//
//  Created by zhang hailong on 13-6-21.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#include "hconfig.h"
#include "MSGAuth.h"

#include "hbuffer.h"
#include "hbase64.h"
#include "hinifile.h"

#include "hserver.h"

#include "MSGDatabase.h"

#include "MSG_CGI.h"

static MSGAuth * MSGAuthDefaultCreate (struct _MSGAuthClass * clazz,MSGHttpRequest * request,MSGBuffer * sbuf){
    
    MSGAuth * auth = NULL;
    MSGHttpHeader * h;
    hcchar * path = getenv(MSG_DEFAULT_PATH_ENV);
    hchar b[MSG_USER_SIZE * 2], * user, * password,buf[PATH_MAX],*p,*t;
    hchar didWritedEntityExec[PATH_MAX] = {0};
    huint32 len,c;
    struct stat s;
    hinifile_t cfg;
    InvokeTickBegin
    
    if(path == NULL){
        return NULL;
    }
    
    h = MSGHttpRequestGetHeader(request, sbuf, "Authorization");
    
    if(h){
        if(strncmp(sbuf->data + h->value.location, "Basic ", 6) ==0){
            
            len = hbase64_decode_bytes(sbuf->data + h->value.location + 6,  h->value.length - 6
                                       , b, sizeof(b) -1);
            
            b[len] = 0;
            
            user = b;
            
            password = b;
            
            while(* password != '\0'){
                if( * password == ':'){
                    * password = 0;
                    password ++ ;
                    break;
                }
                password ++;
            }
            
            snprintf(buf, sizeof(buf),"%s/%s",path,user);
            
            SRVServerLog("\nAuth Path:\n");
            SRVServerLog("%s\n",buf);
            
            if(stat(buf, &s) != -1 && S_ISDIR(s.st_mode)){
                
                snprintf(buf, sizeof(buf), "%s/%s/cfg.ini",path,user);
                
                cfg = inifile_alloc(buf);
                
                if(cfg){
                    
                    while(inifile_read(cfg)){
                        
                        if(strcmp(inifile_section(cfg),"AUTH") ==0){
                            if(strcmp(inifile_key(cfg), "password") ==0){
                                if (strcmp(password, inifile_value(cfg)) != 0) {
                                    inifile_dealloc(cfg);
                                    return NULL;
                                }
                            }
                            
                        }
                        else if(strcmp(inifile_section(cfg), "LISTENER") ==0){
                            if(strcmp(inifile_key(cfg), "didWritedEntityExec") ==0){
                                strncpy(didWritedEntityExec, inifile_value(cfg), sizeof(didWritedEntityExec));
                            }
                        }
                        
                    }
                    
                    inifile_dealloc(cfg);
                }
                else{
                    return NULL;
                }
                
                SRVServerLog("\ninifile OK\n");
                
                auth = (MSGAuth *) malloc(sizeof(MSGAuth));
                memset(auth, 0, sizeof(MSGAuth));
                auth->clazz = &MSGAuthDefaultClass;
                strncpy(auth->user, user,sizeof(auth->user));
                strncpy(auth->didWritedEntityExec, didWritedEntityExec, sizeof(auth->didWritedEntityExec));
                
                t = auth->to;
                c = sizeof(auth->to) -1;
                
                p = sbuf->data + request->path.location;
                
                len = request->path.length;
                
                if(*p == '/'){
                    p ++;
                    len --;
                }
                
                while (len >0 && c > 0) {
                    
                    if(*p == '/'){
                        break;
                    }
                    
                    *t = *p;
                    t ++;
                    c --;
                    
                    len --;
                    p ++;
                }
                
                SRVServerLog("\nMSGAuth OK\n");
                
            }
        }
    }
    
    return auth;
}

static void MSGAuthDefaultRelease (MSGAuth * auth){
    free(auth);
}

static void MSGAuthDefaultDidWritedEntity (MSGAuth * auth,struct _MSGDatabaseEntity * entity){
    
    if(* auth->didWritedEntityExec){
        
        MSGAuth a;
        MSGDatabaseEntity e;
        pid_t pid;
        char sbuf[64];
        
        memcpy(&a,auth, sizeof(MSGAuth));
        memcpy(&e,entity, sizeof(MSGDatabaseEntity));
        
        SRVServerLog("\nMSGAuthDefaultDidWritedEntity\n");
        SRVServerLog("\n%s\n",a.didWritedEntityExec);
        
        if(( pid = fork() ) < 0 )
        {

        }
        else if(pid == 0)
        {
            
            setenv(MSG_ENV_USER, a.user, 1);
            setenv(MSG_ENV_TO, a.to, 1);
            setenv(MSG_ENV_COOKIE, a.cookie, 1);
            setenv(MSG_ENV_ENTITY_USER, e.user, 1);
            setenv(MSG_ENV_ENTITY_URI, e.uri, 1);
            setenv(MSG_ENV_ENTITY_CONTENT_TYPE, e.type, 1);
            sprintf(sbuf, "%f",e.timestamp);
            setenv(MSG_ENV_ENTITY_TIMESTAMP, sbuf, 1);
            
            execl(a.didWritedEntityExec, NULL);
            
            exit(EXIT_SUCCESS);
        }
        

    }
}

MSGAuthClass MSGAuthDefaultClass = {
    MSGAuthDefaultCreate,
    MSGAuthDefaultRelease,
    MSGAuthDefaultDidWritedEntity,
};

