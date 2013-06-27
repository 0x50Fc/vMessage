//
//  vMessagePublish.h
//  vMessageClient
//
//  Created by zhang hailong on 13-6-27.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <vMessageClient/vMessageClient.h>

@interface vMessagePublish : NSOperation<NSStreamDelegate>

@property(nonatomic,readonly) vMessageClient * client;
@property(nonatomic,unsafe_unretained) id delegate;
@property(nonatomic,readonly) NSString * to;

-(id) initWithClient:(vMessageClient *) client to:(NSString *) to;

-(BOOL) willRequest:(CFHTTPMessageRef) request;

-(BOOL) hasSendData;

-(void) didHasSpaceStream:(NSOutputStream *) stream;

-(void) didFinishResponse:(CFHTTPMessageRef) response;

-(void) onDidFailError:(NSError *) error;

@end

@protocol vMessagePublishDelegate

@optional

-(void) vMessagePublish:(vMessagePublish *) publish didFinishResponse:(CFHTTPMessageRef) response;

-(void) vMessagePublish:(vMessagePublish *) publish didFailError:(NSError *) error;

@end