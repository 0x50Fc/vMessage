//
//  MSGDatabase.c
//  vMessageServer
//
//  Created by zhang hailong on 13-6-21.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#include "hconfig.h"
#include "MSGDatabase.h"


MSGDatabaseResult MSGDatabaseResultOK = {200,"OK"};

MSGDatabaseResult MSGDatabaseResultWriteError = {601,"write error"};

static MSGDatabase gMSGDatabaseDefault = {&MSGDatabaseDefaultClass};

static MSGDatabase * MSGDatabaseDefaultOpen (struct _MSGDatabaseClass * clazz){
    return &gMSGDatabaseDefault;
}

static void  MSGDatabaseDefaultClose (MSGDatabase * database){
    
}

static MSGDatabaseResult MSGDatabaseDefaultWrite (MSGDatabase * database,MSGAuth * auth,MSGHttpRequest * request,MSGBuffer * sbuf,huint32 dataOffset,MSGBuffer * dbuf){
    
    MSGDatabaseResult rs = MSGDatabaseResultOK;
    
    huint32 size = sbuf->length - dataOffset + sizeof(MSGDatabaseEntity);
    
    MSGDatabaseEntity * entity;
    
    MSGHttpHeader *h ;
    
    hchar path[PATH_MAX],* dir = getenv(MSG_DEFAULT_PATH_ENV);
    
    struct stat s;
    struct timeval tm;
    
    int fno;
    
    assert(dir);
    
    MSGBufferExpandSize(dbuf, size );
    
    entity = (MSGDatabaseEntity *) dbuf->data;
    
    memset(dbuf->data, 0,size);
    
    h = MSGHttpRequestGetHeader(request, sbuf, "Content-Type");
    
    if(h){
        strncpy(entity->type, sbuf->data + h->value.location, MIN(h->value.length, sizeof(entity->type)));
    }
    strncpy(entity->user, auth->user, sizeof(entity->user));
    entity->length = size - sizeof(MSGDatabaseEntity);
    
    gettimeofday(& tm, NULL);
    
    entity->timestamp = (hdouble)tm.tv_sec + (hdouble) tm.tv_usec / 1000000.0;
    
    memcpy(dbuf->data + sizeof(MSGDatabaseEntity), sbuf->data + dataOffset, entity->length );
    
    snprintf(path, sizeof(path),"%s/%s/db",dir,auth->to);
    
    if(stat(path, &s) == -1){
        mkdir(path, S_IRUSR | S_IWUSR);
    }
    
    strcat(path, "/w.db");
    
    fno = open(path, O_WRONLY | O_APPEND);
    
    if(fno == -1){
        fno = open(path, O_WRONLY | O_CREAT);
        if(fno != -1){
            fchmod(fno, S_IRUSR | S_IWUSR);
            close(fno);
        }
    }
    
    fno = open(path, O_WRONLY | O_APPEND);
    
    if(fno != -1){
        
        if(size != write(fno, dbuf->data, size)){
            rs = MSGDatabaseResultWriteError;
        }
        
        close(fno);
    }
    else{
        rs = MSGDatabaseResultWriteError;
    }
    
    return MSGDatabaseResultOK;
}

typedef struct {
    MSGDatabaseCursor base;
    int dbfno;
    int idxfno;
    huint32 length;
} MSGDatabaseCursorImpl;

static MSGDatabaseCursor * MSGDatabaseDefaultCursorOpen (MSGDatabase * database,MSGAuth * auth,MSGHttpRequest * request,MSGBuffer * sbuf){
    
    hchar path[PATH_MAX],* dir = getenv(MSG_DEFAULT_PATH_ENV);
    int dbfno,idxfno;
    huint32 len;
    struct stat s;
    hdouble timestamp = 0.0;
    MSGDatabaseCursorImpl * cursor = NULL;
    MSGDatabaseEntity entity;
    MSGDatabaseIndex index;
    
    assert(dir);
    
    snprintf(path, sizeof(path),"%s/%s/db/w.db",dir,auth->user);
    
    dbfno = open(path, O_RDONLY);
    
    if(dbfno == -1){
        return NULL;
    }
    
    snprintf(path, sizeof(path),"%s/%s/db/r.db",dir,auth->user);
    
    idxfno = open(path, O_RDWR);
    
    if(idxfno == -1){
        idxfno = open(path, O_WRONLY | O_CREAT);
        if(idxfno != -1){
            fchmod(idxfno, S_IRUSR | S_IWUSR);
            close(idxfno);
        }
        idxfno = open(path, O_RDWR);
    }
    
    if(idxfno == -1){
        close(dbfno);
        return NULL;
    }
    
    flock(idxfno, LOCK_EX);
    
    if(-1 == fstat(idxfno, &s)){
        flock(idxfno, LOCK_UN);
        close(idxfno);
        close(dbfno);
        return NULL;
    }
    
    if(request->path.length > 1){
        strncpy(path, sbuf->data + request->path.location + 1, request->path.length - 1);
        timestamp = atof(path);
    }
    
    len = (huint32) (s.st_size / sizeof(MSGDatabaseIndex));
    
    if(len == 0){
        lseek(idxfno, 0, SEEK_END);
        lseek(dbfno, 0, SEEK_SET);
    }
    else{
        lseek(idxfno, (len -1) * sizeof(MSGDatabaseIndex), SEEK_SET);
        if(read(idxfno, &index, sizeof(MSGDatabaseIndex)) != sizeof(MSGDatabaseIndex)){
            flock(idxfno, LOCK_UN);
            close(idxfno);
            close(dbfno);
            return NULL;
        }
        if(timestamp == 0.0){
            timestamp = index.timestamp;
        }
        lseek(dbfno, index.location, SEEK_SET);
        if(read(dbfno, & entity,sizeof(MSGDatabaseEntity)) == sizeof(MSGDatabaseEntity)){
            lseek(dbfno, entity.length, SEEK_CUR);
        }
    }
    
    while(read(dbfno, &entity, sizeof(MSGDatabaseEntity)) == sizeof(MSGDatabaseEntity)){
        
        index.location = (huint32)( lseek(dbfno, 0, SEEK_CUR) - sizeof(MSGDatabaseEntity) );
        index.timestamp = entity.timestamp;
        
        write(idxfno, & index, sizeof(MSGDatabaseIndex));
        
        lseek(dbfno, entity.length, SEEK_CUR);
        
        len ++;
    }
    
    cursor = (MSGDatabaseCursorImpl *) malloc(sizeof(MSGDatabaseCursorImpl));
    
    memset(cursor, 0, sizeof(MSGDatabaseCursorImpl));
    
    cursor->dbfno = dbfno;
    cursor->idxfno = idxfno;
    cursor->length = len;
    cursor->base.size = MSG_DATABASE_CURSOR_INDEX_SIZE;
    cursor->base.indexes = (MSGDatabaseIndex *) malloc(sizeof(MSGDatabaseIndex) * cursor->base.size);
    
    if( timestamp == 0.0){
        
        lseek(idxfno, 0, SEEK_SET);
        len = (huint32)read(idxfno, cursor->base.indexes, cursor->base.size * sizeof(MSGDatabaseIndex));
        cursor->base.length = len / sizeof(MSGDatabaseIndex);
        
        if(cursor->base.length ==0){
            flock(idxfno, LOCK_UN);
            close(idxfno);
            close(dbfno);
            free(cursor->base.indexes);
            free(cursor);
            return NULL;
        }
        
    }
    else{
        
        if(cursor->base.size < cursor->length){
            cursor->base.location = cursor->length - cursor->base.size;
        }
        
        while(1){

            lseek(idxfno, cursor->base.location * sizeof(MSGDatabaseIndex), SEEK_SET);
            
            len = (huint32)read(idxfno, cursor->base.indexes, cursor->base.size * sizeof(MSGDatabaseIndex));
            
            cursor->base.length = len / sizeof(MSGDatabaseIndex);
        
            if(cursor->base.length ==0){
                flock(idxfno, LOCK_UN);
                close(idxfno);
                close(dbfno);
                free(cursor->base.indexes);
                free(cursor);
                return NULL;
            }
            
            if(timestamp < cursor->base.indexes->timestamp){
                
                if(cursor->base.location == 0){
                    break;
                }
                
                if(cursor->base.location > cursor->base.size){
                    cursor->base.location -= cursor->base.size;
                }
                else {
                    cursor->base.location = 0;
                }

            }
            else if(timestamp >= cursor->base.indexes[cursor->base.length - 1].timestamp){
                flock(idxfno, LOCK_UN);
                close(idxfno);
                close(dbfno);
                free(cursor->base.indexes);
                free(cursor);
                return NULL;
            }
            else{
                {
                    int b = 0,e = cursor->base.length - 1,i;
                    while (b < e) {
                        i = (b + e) / 2;
                        
                        if(cursor->base.indexes[i].timestamp > timestamp){
                            e = i - 1;
                        }
                        else {
                            b = i + 1;
                        }
                    }
                    cursor->base.index = b;
                    cursor->base.length -= cursor->base.index;
                    break;
                }
            }
            
        }
    }
    return (MSGDatabaseCursor *)cursor;
}

static MSGDatabaseEntity * MSGDatabaseDefaultCursorNext (MSGDatabase * database,MSGDatabaseCursor * cursor,MSGBuffer * dbuf){
    
    MSGDatabaseCursorImpl * cur = (MSGDatabaseCursorImpl *) cursor;
    MSGDatabaseEntity entity;
    huint32 len;
    
    if(cursor->length ==0){
        
        if(cursor->index == 0){
            return NULL;
        }
        
        lseek(cur->idxfno, cur->base.location + cursor->index * sizeof(MSGDatabaseIndex), SEEK_SET);
        
        len = (huint32)read(cur->idxfno, cur->base.indexes, cur->base.size * sizeof(MSGDatabaseIndex));
        
        cursor->length = len / sizeof(MSGDatabaseIndex);
        cursor->index = 0;
        
        if(cursor->length == 0){
            return NULL;
        }
        
    }
    
    lseek(cur->dbfno, cursor->indexes[cursor->index].location, SEEK_SET);
    
    if(read(cur->dbfno, &entity, sizeof(MSGDatabaseEntity)) != sizeof(MSGDatabaseEntity)){
        return NULL;
    }
    
    MSGBufferExpandSize(dbuf, sizeof(MSGDatabaseEntity) + entity.length);
    
    memcpy(dbuf->data, &entity, sizeof(MSGDatabaseEntity));
    
    if(read(cur->dbfno,dbuf->data + sizeof(MSGDatabaseEntity), entity.length) != entity.length){
        return NULL;
    }
    
    cursor->index ++;
    cursor->length --;
    
    return (MSGDatabaseEntity *) dbuf->data;
}

static void MSGDatabaseDefaultCursorClose (MSGDatabase * database,MSGDatabaseCursor * cursor){
   
    MSGDatabaseCursorImpl * cur = (MSGDatabaseCursorImpl *) cursor;
    
    if(cursor->indexes){
        free(cursor->indexes);
    }
    
    flock(cur->idxfno, LOCK_UN);
    
    close(cur->idxfno);
    close(cur->dbfno);
    
    free(cursor);
}

MSGDatabaseClass MSGDatabaseDefaultClass = {
    MSGDatabaseDefaultOpen,
    MSGDatabaseDefaultClose,
    MSGDatabaseDefaultWrite,
    MSGDatabaseDefaultCursorOpen,
    MSGDatabaseDefaultCursorNext,
    MSGDatabaseDefaultCursorClose
};

