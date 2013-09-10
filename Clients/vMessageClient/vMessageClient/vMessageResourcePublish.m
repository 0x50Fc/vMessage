//
//  vMessageResourcePublish.m
//  vMessageClient
//
//  Created by Zhang Hailong on 13-6-29.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import "vMessageResourcePublish.h"

#define SBUF_SIZE   4096

@interface vMessageResourcePublish(){

    struct {
        uint8_t data[SBUF_SIZE];
        int index;
        int length;
    } sbuf;
    
    int _fno;
    
}

@end

@implementation vMessageResourcePublish

@synthesize filePath = _filePath;
@synthesize fileEOF = _fileEOF;
@synthesize contentType = _contentType;
@synthesize urlencoded = _urlencoded;

-(void) dealloc{
    if(_fno >0){
        close(_fno);
    }
    [_filePath release];
    [_contentType release];
    [_urlencoded release];
    [super dealloc];
}

-(id) initWithSession:(NSString *)session filePath:(NSString *) filePath{
    
    if((self = [super initWithSession:session])){
        
        _fileEOF = YES;
        
        _filePath = [filePath retain];
        
        _fno = open([filePath UTF8String], O_RDONLY);
        
        if(_fno == -1){
            [self autorelease];
            return nil;
        }
    }
    
    return self;
}

-(BOOL) willRequest:(CFHTTPMessageRef)request client:(vMessageClient *)client{
    
    CFHTTPMessageSetHeaderFieldValue(request, (CFStringRef) @"Content-Type"
                                     , (CFStringRef) _contentType);
    
    CFHTTPMessageSetHeaderFieldValue(request, (CFStringRef) @"Transfer-Encoding"
                                     , (CFStringRef) @"chunked");
    
    if(_urlencoded){
        CFHTTPMessageSetHeaderFieldValue(request, (CFStringRef) @"URL-Encoded", (CFStringRef) _urlencoded);
    }
    
    return YES;
}

-(BOOL) didHasSpaceStream:(NSOutputStream *) stream client:(vMessageClient *)client{
    
    int len;
    char slen[8];
    
    while ([stream hasSpaceAvailable]) {
        
        if(sbuf.length == 0){
            
            sbuf.index = 0;
            
            len = read(_fno, sbuf.data + 6, sizeof(sbuf.data) - 8);
            
            if(len < 0){
                
                dispatch_async(dispatch_get_main_queue(), ^{
                    [self didFailError:[NSError errorWithDomain:NSStringFromClass([self class]) code:0 userInfo:[NSDictionary dictionaryWithObject:@"read file error" forKey:NSLocalizedDescriptionKey]] client:client];
                });
                
                return NO;
            }
            else if(len ==0){
                
                if(_fileEOF){
                    
                    len = snprintf(slen, sizeof(slen),"0\r\n\r\n");
                    
                    if( len != [stream write: (uint8_t *) slen maxLength:len]){
                        
                        NSError * error = [stream streamError];
                        
                        dispatch_async(dispatch_get_main_queue(), ^{
                            [self didFailError:error client:client];
                        });

                        return NO;
                    }
                    
                    return NO;
                }
                
                return YES;
            }
            else {
                sbuf.length = len + 8;
                snprintf(slen, sizeof(slen),"%04x\r\n",len);
                memcpy(sbuf.data, slen, 6);
                strcpy(slen, "\r\n");
                memcpy(sbuf.data + 6 + len, slen, 2);
            }
        }
        else{
            
            len = [stream write:sbuf.data + sbuf.index maxLength:sbuf.length];
            
            if(len >0){
                sbuf.index += len;
                sbuf.length -= len;
            }
            else{
                
                NSError * error = [stream streamError];
                
                dispatch_async(dispatch_get_main_queue(), ^{
                    [self didFailError:error client:client];
                });
                
                return NO;
            }
        }

    }
    
    return YES;
}

@end
