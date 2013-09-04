//
//  MSGAuth.h
//  vMessageServer
//
//  Created by zhang hailong on 13-6-21.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#ifndef vMessageServer_MSGAuth_h
#define vMessageServer_MSGAuth_h



#ifdef __cplusplus
extern "C" {
#endif
    
#include "MSG.h"
    
#define MSG_DEFAULT_PATH_ENV   "MSGPATH"
#define MSG_AUTO_COOKIE_SIZE        512
#define MSG_USER_SIZE               128
    
    struct _MSGAuthClass;
    struct _MSGDatabaseEntity;
    
    typedef struct _MSGAuth {
        struct _MSGAuthClass * clazz;
        hchar cookie[MSG_AUTO_COOKIE_SIZE];
        hchar user[MSG_USER_SIZE];
        hchar to[MSG_USER_SIZE];
        hchar didWritedEntityExec[PATH_MAX];
    } MSGAuth;
    
    typedef MSGAuth * (* MSGAuthCreate) (struct _MSGAuthClass * clazz,MSGHttpRequest * request,MSGBuffer * sbuf);
    
    typedef void (* MSGAuthRelease) (MSGAuth * auth);
    
    typedef void (* MSGAuthDidWritedEntity) (MSGAuth * auth,struct _MSGDatabaseEntity * entity);
    
    typedef struct _MSGAuthClass {
        MSGAuthCreate create;
        MSGAuthRelease release;
        MSGAuthDidWritedEntity didWritedEntity;
    } MSGAuthClass;
    
    extern MSGAuthClass MSGAuthDefaultClass;
    
#ifdef __cplusplus
}
#endif

#endif
