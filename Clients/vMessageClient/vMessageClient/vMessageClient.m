//
//  vMessageClient.m
//  vMessageClient
//
//  Created by zhang hailong on 13-6-26.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import "vMessageClient.h"

@interface vMessageClient(){
    
}

@end


@implementation vMessageClient

@synthesize url = _url;
@synthesize user = _user;
@synthesize password = _password;

-(void) dealloc{
    [_url release];
    [_user release];
    [_password release];
    [super dealloc];
}

-(id) initWithURL:(NSURL *) url user:(NSString *) user password:(NSString *) password {
    if((self = [super init])) {
        
        _url = [url retain];
        _user = [user retain];
        _password = [password retain];

        [self setMaxConcurrentOperationCount:5];
    }
    return self;
}

-(void) setMaxConcurrentOperationCount:(NSInteger)cnt{
    if(cnt < 2){
        cnt = 2;
    }
    [super setMaxConcurrentOperationCount:cnt];
}
@end
