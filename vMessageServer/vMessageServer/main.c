//
//  main.c
//  vMessageServer
//
//  Created by zhang hailong on 13-6-20.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#include "hconfig.h"
#include "hserver.h"
#include "MSGServerProcess.h"

#define MAX_PROCESS_COUNT       32
#define DEFAULT_PROCESS_COUNT   1

static void SRVServerLogMSG(struct _SRVServer * srv,const char * format,va_list va){
    
    char sbuf[PATH_MAX];
    int fno;
    int len;
    
    snprintf(sbuf, sizeof(sbuf),"/var/log/%s.log",getprogname());
    
    fno = open(sbuf, O_WRONLY | O_APPEND);
    
    if(fno == -1){
        fno = open(sbuf, O_WRONLY | O_CREAT);
        if(fno != -1){
            fchmod(fno, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            close(fno);
        }
        fno = open(sbuf, O_WRONLY | O_APPEND);
    }
    
    if(fno != -1){
        len = vsnprintf(sbuf, sizeof(sbuf), format, va);
        
        write(fno, sbuf, len);
        
        close(fno);
    }
}

int main(int argc, char ** argv)
{
    
    SRVProcess processs[MAX_PROCESS_COUNT] = {0};
    
    SRVServer srv = {
        {{argc,argv},{processs,MIN(MAX_PROCESS_COUNT,DEFAULT_PROCESS_COUNT)},{0},0},{0},SRVServerLogMSG
    };
    
    {
        int i;
        
        for(i=1;i<argc;i++){
            if(strcmp(argv[i], "-maxProcessCount") ==0 && i + 1 <argc){
                i ++;
                srv.config.process.length = atoi(argv[i]);
                if(srv.config.process.length ==0 ){
                    srv.config.process.length = DEFAULT_PROCESS_COUNT;
                }
                if(srv.config.process.length > MAX_PROCESS_COUNT){
                    srv.config.process.length = MAX_PROCESS_COUNT;
                }
            }
        }
        
        for(i=0;i<srv.config.process.length;i++){
            srv.config.process.data[i].clazz = & MSGServerProcessClass;
        }
    }
    
    return SRVServerRun(& srv);
}

