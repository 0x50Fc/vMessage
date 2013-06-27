//
//  vMessageClient.m
//  vMessageClient
//
//  Created by zhang hailong on 13-6-26.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import "vMessageClient.h"

#define INPUT_DATA_SIZE  20480

@interface NSURL(Test)

-(NSUInteger) length;

@end

@implementation NSURL(Test)

-(NSUInteger) length{
    
    return 0;
}

@end

@class vMessageReceive;

@interface vMessageClient(){
    
}

@property(nonatomic,retain) vMessageReceive * recvice;

-(void) onDidMessage:(vMessage *) message;

-(void) onDidFailError:(NSError * ) error;

@end


@interface vMessageReceive : NSOperation<NSStreamDelegate>{
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
}

@property(copy) NSURL * url;
@property(copy) NSString * user;
@property(copy) NSString * password;
@property(retain) NSInputStream * inputStream;
@property(retain) NSOutputStream * outputStream;
@property(retain) NSData * outputData;
@property(retain) vMessage * message;
@property(retain) NSRunLoop * runloop;

@property(nonatomic,assign) NSTimeInterval timestamp;
@property(nonatomic,assign) NSTimeInterval minIdle;
@property(nonatomic,assign) NSTimeInterval idle;
@property(nonatomic,assign) NSTimeInterval maxIdle;
@property(nonatomic,assign) NSTimeInterval addIdle;

-(id) initWithURL:(NSURL *) url user:(NSString *) user password:(NSString *) password;

@end

@implementation vMessageReceive

@synthesize url = _url;
@synthesize user = _user;
@synthesize password = _password;

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

-(id) initWithURL:(NSURL *) url user:(NSString *) user password:(NSString *) password;{
    if((self = [super init])){
        self.url = url;
        self.user = user;
        self.password = password;
    }
    return self;
}

-(void) dealloc{
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
    [_url release];
    [_user release];
    [_password release];
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
        
        [self sendRequest];
        
        while(![self isCancelled] && ! _finished){
            @autoreleasepool {
                [_runloop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.3]];
            }
        }
        
    }
        
    _executing = NO;
}

-(void) sendRequest {

    NSString * host = [_url host];
    
    NSUInteger port = [[_url port] unsignedIntValue];
    
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
    
    
    [_inputStream setDelegate:self];
    [_outputStream setDelegate:self];
    
    [_inputStream scheduleInRunLoop:_runloop forMode:NSRunLoopCommonModes];
    [_outputStream scheduleInRunLoop:_runloop forMode:NSRunLoopCommonModes];
    
    [_inputStream open];
    [_outputStream open];
    
}


- (void)stream:(NSStream *)aStream handleEvent:(NSStreamEvent)eventCode{
    
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
                    
                    uint8_t * pByte = _inputData + _inputState.index;
                    
                    while(_inputState.length){
                        
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
                                    
                                    NSString * status = [NSString stringWithCString:_inputState.status encoding:NSUTF8StringEncoding];
                                    
                                    NSString * version = [NSString stringWithCString:_inputState.version encoding:NSUTF8StringEncoding];
                                    
                                    _response = CFHTTPMessageCreateResponse(nil, atoi(_inputState.statusCode), (CFStringRef) status, (CFStringRef) version);
                                    
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
                                    
                                    vMessageClient * client = (vMessageClient *)[NSOperationQueue currentQueue];
                                    
                                    if([client isKindOfClass:[vMessageClient class]]){
                                        dispatch_async(dispatch_get_main_queue(), ^{
                                            
                                            if(![self isCancelled] && client.started){
                                                [client onDidFailError:[NSError errorWithDomain:@"vMessageClient" code:CFHTTPMessageGetResponseStatusCode(_response) userInfo:data]];
                                            }
                                            
                                        });
                                    }
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
                                    
                                    vMessageClient * client = (vMessageClient *)[NSOperationQueue currentQueue];
                                    
                                    if([client isKindOfClass:[vMessageClient class]]){
                                        dispatch_async(dispatch_get_main_queue(), ^{
                                            
                                            if(![self isCancelled] &&client.started){
                                                [client onDidFailError:[NSError errorWithDomain:@"vMessageClient" code:-11 userInfo:[NSDictionary dictionaryWithObject:@"only support chunked" forKey:NSLocalizedDescriptionKey]]];
                                            }
                                            
                                        });
                                    }
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
                                        
                                        dispatch_async(dispatch_get_main_queue(), ^{
                                            
                                            if(![self isCancelled]){
                                                [self performSelector:@selector(sendRequest) withObject:nil afterDelay:_idle];
                                                _idle += _addIdle;
                                                if(_idle > _maxIdle){
                                                    _idle = _maxIdle;
                                                }
                                            }
                                            
                                        });
                                        
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
                                    [(NSMutableData *) _message.body appendBytes:pByte length:1];
                                    _inputState.contentLength --;
                                }
                                else if(*pByte == '\r'){
                                    
                                }
                                else if(*pByte == '\n'){
                                    _inputState.key = 0;
                                    _inputState.value = 0;
                                    _inputState.state = 11;
                                    
                                    {
                                        vMessage * msg = self.message;
        
                                        vMessageClient * client = (vMessageClient *)[NSOperationQueue currentQueue];
                                        
                                        if([client isKindOfClass:[vMessageClient class]]){
                                            
                                            if(msg.timestamp){
                                                self.timestamp = msg.timestamp;
                                            }
                                            
                                            dispatch_async(dispatch_get_main_queue(), ^{
                                                self.idle = self.minIdle;
                                                if(![self isCancelled] && client.started){
                                                    [client onDidMessage:msg];
                                                }
                                            });
                                        }
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
                                    
                                    dispatch_async(dispatch_get_main_queue(), ^{
                                        
                                        if(![self isCancelled]){
                                            [self performSelector:@selector(sendRequest) withObject:nil afterDelay:_idle];
                                            _idle += _addIdle;
                                            if(_idle > _maxIdle){
                                                _idle = _maxIdle;
                                            }
                                        }
                                        
                                    });
                                    
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
                    
                    CFHTTPMessageRef request = CFHTTPMessageCreateRequest(nil, (CFStringRef)@"GET", (CFURLRef)[NSURL URLWithString:[NSString stringWithFormat:@"%f",_timestamp] relativeToURL:_url], (CFStringRef)@"1.1");
                    
                    CFHTTPMessageAddAuthentication(request, nil, (CFStringRef) _user, (CFStringRef) _password, kCFHTTPAuthenticationSchemeBasic, NO);
                    
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
            [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendRequest) object:nil];
            
            [_inputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
            [_outputStream removeFromRunLoop:_runloop forMode:NSRunLoopCommonModes];
            [_inputStream setDelegate:nil];
            [_outputStream setDelegate:nil];
            [_inputStream close];
            [_outputStream close];
            
            NSError * error = aStream.streamError;
            
            vMessageClient * client = (vMessageClient *)[NSOperationQueue currentQueue];
            
            if([client isKindOfClass:[vMessageClient class]]){
                
                dispatch_async(dispatch_get_main_queue(), ^{
                    if(![self isCancelled] && client.started){
                        [client onDidFailError:error];
                    }
                });
            }
            
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


@end


@implementation vMessageClient

@synthesize url = _url;
@synthesize user = _user;
@synthesize password = _password;
@synthesize delegate = _delegate;

@synthesize minIdle = _minIdle;
@synthesize idle = _idle;
@synthesize maxIdle = _maxIdle;
@synthesize addIdle = _addIdle;
@synthesize timestamp = _timestamp;

-(void) dealloc{
    [_url release];
    [_user release];
    [_password release];
    [super dealloc];
}

-(id) initWithURL:(NSURL *) url user:(NSString *) user password:(NSString *) password{
    if((self = [super init])) {
        
        _url = [url retain];
        _user = [user retain];
        _password = [password retain];
        
        _minIdle = 0.03;
        _idle = 0.03;
        _maxIdle = 6.0;
        _addIdle = 0.03;
        
        [self setMaxConcurrentOperationCount:5];
    }
    return self;
}

-(BOOL) isStarted{
    return _recvice != nil;
}

-(void) start{
    
    if(![self isStarted]){
        
        [_recvice cancel];
        
        self.recvice = [[[vMessageReceive alloc] initWithURL:_url user:_user password:_password] autorelease];
        [_recvice setMaxIdle:_minIdle];
        [_recvice setIdle:_idle];
        [_recvice setMaxIdle:_maxIdle];
        [_recvice setAddIdle:_addIdle];
        [_recvice setTimestamp:_timestamp];
        
        [self addOperation:_recvice];
        
    }
}

-(void) stop{
    
    if([self isStarted]){
        
        [_recvice cancel];
        
        self.recvice = nil;
    }
    
}

-(NSTimeInterval) timestamp{
    if(_recvice){
        _timestamp = _recvice.timestamp;
    }
    return _timestamp;
}

-(void) setTimestamp:(NSTimeInterval)timestamp{
    _timestamp = timestamp;
    [_recvice setTimestamp:_timestamp];
}

-(void) setMaxIdle:(NSTimeInterval)maxIdle{
    [_recvice setMaxIdle:maxIdle];
    _maxIdle = maxIdle;
}

-(void) setMinIdle:(NSTimeInterval)minIdle{
    [_recvice setMinIdle:minIdle];
    _minIdle = minIdle;
}

-(void) setAddIdle:(NSTimeInterval)addIdle{
    [_recvice setAddIdle:addIdle];
    _addIdle = addIdle;
}

-(void) setIdle:(NSTimeInterval)idle{
    [_recvice setIdle:idle];
    _idle = idle;
}

-(NSTimeInterval) idle{
    if(_recvice){
        _idle =[_recvice idle];
    }
    return _idle;
}

-(void) onDidMessage:(vMessage *) message{
    if([_delegate respondsToSelector:@selector(vMessageClient:didRecvMessage:)]){
        [_delegate vMessageClient:self didRecvMessage:message];
    }
}

-(void) onDidFailError:(NSError * ) error{
    
    [self stop];
    
    if([_delegate respondsToSelector:@selector(vMessageClient:didFailError:)]){
        [_delegate vMessageClient:self didFailError:error];
    }
}

-(void) setMaxConcurrentOperationCount:(NSInteger)cnt{
    if(cnt < 2){
        cnt = 2;
    }
    [super setMaxConcurrentOperationCount:2];
}

@end
