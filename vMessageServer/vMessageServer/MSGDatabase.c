//
//  MSGDatabase.c
//  vMessageServer
//
//  Created by zhang hailong on 13-6-21.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#include "hconfig.h"
#include "MSGDatabase.h"

#include "hserver.h"

static inline int os_file_open(const char * path,int flags,mode_t mode){
    int rc;
    do{ rc = open(path, flags,mode); } while( rc<0 && errno==EINTR );
    return rc;
}

static inline void os_file_close(int file){
    int rc;
    do{ rc = close(file); } while( rc<0 && errno==EINTR );
}

static inline int os_file_lock(int file,int op){
    int rc;
    do{ rc = flock(file,op); }while( rc<0 && errno==EINTR );
    return rc;
}

static inline ssize_t os_file_read(int file,void * bytes,size_t length){
    ssize_t rc;
    do{ rc = read(file, bytes, length); }while( rc<0 && errno==EINTR );
    return rc;
}

static inline ssize_t os_file_write(int file,void * bytes,size_t length){
    ssize_t rc;
    do{ rc = write(file, bytes, length); }while( rc<0 && errno==EINTR );
    return rc;
}

static inline off_t os_file_seek(int file,off_t off,int by){
    return lseek(file, off, by);
}

#define MODE S_IRWXU | S_IRWXG


MSGDatabaseResult MSGDatabaseResultOK = {200,"OK"};

MSGDatabaseResult MSGDatabaseResultWriteError = {601,"write error"};

static MSGDatabase gMSGDatabaseDefault = {&MSGDatabaseDefaultClass};

static MSGDatabase * MSGDatabaseDefaultOpen (struct _MSGDatabaseClass * clazz,MSGHttpRequest * request,MSGBuffer * sbuf){
    
    if (request->path.length > 1) {
        return &gMSGDatabaseDefault;
    }

    return NULL;
}

static void  MSGDatabaseDefaultClose (MSGDatabase * database){
    
}

static MSGDatabaseResult MSGDatabaseDefaultWrite (MSGDatabase * database,MSGAuth * auth
                                                  ,MSGHttpRequest * request,MSGBuffer * sbuf,huint32 dataOffset,MSGBuffer * dbuf,hcchar * uri){
    
    MSGDatabaseResult rs = MSGDatabaseResultOK;
    
    huint32 size = sbuf->length - dataOffset + sizeof(MSGDatabaseEntity);
    
    MSGDatabaseEntity * entity;
    
    MSGHttpHeader *h ;
    
    hchar path[PATH_MAX],* dir = getenv(MSG_DEFAULT_PATH_ENV);
    
    struct stat s;
    struct timeval tm;
    
    int fno;
    
    SRVServerLog("\nMSGDatabaseDefaultWrite\n");

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
    
    if(uri){
        strncpy(entity->uri, uri,sizeof(entity->uri));
    }
    
    entity->verify = MSGDatabaseEntityVerify(entity);
    
    memcpy(dbuf->data + sizeof(MSGDatabaseEntity), sbuf->data + dataOffset, entity->length );
    
    snprintf(path, sizeof(path),"%s/%s/db",dir,auth->to);
    
    if(stat(path, &s) == -1){
        mkdir(path, MODE);
    }
    
    strcat(path, "/w.db");
    
    fno = os_file_open(path, O_WRONLY | O_APPEND,MODE);
    
    if(fno == -1){
        
        fno = os_file_open(path, O_WRONLY | O_CREAT,MODE);
        
        if(fno != -1){
            os_file_close(fno);
        }
        
        fno = os_file_open(path, O_WRONLY | O_APPEND,MODE);
    }
    
    if(fno != -1){
        
        if(size != os_file_write(fno, dbuf->data, size)){
            rs = MSGDatabaseResultWriteError;
        }
        
        os_file_close(fno);
    }
    else{
        rs = MSGDatabaseResultWriteError;
    }
    
    (* auth->clazz->didWritedEntity)(auth,entity);
    
    SRVServerLog("\nMSGDatabaseDefaultWrite OK\n");
    
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
    
    SRVServerLog("\nMSGDatabaseDefaultCursorOpen\n");
    
    assert(dir);

    snprintf(path, sizeof(path),"%s/%s/db/w.db",dir,auth->user);
    
    dbfno = os_file_open(path, O_RDONLY,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    if(dbfno == -1){
        return NULL;
    }
    
    snprintf(path, sizeof(path),"%s/%s/db/r.db",dir,auth->user);
    
    idxfno = os_file_open(path,O_RDONLY,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    if(idxfno == -1){
        idxfno = os_file_open(path, O_WRONLY | O_CREAT,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if(idxfno != -1){
            os_file_close(idxfno);
        }
        idxfno = os_file_open(path,O_RDONLY,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    
    if(idxfno == -1){
        close(dbfno);
        return NULL;
    }
    
    os_file_lock(idxfno, LOCK_SH);
    
    if(-1 == fstat(idxfno, &s)){
        os_file_lock(idxfno, LOCK_UN);
        os_file_close(idxfno);
        os_file_close(dbfno);
        return NULL;
    }
    
    if(request->path.length > 1){
        sbuf->data[request->path.location + request->path.length] = 0;
        timestamp = atof(sbuf->data + request->path.location + 1);
    }
    
    len = (huint32) (s.st_size / sizeof(MSGDatabaseIndex));
    
    if(len == 0){
        
    }
    else{
        
        os_file_seek(idxfno, (len -1) * sizeof(MSGDatabaseIndex), SEEK_SET);
        
        if(read(idxfno, &index, sizeof(MSGDatabaseIndex)) != sizeof(MSGDatabaseIndex)){
            os_file_lock(idxfno, LOCK_UN);
            os_file_close(idxfno);
            os_file_close(dbfno);
            return NULL;
        }
        
        if(timestamp == 0.0){
            timestamp = index.timestamp;
        }
        
        os_file_seek(dbfno, index.location, SEEK_SET);
        
        if(os_file_read(dbfno, & entity,sizeof(MSGDatabaseEntity)) == sizeof(MSGDatabaseEntity)){
            os_file_seek(dbfno, entity.length, SEEK_CUR);
        }
    }
    
    os_file_lock(idxfno, LOCK_UN);
    os_file_close(idxfno);
    
    idxfno = os_file_open(path,O_WRONLY | O_APPEND,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    if(idxfno == -1){
        os_file_close(dbfno);
        return NULL;
    }
    
    os_file_lock(idxfno, LOCK_EX);
    
    while(os_file_read(dbfno, &entity, sizeof(MSGDatabaseEntity)) == sizeof(MSGDatabaseEntity)){
        
        index.location = (huint32)( lseek(dbfno, 0, SEEK_CUR) - sizeof(MSGDatabaseEntity) );
        index.timestamp = entity.timestamp;
        
        os_file_write(idxfno, & index, sizeof(MSGDatabaseIndex));
        
        os_file_seek(dbfno, entity.length, SEEK_CUR);
        
        len ++;
    }
    
    os_file_lock(idxfno, LOCK_UN);
    os_file_close(idxfno);
    
    idxfno = os_file_open(path,O_RDONLY,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    if(idxfno == -1){
        os_file_close(dbfno);
        return NULL;
    }
    
    os_file_lock(idxfno, LOCK_SH);
    
    cursor = (MSGDatabaseCursorImpl *) malloc(sizeof(MSGDatabaseCursorImpl));
    
    memset(cursor, 0, sizeof(MSGDatabaseCursorImpl));
    
    cursor->dbfno = dbfno;
    cursor->idxfno = idxfno;
    cursor->length = len;
    cursor->base.size = MSG_DATABASE_CURSOR_INDEX_SIZE;
    cursor->base.indexes = (MSGDatabaseIndex *) malloc(sizeof(MSGDatabaseIndex) * cursor->base.size);
    cursor->base.timestamp = timestamp;
    
    if( timestamp == 0.0){
        
        os_file_seek(idxfno, 0, SEEK_SET);
        len = (huint32)read(idxfno, cursor->base.indexes, cursor->base.size * sizeof(MSGDatabaseIndex));
        cursor->base.length = len / sizeof(MSGDatabaseIndex);
        
    }
    else{
        
        if(cursor->base.size < cursor->length){
            cursor->base.location = cursor->length - cursor->base.size;
        }
        
        while(1){

            os_file_seek(idxfno, cursor->base.location * sizeof(MSGDatabaseIndex), SEEK_SET);
            
            len = (huint32) os_file_read(idxfno, cursor->base.indexes, cursor->base.size * sizeof(MSGDatabaseIndex));
            
            cursor->base.length = len / sizeof(MSGDatabaseIndex);
        
            if(cursor->base.length ==0){
                os_file_lock(idxfno, LOCK_UN);
                os_file_close(idxfno);
                os_file_close(dbfno);
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
                
                cursor->base.index = cursor->base.length;
                cursor->base.length = 0;
                break;

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
    
    SRVServerLog("\nMSGDatabaseDefaultCursorOpen OK\n");
    
    return (MSGDatabaseCursor *)cursor;
}

static MSGDatabaseEntity * MSGDatabaseDefaultCursorNext (MSGDatabase * database,MSGDatabaseCursor * cursor,MSGBuffer * dbuf){
    
    MSGDatabaseCursorImpl * cur = (MSGDatabaseCursorImpl *) cursor;
    MSGDatabaseEntity entity, * pEntity;
    huint32 len;
    
    if(cursor->length ==0){
        
        if(cursor->index == 0){
            return NULL;
        }
        
        os_file_seek(cur->idxfno, cur->base.location + cursor->index * sizeof(MSGDatabaseIndex), SEEK_SET);
        
        len = (huint32) os_file_read(cur->idxfno, cur->base.indexes, cur->base.size * sizeof(MSGDatabaseIndex));
        
        if(len < sizeof(MSGDatabaseIndex)){
            return NULL;
        }
        
        cursor->length = len / sizeof(MSGDatabaseIndex);
        cursor->index = 0;
        
        cur->base.location += cursor->length;
    }
    
    os_file_seek(cur->dbfno, cursor->indexes[cursor->index].location, SEEK_SET);
    
    if(os_file_read(cur->dbfno, &entity, sizeof(MSGDatabaseEntity)) != sizeof(MSGDatabaseEntity)){
        return NULL;
    }
    
    MSGBufferExpandSize(dbuf, sizeof(MSGDatabaseEntity) + entity.length);
    
    memcpy(dbuf->data, &entity, sizeof(MSGDatabaseEntity));
    
    if(os_file_read(cur->dbfno,dbuf->data + sizeof(MSGDatabaseEntity), entity.length) != entity.length){
        return NULL;
    }
    
    cursor->index ++;
    cursor->length --;
    
    pEntity = (MSGDatabaseEntity *) dbuf->data;
    
    if(MSGDatabaseEntityIsVerify(pEntity)){
        return pEntity;
    }
    
    return NULL;
}

static void MSGDatabaseDefaultCursorClose (MSGDatabase * database,MSGDatabaseCursor * cursor){
   
    MSGDatabaseCursorImpl * cur = (MSGDatabaseCursorImpl *) cursor;
    
    if(cursor->indexes){
        free(cursor->indexes);
    }
    
    os_file_lock(cur->idxfno, LOCK_UN);
    
    os_file_close(cur->idxfno);
    os_file_close(cur->dbfno);
    
    free(cursor);
}

static hbool MSGDatabaseDefaultOpenResource (MSGDatabase * database,MSGAuth * auth,MSGDatabaseResource * res,hcchar * uri,hcchar * contentType){
    
    hchar path[PATH_MAX] , * dir = getenv(MSG_DEFAULT_PATH_ENV);;
    hchar uuid[128];
    struct timeval tv = {0,0};
    struct stat s;
    int fno = -1;
    
    SRVServerLog("\nMSGDatabaseDefaultOpenResource\n");
    
    if(uri){
        
        sscanf(uri, "/r/%s",uuid);
        
        snprintf(path, sizeof(path),"%s/%s/res/%s",dir,auth->user,uuid);
        
        fno = os_file_open(path, O_RDONLY,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        
        if(fno == -1){
            return hbool_false;
        }
        
        if(read(fno, res->type, sizeof(res->type)) != sizeof(res->type)){
            close(fno);
            return hbool_false;
        }
        
    }
    else{
        
        snprintf(path, sizeof(path),"%s/%s/res",dir,auth->to);
        
        if(stat(path, &s) == -1){
            mkdir(path,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        }
        
        while(1){
            
            if(tv.tv_sec == 0){
                gettimeofday(&tv, NULL);
            }
            else{
                tv.tv_usec ++;
            }
            
            sprintf(uuid,"%lx%x.r",tv.tv_sec,tv.tv_usec);
            
            snprintf(path, sizeof(path),"%s/%s/res/%s",dir,auth->to,uuid);
            
            if(stat(path, &s) == -1){
                break;
            }
        }
        
        fno = os_file_open(path, O_WRONLY | O_CREAT | O_TRUNC,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        
        if(fno == -1){
            return hbool_false;
        }
        
        strncpy(res->type, contentType, sizeof(res->type));
        
        if(os_file_write(fno, res->type, sizeof(res->type)) != sizeof(res->type)){
            os_file_close(fno);
            unlink(path);
            return hbool_false;
        }
        
    }
    
    snprintf(res->uri, sizeof(res->uri),"/r/%s",uuid);
    res->fno = fno;
    
    SRVServerLog("\nMSGDatabaseDefaultOpenResource OK\n");
    
    return hbool_true;
}

static void MSGDatabaseDefaultCloseResource (MSGDatabase * database,MSGAuth * auth,MSGDatabaseResource * res){
    
    if(res->fno >0){
        os_file_close(res->fno);
        res->fno = -1;
    }

}

static void MSGDatabaseDefaultRemoveResource (MSGDatabase * database,MSGAuth * auth,hcchar * uri){
    
    hchar path[PATH_MAX] , * dir = getenv(MSG_DEFAULT_PATH_ENV);;
    hchar uuid[128];
    
    if(uri){
        
        sscanf(uri, "/r/%s",uuid);
        
        snprintf(path, sizeof(path),"%s/%s/res/%s.r",dir,auth->user,uuid);
        
        unlink(path);
        
    }
}

huint32 MSGDatabaseEntityVerify(MSGDatabaseEntity * entity){
    
    huint32 v = entity->verify;
    huint32 verify = 0;
    hubyte * p = (hubyte *) entity;
    huint32 len = sizeof(MSGDatabaseEntity);
    
    entity->verify = 0;
    
    while(len >0){
        verify += * p;
        p ++;
        len --;
    }
    
    entity->verify = v;
    
    return verify;
}

bool MSGDatabaseEntityIsVerify(MSGDatabaseEntity * entity){
    return MSGDatabaseEntityVerify(entity) == entity->verify;
}


MSGDatabaseClass MSGDatabaseDefaultClass = {
    MSGDatabaseDefaultOpen,
    MSGDatabaseDefaultClose,
    MSGDatabaseDefaultWrite,
    MSGDatabaseDefaultCursorOpen,
    MSGDatabaseDefaultCursorNext,
    MSGDatabaseDefaultCursorClose,
    MSGDatabaseDefaultOpenResource,
    MSGDatabaseDefaultCloseResource,
    MSGDatabaseDefaultRemoveResource
};

