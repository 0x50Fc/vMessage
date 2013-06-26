//
//  vMessageClient.m
//  vMessageClient
//
//  Created by zhang hailong on 13-6-26.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import "vMessageClient.h"

#define INPUT_DATA_SIZE  20480

@interface vMessageClient(){
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
}

@property(nonatomic,retain) NSInputStream * inputStream;
@property(nonatomic,retain) NSOutputStream * outputStream;
@property(nonatomic,retain) NSData * outputData;
@property(nonatomic,retain) vMessage * message;

@end

@implementation vMessageClient

@synthesize url = _url;
@synthesize user = _user;
@synthesize password = _password;
@synthesize delegate = _delegate;
@synthesize started = _started;
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

-(void) dealloc{
    if(_response){
        CFRelease(_response);
    }
    [_url release];
    [_user release];
    [_password release];
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

-(id) initWithURL:(NSURL *) url user:(NSString *) user password:(NSString *) password{
    if((self = [super init])) {
        
        _url = [url retain];
        _user = [user retain];
        _password = [password retain];
        
        _minIdle = 0.03;
        _idle = 0.03;
        _maxIdle = 6.0;
        _addIdle = 0.03;
        
    }
    return self;
}

-(BOOL) sendRequest {
    
    NSString * host = [_url host];
    NSUInteger port = [[_url port] unsignedIntValue];
    
    if(port == 0){
        port = 80;
    }
    
    if(_response){
        CFRelease(_response);
        _response = nil;
    }
    
    _inputState.length = 0;
    _inputState.index = 0;
    _inputState.state = 0;
    
    _outputState.length = 0;
    _outputState.index = 0;
    
    self.outputData = nil;
    self.message = nil;
    
    [_inputStream close];
    [_inputStream setDelegate:nil];
    self.inputStream = nil;
    [_outputStream close];
    [_outputStream setDelegate:nil];
    self.outputStream = nil;
    
    
    CFStreamCreatePairWithSocketToHost(nil, (CFStringRef)host, port
                                       , (CFReadStreamRef *)&_inputStream
                                       , (CFWriteStreamRef *)&_outputStream);
    
    
    if(_inputStream && _outputStream){
        
        
        [_inputStream setDelegate:self];
        [_outputStream setDelegate:self];
        
        [_inputStream scheduleInRunLoop:_runloop forMode:NSRunLoopCommonModes];
        [_outputStream scheduleInRunLoop:_runloop forMode:NSRunLoopCommonModes];
        
        [_inputStream open];
        [_outputStream open];
        
        return YES;
    }

    return NO;
}

-(BOOL) start:(NSRunLoop *) runloop{
    if(!_started){
        
        [runloop retain];
        
        [_runloop release];
        
        _runloop = runloop;
        
        if([self sendRequest]){
            _started = YES;
        }
        
    }
    
    return NO;
}

-(void) stop{
    if(_started){
        
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendRequest) object:nil];
        
        [_inputStream close];
        [_inputStream setDelegate:nil];
        self.inputStream = nil;
        [_outputStream close];
        [_outputStream setDelegate:nil];
        self.outputStream = nil;
        
        [_runloop release];
        _runloop = nil;
        
        _started = NO;
    }
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
                        _inputState.length = [_inputStream read:_inputData maxLength:sizeof(_inputData)];
                        _inputState.index = 0;
                        _inputState.protocol = 0;
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
                                    
                                }
                                else if(*pByte == '\n'){
                                    
                                    if(_inputState.key && _inputState.value){
                                        NSString * key = [NSString stringWithCString:_inputState.key encoding:NSUTF8StringEncoding];
                                        NSString * value = [NSString stringWithCString:_inputState.value encoding:NSUTF8StringEncoding];
                                        
                                        CFHTTPMessageSetHeaderFieldValue(_response, (CFStringRef) key, (CFStringRef) value);
                                    }
                                    
                                    _inputState.key = 0;
                                    _inputState.value = 0;
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
                                CFStringRef encoding = CFHTTPMessageCopyHeaderFieldValue(_response, (CFStringRef) @"Transfer-Encoding");
                                
                                _inputState.chunked = [(id)encoding isEqualToString:@"chunked"];
                                
                                if(encoding){
                                    CFRelease(encoding);
                                }
                                
                                if(!_inputState.chunked){
                                    
                                    [_inputStream close];
                                    [_outputStream close];
                                    
                                    dispatch_async(dispatch_get_main_queue(), ^{
                                        
                                        if(self.started){
                                            if([_delegate respondsToSelector:@selector(vMessageClient:didFailError:)]){
                                                [_delegate vMessageClient:self didFailError:[NSError errorWithDomain:@"vMessageClient" code:-11 userInfo:[NSDictionary dictionaryWithObject:@"only support chunked" forKey:NSLocalizedDescriptionKey]]];
                                            }
                                        }
                                        
                                    });
                                }
                                else{
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
                                    
                                    if(atoi(_inputState.chunkedLength) ==0){
                                        
                                        [_inputStream close];
                                        [_outputStream close];
                                        
                                        dispatch_async(dispatch_get_main_queue(), ^{
                                           
                                            if(self.started){
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
                            }
                                break;
                            case 10:
                            {
                                // chunked body
                                if(_inputState.contentLength){
                                    if(_message.body == nil){
                                        _message.body = [NSMutableData dataWithCapacity:_inputState.contentLength];
                                    }
                                    [(NSMutableData *) _message appendBytes:pByte length:1];
                                    _inputState.contentLength --;
                                }
                                else if(*pByte == '\r'){
                                    
                                }
                                else if(*pByte == '\n'){
                                    _inputState.key = 0;
                                    _inputState.value = 0;
                                    _inputState.state = 7;
                                    
                                    {
                                        vMessage * msg = self.message;
                                        vMessageClient * client = self;
                                        dispatch_async(dispatch_get_main_queue(), ^{
                                            if(client.started){
                                                if([client.delegate respondsToSelector:@selector(vMessageClient:didRecvMessage:)]){
                                                    [client.delegate vMessageClient:client didRecvMessage:msg];
                                                }
                                            }
                                        });
                                        self.message = nil;
                                    }
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
                        [_outputStream close];
                    }
                    
                }
                else{
                    
                    CFHTTPMessageRef request = CFHTTPMessageCreateRequest(NULL, (CFStringRef) @"GET"
                                                                      , (CFURLRef) [NSURL URLWithString:[NSString stringWithFormat:@"%f",_timestamp] relativeToURL:_url]
                                                                      , (CFStringRef) @"1.1");
                    
                    
                    CFHTTPMessageAddAuthentication(request, NULL, (CFStringRef) _user, (CFStringRef) _password, kCFHTTPAuthenticationSchemeBasic, NO);
                    
                    
                    CFDataRef data = CFHTTPMessageCopySerializedMessage(request);
                    
                    NSUInteger length = [_outputStream write:CFDataGetBytePtr(data) maxLength:CFDataGetLength(data)];
                    
                    if(length < CFDataGetLength(data)){
                        self.outputData = (id)data;
                        _outputState.index = length;
                        _outputState.length = CFDataGetLength(data) - length;
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
            [_inputStream close];
            [_outputStream close];
            
            NSError * error = aStream.streamError;
            
            dispatch_async(dispatch_get_main_queue(), ^{
                if(self.started){
                    if([_delegate respondsToSelector:@selector(vMessageClient:didFailError:)]){
                        [_delegate vMessageClient:self didFailError:error];
                    }
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

@end
