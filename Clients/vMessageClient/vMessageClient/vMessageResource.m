//
//  vMessageResource.m
//  vMessageClient
//
//  Created by Zhang Hailong on 13-6-29.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import "vMessageResource.h"

#define INPUT_DATA_SIZE  10240

#include <sys/stat.h>
#include <utime.h>

@interface vMessageResource(){
    CFHTTPMessageRef _request;
    CFHTTPMessageRef _response;
    BOOL _executing;
    BOOL _finished;
    
    struct {
        int state;
        NSUInteger index;
        NSUInteger length;
    } _outputState;
    
    struct {
        NSUInteger index;
        NSUInteger length;
        int state;
        const char * protocol;
        const char * version;
        const char * statusCode;
        const char * status;
        const char * key;
        const char * value;
        NSUInteger contentLength;
        NSUInteger bytes;
    } _inputState;

    uint8_t _inputData[INPUT_DATA_SIZE];
    
    FILE * _file;
}

@property(retain) NSInputStream * inputStream;
@property(retain) NSOutputStream * outputStream;
@property(retain) NSRunLoop * runloop;
@property(retain) NSData * outputData;

@end

@implementation vMessageResource

@synthesize filePath = _filePath;
@synthesize uri = _uri;
@synthesize inputStream = _inputStream;
@synthesize outputStream = _outputStream;
@synthesize runloop = _runloop;
@synthesize outputData = _outputData;
@synthesize delegate = _delegate;


-(void) dealloc{
    
    [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
    [_outputStream setDelegate:nil];
    [_outputStream close];
    
    [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
    [_inputStream setDelegate:nil];
    [_inputStream close];
    
    [_inputStream release];
    [_outputStream release];
    
    [_runloop release];
    [_outputData release];
    if(_request){
        CFRelease(_request);
    }
    if(_response){
        CFRelease(_response);
    }
    [_filePath release];
    [_uri release];
    [super dealloc];
}

-(id) initWithUri:(NSString *) uri filePath:(NSString *) filePath{
    
    if([uri length] ==0 || [filePath length] == 0){
        [self autorelease];
        return nil;
    }
    
    if((self = [super init])){
        _uri = [uri retain];
        _filePath = [filePath retain];
    }
    
    return self;
}

-(BOOL) isConcurrent{
    return YES;
}

-(BOOL) isReady{    
    return YES;
}

-(BOOL) isFinished{
    return _finished;
}

-(BOOL) isExecuting{
    return _executing;
}

-(void) didResponse:(CFHTTPMessageRef) response client:(vMessageClient *) client{
    if([_delegate respondsToSelector:@selector(vMessageResource:didResponse:client:)]){
        [_delegate vMessageResource:self didResponse:response client:client];
    }
}

-(void) didRecvBytes:(NSUInteger ) bytes contentLength:(NSUInteger) contentLength client:(vMessageClient *) client{
    if([_delegate respondsToSelector:@selector(vMessageResource:didRecvBytes:contentLength:client:)]){
        [_delegate vMessageResource:self didRecvBytes:bytes contentLength:contentLength client:client];
    }
}

-(void) didFailError:(NSError *) error client:(vMessageClient *) client{
    
    if(_file){
        fclose(_file);
        _file = NULL;
    }
    
    _finished = YES;
    if([_delegate respondsToSelector:@selector(vMessageResource:didFailError:client:)]){
        [_delegate vMessageResource:self didFailError:error client:client];
    }
}

-(void) didFinished:(vMessageClient *) client{
    
    if(_file){
        fclose(_file);
        _file = NULL;
    }
    
    CFStringRef c = CFHTTPMessageCopyHeaderFieldValue(_response, (CFStringRef) @"ETag");
    
    if(c){
        
        struct utimbuf ut = {
            time(NULL),0
        };
        
        sscanf([(id)c UTF8String], "%lx",&ut.modtime);
        
        utime([_filePath UTF8String], &ut);
        
        CFRelease(c);
    }
    
    _finished = YES;
    if([_delegate respondsToSelector:@selector(vMessageResourceDidFinished:)]){
        [_delegate vMessageResourceDidFinished:self client:client];
    }
}

-(void) main{
    
    _executing = YES;
    
    @autoreleasepool {
        
        self.runloop = [NSRunLoop currentRunLoop];
        
        vMessageClient * client = [NSOperationQueue currentQueue];
        
        if(_request == nil){
            
            _request = CFHTTPMessageCreateRequest(nil, (CFStringRef)@"GET", (CFURLRef)[NSURL URLWithString:_uri relativeToURL:client.url], (CFStringRef) @"1.1");
            
            CFHTTPMessageAddAuthentication(_request, NULL, (CFStringRef) client.user, (CFStringRef) client.password, kCFHTTPAuthenticationSchemeBasic, NO);
            
            struct stat s;
            char etag[128];
            
            if(stat([_filePath UTF8String], &s) != -1){
                
                snprintf(etag, sizeof(etag),"W/%lx",s.st_mtimespec.tv_sec);
                
                CFHTTPMessageSetHeaderFieldValue(_request, (CFStringRef) @"If-None-Match"
                                                 , (CFStringRef) [NSString stringWithCString:etag encoding:NSUTF8StringEncoding]);
                
                _file = fopen([_filePath UTF8String], "ab");
            }
            else{
                _file = fopen([_filePath UTF8String], "wb");
            }
            
        }
        
        NSURL * url = client.url;
        NSString * host = [url host];
        NSUInteger port = [[url port] unsignedIntValue];
        
        if(port == 0){
            port = 80;
        }
        
        CFStreamCreatePairWithSocketToHost(nil, (CFStringRef) host, port
                                           , (CFReadStreamRef *) & _inputStream, (CFWriteStreamRef *)& _outputStream);
        
        
        [_inputStream setDelegate:self];
        [_inputStream scheduleInRunLoop:_runloop forMode:NSRunLoopCommonModes];
        [_outputStream setDelegate:self];
        [_outputStream scheduleInRunLoop:_runloop forMode:NSRunLoopCommonModes];
        
        [_outputStream open];
        [_inputStream open];
        
        while(![self isCancelled] && ! _finished){
            @autoreleasepool {
                [_runloop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.3]];
            }
        }
        
    }
    
    _executing = NO;
}

-(void) cancel{
    [super cancel];
    
    _finished = YES;
    
    [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
    [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
    [_inputStream setDelegate:nil];
    [_outputStream setDelegate:nil];
    [_inputStream close];
    [_outputStream close];
}

-(void) stream:(NSStream *)aStream handleEvent:(NSStreamEvent)eventCode{
    
    switch (eventCode) {
        case NSStreamEventOpenCompleted:
        {
            
        }
            break;
        case NSStreamEventHasBytesAvailable:
        {
            if(_inputStream == aStream){
                
                while([_inputStream hasBytesAvailable]){
                    
                    if(_inputState.length == 0){
                        if(_inputState.index < sizeof(_inputData)){
                            NSInteger len = [_inputStream read:_inputData + _inputState.index
                                                     maxLength:sizeof(_inputData) - _inputState.index];
                            if(len <= 0){
                                return;
                            }
                            _inputState.length = len;
                        }
                        else{
                            NSInteger len = [_inputStream read:_inputData maxLength:sizeof(_inputData)];
                            if(len <= 0){
                                return;
                            }
                            _inputState.length = len;
                            _inputState.index = 0;
                        }
                    }
                    
                    uint8_t * pByte ;
                    
                    while(_inputState.length){
                        
                        pByte = _inputData + _inputState.index;
                        
                        switch (_inputState.state) {
                            case 0:
                            {
                                // HTTP
                                if(*pByte == '/'){
                                    *pByte = 0;
                                    _inputState.state =1;
                                    _inputState.version = 0;
                                }
                                else if(_inputState.protocol == 0){
                                    _inputState.protocol = (char *) pByte;
                                }
                            }
                                break;
                            case 1:
                            {
                                // HTTP/1.1
                                if(*pByte == ' '){
                                    *pByte = 0;
                                    _inputState.statusCode = (char *) pByte + 1;
                                    _inputState.state = 2;
                                }
                                else if(_inputState.version ==0){
                                    _inputState.version = (char *) pByte;
                                }
                            }
                                break;
                            case 2:
                            {
                                // HTTP/1.1 200
                                if(*pByte == ' '){
                                    *pByte = 0;
                                    _inputState.state = 3;
                                    _inputState.status = 0;
                                }
                                else if(_inputState.statusCode == 0){
                                    _inputState.statusCode = (char *) pByte;
                                }
                            }
                                break;
                            case 3 :
                            {
                                // HTTP/1.1 200 OK
                                if( * pByte == '\r'){
                                    * pByte = 0;
                                }
                                else if( * pByte == '\n'){
                                    _inputState.state = 4;
                                    _inputState.key = 0;
                                    
                                    CFStringRef version = CFStringCreateWithCString(nil, _inputState.version, kCFStringEncodingUTF8);
                                    
                                    if(_response){
                                        CFRelease(_response);
                                        _response = nil;
                                    }
                                    
                                    _response = CFHTTPMessageCreateResponse(nil, atoi(_inputState.statusCode), nil, version);
                                    
                                    CFRelease(version);
                                    
                                    int statusCode = CFHTTPMessageGetResponseStatusCode(_response);
                                    
                                    if(statusCode == 200){
                                    

                                        
                                    }
                                    else if(statusCode == 304){
                                        
                                        [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                        [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                        [_inputStream setDelegate:nil];
                                        [_outputStream setDelegate:nil];
                                        [_inputStream close];
                                        [_outputStream close];
                                        
                                        vMessageClient * client = [NSOperationQueue currentQueue];
                                        
                                        dispatch_async(dispatch_get_main_queue(), ^{
                                            if(! _finished){
                                                [self didFinished:client];
                                            }
                                        });
                                        
                                        return;
                                        
                                    }
                                    else{
                                        
                                        [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                        [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                        [_inputStream setDelegate:nil];
                                        [_outputStream setDelegate:nil];
                                        [_inputStream close];
                                        [_outputStream close];
                                        
                                        NSError * error = [NSError errorWithDomain:NSStringFromClass([self class])
                                                                              code:CFHTTPMessageGetResponseStatusCode(_response) userInfo:nil];
                                        
                                        vMessageClient * client = [NSOperationQueue currentQueue];
                                        
                                        dispatch_async(dispatch_get_main_queue(), ^{
                                            if(! _finished){
                                                [self didFailError:error client:client];
                                            }
                                        });

                                        return;
                                    }
                                    
                                }
                                else if(_inputState.status ==0){
                                    _inputState.status = (char *) pByte;
                                }
                                
                            }
                                break;
                            case 4:
                            {
                                // key
                                if(*pByte == ':'){
                                    *pByte = 0;
                                    
                                    _inputState.state = 5;
                                    _inputState.value = 0;
                                }
                                else if(*pByte == '\r'){
                                    
                                }
                                else if(*pByte == '\n'){
                                    
                                    _inputState.state = 6;
                                    _inputState.key = 0;
                                    _inputState.value = 0;
                                    
                                    vMessageClient * client = [NSOperationQueue currentQueue];
                                    
                                    dispatch_async(dispatch_get_main_queue(), ^{
                                        if(!_finished){
                                            [self didResponse:_response client:client];
                                        }
                                    });
                                    
                                    CFStringRef c = CFHTTPMessageCopyHeaderFieldValue(_response, (CFStringRef) @"Content-Length");
                                    
                                    _inputState.contentLength = 0;
                                    _inputState.bytes = 0;
                                    
                                    if(c){
                                        _inputState.contentLength = [(id)c intValue];
                                        
                                        CFRelease(c);
                                    }
                                    
                                    if(_inputState.contentLength == 0){
                                        
                                        [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                        [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                        [_inputStream setDelegate:nil];
                                        [_outputStream setDelegate:nil];
                                        [_inputStream close];
                                        [_outputStream close];
                                        
                                        vMessageClient * client = [NSOperationQueue currentQueue];
                                        
                                        dispatch_async(dispatch_get_main_queue(), ^{
                                            if(! _finished){
                                                [self didFinished:client];
                                            }
                                        });
                                        
                                        return;
                                        
                                    }
                                    
                                }
                                else if(_inputState.key == 0){
                                    _inputState.key = (char *) pByte;
                                }
                            }
                                break;
                            case 5:
                            {
                                // value
                                if(*pByte == '\r'){
                                    
                                    *pByte = 0;
                                }
                                else if(*pByte == '\n'){
                                    *pByte = 0;
                                    
                                    if(_inputState.key && _inputState.value){
                                        NSString * key = [NSString stringWithCString:_inputState.key encoding:NSUTF8StringEncoding];
                                        NSString * value = [NSString stringWithCString:_inputState.value encoding:NSUTF8StringEncoding];
                                        
                                        CFHTTPMessageSetHeaderFieldValue(_response, (CFStringRef) key, (CFStringRef) value);
                                    }
                                    
                                    _inputState.key = 0;
                                    _inputState.value = 0;
                                    _inputState.state = 4;
                                }
                                else if(_inputState.value == 0){
                                    if(*pByte == ' '){
                                        
                                    }
                                    else{
                                        _inputState.value = (char *) pByte;
                                    }
                                }
                            }
                                break;
                            case 6:
                            {
                                int len = MIN(_inputState.length, _inputState.contentLength);
                                
                                if(_file){
                                    len = fwrite(_inputData + _inputState.index, 1, len, _file);
                                }
                                
                                _inputState.contentLength -= len;
                                
                                if(_inputState.contentLength == 0){
                                    
                                    [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                    [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                    [_inputStream setDelegate:nil];
                                    [_outputStream setDelegate:nil];
                                    [_inputStream close];
                                    [_outputStream close];
                                    
                                    vMessageClient * client = [NSOperationQueue currentQueue];
                                    
                                    dispatch_async(dispatch_get_main_queue(), ^{
                                        if(! _finished){
                                            [self didFinished:client];
                                        }
                                    });
                                    
                                    return;
                                }
                                
                                _inputState.index += len;
                                _inputState.length -= len;
                                
                                continue;
                                
                            }
                            default:
                                break;
                        }
                        
                        _inputState.index ++;
                        _inputState.length --;
                    }
                }
            }
        }
            break;
        case NSStreamEventHasSpaceAvailable:
        {
            if(_outputStream == aStream){
                while ([_outputStream hasSpaceAvailable]) {
                    
                    if(_outputState.state == 0){
                        
                        CFDataRef data = CFHTTPMessageCopySerializedMessage(_request);
                        
                        NSInteger len = [_outputStream write:CFDataGetBytePtr(data) maxLength:CFDataGetLength(data)];
                    
                        if(len <=0){
                            [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                            [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                            [_inputStream setDelegate:nil];
                            [_outputStream setDelegate:nil];
                            [_inputStream close];
                            [_outputStream close];
                            
                            NSError * error = aStream.streamError;
                            
                            vMessageClient * client = [NSOperationQueue currentQueue];
                            
                            dispatch_async(dispatch_get_main_queue(), ^{
                                if(! _finished){
                                    [self didFailError:error client:client];
                                }
                            });
                            return;
                        }
                        
                        if(len < CFDataGetLength(data)){
                            self.outputData = (id) data;
                            _outputState.index = len;
                            _outputState.length = CFDataGetLength(data) - len;
                        }
                        else{
                            _outputState.state = 1;
                            break;
                        }
                        
                        CFRelease(data);
                    }
                    else {
                        break;
                    }
                    
                }
            }
        }
            break;
        case NSStreamEventEndEncountered:
        {
            
        }
            break;
        case NSStreamEventErrorOccurred:
        {
            [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
            [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
            [_inputStream setDelegate:nil];
            [_outputStream setDelegate:nil];
            [_inputStream close];
            [_outputStream close];
            
            NSError * error = aStream.streamError;
            
            vMessageClient * client = [NSOperationQueue currentQueue];
            
            dispatch_async(dispatch_get_main_queue(), ^{
                if(! _finished){
                    [self didFailError:error client:client];
                }
            });
        }
            break;
        default:
            break;
    }
    
}


@end
