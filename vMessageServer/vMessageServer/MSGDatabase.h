//
//  MSGDatabase.h
//  vMessageServer
//
//  Created by zhang hailong on 13-6-21.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#ifndef vMessageServer_MSGDatabase_h
#define vMessageServer_MSGDatabase_h


#ifdef __cplusplus
extern "C" {
#endif
    
#include "MSGAuth.h"
    
#define MSG_DATABASE_ENTITY_TYPE_SIZE   64
#define MSG_DATABASE_CURSOR_INDEX_SIZE  1024

    struct _MSGDatabaseClass;
    
    typedef struct _MSGDatabaseEntity {
        hchar type[MSG_DATABASE_ENTITY_TYPE_SIZE];
        hchar user[MSG_USER_SIZE];
        huint32 length;
        hdouble timestamp;
    } MSGDatabaseEntity;
    
    typedef struct _MSGDatabaseResult {
        hint32 statusCode;
        hchar status[64];
    } MSGDatabaseResult;
    
    typedef struct _MSGDatabase {
        struct _MSGDatabaseClass * clazz;
    } MSGDatabase;
    
    typedef struct _MSGDatabaseIndex {
        hdouble timestamp;
        huint32 location;
    } MSGDatabaseIndex;
    
    typedef struct _MSGDatabaseCursor {
        MSGDatabaseIndex * indexes;
        huint32 size;
        huint32 length;
        huint32 index;
        huint32 location;
        hdouble timestamp;
    } MSGDatabaseCursor;
    
    typedef MSGDatabase * (* MSGDatabaseOpen) (struct _MSGDatabaseClass * clazz,MSGHttpRequest * request,MSGBuffer * sbuf);
    
    typedef void (* MSGDatabaseClose) (MSGDatabase * database);
    
    typedef MSGDatabaseResult (* MSGDatabaseWrite) (MSGDatabase * database,MSGAuth * auth,MSGHttpRequest * request,MSGBuffer * sbuf,huint32 dataOffset,MSGBuffer * dbuf);
    
    typedef MSGDatabaseCursor * (* MSGDatabaseCursorOpen) (MSGDatabase * database,MSGAuth * auth,MSGHttpRequest * request,MSGBuffer * sbuf);
    
    typedef MSGDatabaseEntity * (* MSGDatabaseCursorNext) (MSGDatabase * database,MSGDatabaseCursor * cursor,MSGBuffer * dbuf);
    
    typedef void (* MSGDatabaseCursorClose) (MSGDatabase * database,MSGDatabaseCursor * cursor);
    
    typedef struct _MSGDatabaseClass {
        MSGDatabaseOpen open;
        MSGDatabaseClose close;
        MSGDatabaseWrite write;
        MSGDatabaseCursorOpen cursorOpen;
        MSGDatabaseCursorNext cursorNext;
        MSGDatabaseCursorClose cursorClose;
    } MSGDatabaseClass;
    
    extern MSGDatabaseResult MSGDatabaseResultOK;
    
    extern MSGDatabaseResult MSGDatabaseResultWriteError;
    
    extern MSGDatabaseClass MSGDatabaseDefaultClass;
    
#ifdef __cplusplus
}
#endif

#endif
