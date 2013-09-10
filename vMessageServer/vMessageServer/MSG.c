//
//  MSG.c
//  vMessageServer
//
//  Created by zhang hailong on 13-6-21.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#include "hconfig.h"
#include "MSG.h"

#define DEF_HEADER_COUNT    16

MSGString MSGStringZero = {0,0};


huint32 MSGBufferExpandSize(MSGBuffer * buf,huint32 expandSize){
    if(buf->data == NULL){
        buf->size = expandSize;
        buf->data = malloc(buf->size);
    }
    else if(buf->size < expandSize){
        buf->size = expandSize;
        buf->data = realloc(buf->data, buf->size);
    }
    return buf->size;
}

void MSGHTTPRequestReset(MSGHttpRequest * request){
    request->method = MSGStringZero;
    request->path = MSGStringZero;
    request->headers.length = 0;
    request->state.state = MSGHttpRequestStateNone;
    request->state.key = MSGStringZero;
    request->state.value = MSGStringZero;
}


huint32 MSGHTTPRequestRead(MSGHttpRequest * request,MSGBuffer * buf,huint32 offset,huint32 length){
    
    huint32 c = length;
    hchar * p = buf->data + offset;
    huint32 off = offset;
    MSGHttpHeader * h;
    while(p && c >0){
        
        switch (request->state.state) {
            case MSGHttpRequestStateNone:
            {
                request->method.location = off;
                request->method.length = 1;
                request->state.state = MSGHttpRequestStateMethod;
            }
                break;
            case MSGHttpRequestStateMethod:
            {
                if(*p == ' '){
                    request->state.state = MSGHttpRequestStatePath;
                    request->path.location = off + 1;
                }
                else{
                    request->method.length ++;
                }
            }
                break;
            case MSGHttpRequestStatePath:
            {
                if( *p == ' '){
                    request->state.state = MSGHttpRequestStateVersion;
                }
                else{
                    request->path.length ++;
                }
            }
                break;
            case MSGHttpRequestStateVersion:
            {
                if(*p == '\r'){
                    
                }
                else if( *p == '\n'){
                    request->state.state = MSGHttpRequestStateHeaderKey;
                    request->state.key.length = 0;
                }
                else{
                    if(request->version.length ==0){
                        request->version.location = off;
                    }
                    request->version.length ++;
                }
            }
                break;
            case MSGHttpRequestStateHeaderKey:
            {
                if(* p == '\r'){
                    
                }
                else if(*p == '\n'){
                    request->state.state = MSGHttpRequestStateOK;
                }
                else if(*p == ':'){
                    request->state.state = MSGHttpRequestStateHeaderValue;
                    request->state.value.length = 0;
                }
                else {
                    if(request->state.key.length ==0){
                        request->state.key.location = off;
                    }
                    request->state.key.length ++;
                }
            }
                break;
            case MSGHttpRequestStateHeaderValue:
            {
                if(* p == '\r'){
                    
                }
                else if(*p == '\n'){
                    
                    if(request->headers.data == NULL){
                        request->headers.size = DEF_HEADER_COUNT;
                        request->headers.data = malloc(request->headers.size * sizeof(MSGHttpHeader));
                    }
                    else if(request->headers.length + 1 > request->headers.size){
                        request->headers.size += DEF_HEADER_COUNT;
                        request->headers.data = realloc(request->headers.data,request->headers.size * sizeof(MSGHttpHeader));
                    }
                    
                    h = request->headers.data + request->headers.length;
                    h->key = request->state.key;
                    h->value = request->state.value;
                    
                    request->headers.length ++;
                    
                    request->state.state = MSGHttpRequestStateHeaderKey;
                    request->state.key.length = 0;
                    request->state.value.length = 0;
                }
                else {
                    if(request->state.value.length ==0){
                        if( *p == ' '){
                            
                        }
                        else {
                            request->state.value.location = off;
                            request->state.value.length = 1;
                        }
                    }
                    else {
                        request->state.value.length ++;
                    }
                }
            }
                break;
            default:
                return length - c;
                break;
        }
        
        c -- ;
        p ++;
        off ++;
    }
    
    return length - c;
}

hbool MSGStringEqual(MSGBuffer * buf,MSGString string,hcchar * cString){
    hchar * p1 = buf->data + string.location;
    hchar * p2 = (hchar *) cString;
    huint32 len = string.length;
    
    while(p2 && p1 && len >0 && *p1 == * p2){
    
        len --;
        p1 ++;
        p2 ++;
    }
    
    return p2 && * p2 == 0 && len == 0;
}

hbool MSGStringHasPrefix(MSGBuffer * buf,MSGString string,hcchar * cString){
    hchar * p1 = buf->data + string.location;
    hchar * p2 = (hchar *) cString;
    huint32 len = string.length;
    
    while(p2 && p1 && len >0 && *p1 == * p2){
        
        len --;
        p1 ++;
        p2 ++;
    }
    
    return p2 && * p2 == 0;
}

hint32 MSGHttpRequestHeaderIntValue(MSGHttpRequest * request,MSGBuffer * buf,hcchar * key,hint32 defaultValue){
    huint32 c = request->headers.length;
    MSGHttpHeader * h = request->headers.data;
    hchar b[128] = {0};
    
    while(c >0){
        
        if(MSGStringEqual(buf, h->key, key)){
            strncpy(b, buf->data + h->value.location, h->value.length);
            return atoi(b);
        }
        
        c --;
        h ++;
    }
    return defaultValue;
}

MSGHttpHeader * MSGHttpRequestGetHeader(MSGHttpRequest * request,MSGBuffer * buf,hcchar * key){
    huint32 c = request->headers.length;
    MSGHttpHeader * h = request->headers.data;
 
    while(c >0){
        

        if(MSGStringEqual(buf, h->key, key)){
            return h;
        }
        
        c --;
        h ++;
    }
    
    return NULL;
}

hbool MSGBlockHasUser(hcchar * block,hcchar * user){
    if(block && user){
        hchar *p = (hchar *)block;
        hint32 l = (hint32)strlen(user),i=0;
        while(*p!='\0'){
            if(user[i] == '\0'){
                break;
            }
            if(*p == user[i]){
                i++;
            }
            else{
                p = p -i;
                i = 0;
            }
            p++;
        }
        return i==l && (*p =='\0' || *p ==' ' || *p ==',' || *p == ';') ;
    }
    return 0;
}
