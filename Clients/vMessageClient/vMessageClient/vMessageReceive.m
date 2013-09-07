//
//  vMessageReceive.m
//  vMessageClient
//
//  Created by Zhang Hailong on 13-8-31.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import "vMessageReceive.h"

#import "vMessageClient.h"

#define INPUT_DATA_SIZE  20480

@interface vMessageReceive(){
    struct {
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
        BOOL chunked;
        const char * chunkedLength;
        NSUInteger contentLength;
    } _inputState;
    uint8_t _inputData[INPUT_DATA_SIZE];
    CFHTTPMessageRef _response;
    
    BOOL _executing;
    BOOL _finished;
    
    FILE * _resourceFile;
}

@property(retain) NSInputStream * inputStream;
@property(retain) NSOutputStream * outputStream;
@property(retain) NSData * outputData;
@property(retain) vMessage * message;
@property(retain) NSRunLoop * runloop;


@end

@implementation vMessageReceive


@synthesize runloop = _runloop;
@synthesize outputData = _outputData;
@synthesize minIdle = _minIdle;
@synthesize idle = _idle;
@synthesize maxIdle = _maxIdle;
@synthesize addIdle = _addIdle;

@synthesize inputStream = _inputStream;
@synthesize outputStream = _outputStream;
@synthesize timestamp = _timestamp;
@synthesize message = _message;

@synthesize delegate = _delegate;

-(id) init{

    if((self = [super init])){
        
        _minIdle = 0.03;
        _idle = 0.03;
        _maxIdle = 6.0;
        _addIdle = 0.03;
    }
    
    return self;
}

-(void) dealloc{
    if(_resourceFile){
        fclose(_resourceFile);
    }
    if(_response){
        CFRelease(_response);
    }
    [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
    [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
    [_runloop release];
    [_inputStream close];
    [_inputStream setDelegate:nil];
    [_inputStream release];
    [_outputStream close];
    [_outputStream setDelegate:nil];
    [_outputStream release];
    [_outputData release];
    [_message release];
    [super dealloc];
}

-(void) cancel{
    [super cancel];
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    _finished = YES;
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

-(void) main{
    
    _executing = YES;
    
    @autoreleasepool {
        
        self.runloop = [NSRunLoop currentRunLoop];
        
        [self sendRequest:[NSOperationQueue currentQueue]];
        
        while(![self isCancelled] && ! _finished){
            @autoreleasepool {
                [_runloop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.3]];
            }
        }
        
    }
    
    _executing = NO;
}

-(void) sendRequest:(vMessageClient *) client {
    
    NSURL * url = [client url];
    
    NSString * host = [url host];
    
    NSUInteger port = [[url port] unsignedIntValue];

    if(port == 0){
        port = 80;
    }
    
    if(_response){
        CFRelease(_response);
        _response = nil;
    }
    
    memset(&_inputState, 0,sizeof(_inputState));
    memset(&_outputState, 0, sizeof(_outputState));
    
    
    self.outputData = nil;
    self.message = nil;
    
    [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
    [_outputStream setDelegate:nil];
    [_outputStream close];
    
    self.outputStream = nil;
    
    [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
    [_inputStream setDelegate:nil];
    [_inputStream close];
    
    self.inputStream = nil;
    
    CFStreamCreatePairWithSocketToHost(nil, (CFStringRef) host, port
                                       , (CFReadStreamRef *)&_inputStream
                                       , (CFWriteStreamRef *)&_outputStream);
    
    [_inputStream setProperty:NSStreamNetworkServiceTypeVoIP forKey:NSStreamNetworkServiceType];
    [_inputStream setDelegate:self];
    [_outputStream setProperty:NSStreamNetworkServiceTypeVoIP forKey:NSStreamNetworkServiceType];
    [_outputStream setDelegate:self];
    
    [_inputStream scheduleInRunLoop:_runloop forMode:NSRunLoopCommonModes];
    [_outputStream scheduleInRunLoop:_runloop forMode:NSRunLoopCommonModes];
    
    [_inputStream open];
    [_outputStream open];
    
}

- (void)stream:(NSStream *)aStream handleEvent:(NSStreamEvent)eventCode{
    
    vMessageClient * client = [NSOperationQueue currentQueue];
    
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
                                    _inputState.statusCode = 0;
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
                                    
                                    * pByte = 0;
                                    
                                    _inputState.state = 4;
                                    _inputState.key = 0;
                                    
                                    CFStringRef version = CFStringCreateWithCString(nil, _inputState.version, kCFStringEncodingUTF8);
                                    
                                    if(_response){
                                        CFRelease(_response);
                                        _response = nil;
                                    }
                                    
                                    _response = CFHTTPMessageCreateResponse(nil, atoi(_inputState.statusCode), nil, version);
                                    
                                    CFRelease(version);
                                    
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
                                if(CFHTTPMessageGetResponseStatusCode(_response) != 200){
                                    CFStringRef status = CFHTTPMessageCopyResponseStatusLine(_response);
                                    CFDictionaryRef fields = CFHTTPMessageCopyAllHeaderFields(_response);
                                    
                                    NSMutableDictionary * data = [NSMutableDictionary dictionaryWithDictionary:(NSDictionary *) fields];
                                    
                                    [data setValue:(id) status forKey:NSLocalizedDescriptionKey];
                                    
                                    CFRelease(status);
                                    CFRelease(fields);
                                    
                                    [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                    [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                    [_inputStream setDelegate:nil];
                                    [_outputStream setDelegate:nil];
                                    [_inputStream close];
                                    [_outputStream close];
                                    
                                    vMessageClient * client = [NSOperationQueue currentQueue];
                                    
                                    dispatch_async(dispatch_get_main_queue(), ^{
                                        
                                        if(![self isCancelled] ){
                                            [self didFailError:[NSError errorWithDomain:@"vMessageReceive" code:CFHTTPMessageGetResponseStatusCode(_response) userInfo:data] client:client];
                                        }
                                        
                                    });
                                    
                                    return;
                                }
                                
                                CFStringRef encoding = CFHTTPMessageCopyHeaderFieldValue(_response, (CFStringRef) @"Transfer-Encoding");
                                
                                _inputState.chunked = [(id)encoding isEqualToString:@"chunked"];
                                
                                if(encoding){
                                    CFRelease(encoding);
                                }
                                
                                if(!_inputState.chunked){
                                    
                                    [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                    [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                    [_inputStream setDelegate:nil];
                                    [_outputStream setDelegate:nil];
                                    [_inputStream close];
                                    [_outputStream close];
                                    
                                    vMessageClient * client = [NSOperationQueue currentQueue];
                                    
                                    dispatch_async(dispatch_get_main_queue(), ^{
                                        
                                        if(![self isCancelled]){
                                            [self didFailError:[NSError errorWithDomain:@"vMessageReceive" code:-11 userInfo:[NSDictionary dictionaryWithObject:@"only support chunked" forKey:NSLocalizedDescriptionKey]] client:client];
                                        }
                                        
                                    });
                                    
                                    return;
                                }
                                else{
                                    
                                    CFStringRef timestamp = CFHTTPMessageCopyHeaderFieldValue(_response, (CFStringRef) @"Timestamp");
                                    
                                    if(timestamp){
                                        
                                        _timestamp = atof([(NSString *) timestamp UTF8String]);
                                        CFRelease(timestamp);
                                    }
                                    
                                    _inputState.state = 7;
                                    _inputState.key = 0;
                                    _inputState.chunkedLength = 0;
                                }
                                continue;
                            }
                                break;
                            case 7:
                            {
                                // chunked
                                if(* pByte == '\r'){
                                    *pByte = 0;
                                }
                                else if( *pByte == '\n'){
                                    *pByte = 0;
                                    
                                    NSUInteger length = 0;
                                    
                                    if(_inputState.chunkedLength){
                                        sscanf(_inputState.chunkedLength,"%x", &length);
                                    }
                                    
                                    if(length ==0){
                                        
                                        [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                        [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                        [_inputStream setDelegate:nil];
                                        [_outputStream setDelegate:nil];
                                        [_inputStream close];
                                        [_outputStream close];
                                        
                                        if(![self isCancelled]){
                                            [self performSelector:@selector(sendRequest:) withObject:[NSOperationQueue currentQueue] afterDelay:_idle];
                                            _idle += _addIdle;
                                            if(_idle > _maxIdle){
                                                _idle = _maxIdle;
                                            }
                                        }
                                        
                                        return;
                                    }
                                    else{
                                        _inputState.state = 8;
                                    }
                                }
                                else if(_inputState.chunkedLength == 0){
                                    _inputState.chunkedLength = (char *) pByte;
                                }
                            }
                                break;
                            case 8:
                            {
                                // chunked
                                // key
                                if(* pByte == ':'){
                                    * pByte = 0;
                                    _inputState.state = 9;
                                    _inputState.value = 0;
                                }
                                else if(*pByte == '\r'){
                                    
                                }
                                else if(*pByte == '\n'){
                                    _inputState.state = 10;
                                    _inputState.key =0;
                                    _inputState.value = 0;
                                }
                                else if(_inputState.key == 0){
                                    _inputState.key = (char *) pByte;
                                }
                            }
                                break;
                            case 9:
                            {
                                // chunked value
                                if(* pByte == '\r'){
                                    * pByte = 0;
                                }
                                else if(* pByte == '\n'){
                                    * pByte = 0;
                                    
                                    if(_inputState.key && _inputState.value){
                                        
                                        if(strcmp(_inputState.key, "Content-Type") == 0){
                                            if(_message == nil){
                                                _message = [[vMessage alloc] init];
                                            }
                                            [_message setContentType:[NSString stringWithCString:_inputState.value encoding:NSUTF8StringEncoding]];
                                        }
                                        else if(strcmp(_inputState.key, "Content-Length") == 0){
                                            if(_message == nil){
                                                _message = [[vMessage alloc] init];
                                            }
                                            [_message setContentLength:atoi(_inputState.value)];
                                            _inputState.contentLength = _message.contentLength;
                                        }
                                        else if(strcmp(_inputState.key, "Timestamp") == 0){
                                            if(_message == nil){
                                                _message = [[vMessage alloc] init];
                                            }
                                            [_message setTimestamp:atof(_inputState.value)];
                                        }
                                        else if(strcmp(_inputState.key, "From") == 0){
                                            if(_message == nil){
                                                _message = [[vMessage alloc] init];
                                            }
                                            [_message setFrom:[NSString stringWithCString:_inputState.value encoding:NSUTF8StringEncoding]];
                                        }
                                        else if(strcmp(_inputState.key, "Resource-URI") == 0 ){
                                            if(_message == nil){
                                                _message = [[vMessage alloc] init];
                                            }
                                            if(_inputState.value && * _inputState.value != '\0'){
                                                [_message setResourceURI:[NSString stringWithCString:_inputState.value encoding:NSUTF8StringEncoding]];
                                            }
                                        }
                                    }
                                    
                                    _inputState.state = 8;
                                    _inputState.key = 0;
                                    _inputState.value = 0;
                                }
                                else if(_inputState.value == 0){
                                    if(* pByte == ' '){
                                        
                                    }
                                    else{
                                        _inputState.value = (char *) pByte;
                                    }
                                }
                            }
                                break;
                            case 10:
                            {
                                // chunked body
                                if(_inputState.contentLength){
                                    
                                    if(_message.body == nil){
                                        _message.body = [NSMutableData dataWithCapacity:_inputState.contentLength];
                                    }
                                    
                                    if(_inputState.contentLength <= _inputState.length){
                                        
                                        [(NSMutableData *) _message.body appendBytes:pByte length:_inputState.contentLength];
                                        
                                        _inputState.length -= _inputState.contentLength;
                                        _inputState.index += _inputState.contentLength;
                                        _inputState.contentLength = 0;
                                        
                                        continue;
                                    }
                                    else{
                                        [(NSMutableData *) _message.body appendBytes:pByte length:_inputState.length];
                                        _inputState.index += _inputState.length;
                                        _inputState.contentLength -= _inputState.length;
                                        _inputState.length -= _inputState.length;
                                        
                                        continue;
                                    }
                                    
                                    _inputState.contentLength --;
                                }
                                else if(*pByte == '\r'){
                                    
                                }
                                else if(*pByte == '\n'){
                                    _inputState.key = 0;
                                    _inputState.value = 0;
                                    _inputState.state = 11;
                                    
                                    if(_resourceFile){
                                        fclose(_resourceFile);
                                        _resourceFile = NULL;
                                    }
                                    
                                    {
                                        vMessage * msg = self.message;
                                        
                                        if(msg.timestamp){
                                            self.timestamp = msg.timestamp;
                                        }
                                        
                                        vMessageClient * client = [NSOperationQueue currentQueue];
                                        
                                        dispatch_async(dispatch_get_main_queue(), ^{
                                            self.idle = self.minIdle;
                                            if(![self isCancelled]){
                                                [self didRecvMessage:msg client:client];
                                            }
                                        });
                                        
                                        self.message = nil;
                                    }
                                }
                            }
                                break;
                            case 11:
                            {
                                if( *pByte == '\r'){
                                    
                                }
                                else if( *pByte == '\n'){
                                    _inputState.state = 7;
                                    _inputState.chunkedLength = 0;
                                }
                                else{
                                    [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                    [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                                    [_inputStream setDelegate:nil];
                                    [_outputStream setDelegate:nil];
                                    [_inputStream close];
                                    [_outputStream close];
                                    
                                    if(![self isCancelled]){
                                        [self performSelector:@selector(sendRequest:) withObject:[NSOperationQueue currentQueue] afterDelay:_idle];
                                        _idle += _addIdle;
                                        if(_idle > _maxIdle){
                                            _idle = _maxIdle;
                                        }
                                    }
                                    
                                    return;
                                }
                            }
                                break;
                            default:
                                break;
                        }
                        
                        _inputState.index ++;
                        _inputState.length --;
                        pByte ++;
                    }
                }
            }
        }
            break;
        case NSStreamEventHasSpaceAvailable:
        {
            if(aStream == _outputStream){
                
                if(_outputStream && _outputState.length){
                    
                    NSUInteger length = [_outputStream write:(uint8_t *)[_outputData bytes] + _outputState.index maxLength:_outputState.length];
                    
                    _outputState.index += length;
                    _outputState.length -= length;
                    
                    if(_outputState.length ==0){
                        self.outputData = nil;
                        [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                        [_outputStream setDelegate:nil];
                        [_outputStream close];
                    }
                    
                }
                else{
                    
                    
                    NSURL * url = [client url];
                    NSString * user = [client user];
                    NSString * password = [client password];
                    
                    CFHTTPMessageRef request = CFHTTPMessageCreateRequest(nil, (CFStringRef)@"GET", (CFURLRef)[NSURL URLWithString:[NSString stringWithFormat:@"%f",_timestamp] relativeToURL:url], (CFStringRef)@"1.1");
                    
                    CFHTTPMessageAddAuthentication(request, nil, (CFStringRef) user, (CFStringRef) password, kCFHTTPAuthenticationSchemeBasic, NO);
                    
                    CFHTTPMessageSetHeaderFieldValue(request, (CFStringRef)@"Accept", (CFStringRef)@"text/html,resource");
                    
                    CFDataRef data = CFHTTPMessageCopySerializedMessage(request);
                    
                    NSUInteger length = [_outputStream write:CFDataGetBytePtr(data) maxLength:CFDataGetLength(data)];
                    
                    if(length < CFDataGetLength(data)){
                        self.outputData = (id)data;
                        _outputState.index = length;
                        _outputState.length = CFDataGetLength(data) - length;
                    }
                    else{
                        [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
                        [_outputStream setDelegate:nil];
                        [_outputStream close];
                    }
                    
                    CFRelease(data);
                    
                    CFRelease(request);
                    
                }
            }
        }
            break;
        case NSStreamEventErrorOccurred:
        {
            [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendRequest:) object:[NSOperationQueue currentQueue]];
            
            [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
            [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
            [_inputStream setDelegate:nil];
            [_outputStream setDelegate:nil];
            [_inputStream close];
            [_outputStream close];
            
            NSError * error = aStream.streamError;
            
            vMessageClient * client = [NSOperationQueue currentQueue];
            
            dispatch_async(dispatch_get_main_queue(), ^{
                if(![self isCancelled]){
                    [self didFailError:error client:client];
                }
            });
            
        }
            break;
        case NSStreamEventEndEncountered:
        {
            if(_inputStream == aStream){
                
            }
            else if(_outputStream == aStream){
                
            }
        }
            break;
        default:
            break;
    }
}


-(void) didRecvMessage:(vMessage *) message client:(vMessageClient *) client{
    if([_delegate respondsToSelector:@selector(vMessageReceive:didRecvMessage:client:)]){
        [_delegate vMessageReceive:self didRecvMessage:message client:client];
    }
}

-(void) didFailError:(NSError *) error client:(vMessageClient *) client{
    
    if([_delegate respondsToSelector:@selector(vMessageReceive:didFailError:client:)]){
        [_delegate vMessageReceive:self didFailError:error client:client];
    }
    
    _finished = YES;
}

@end

