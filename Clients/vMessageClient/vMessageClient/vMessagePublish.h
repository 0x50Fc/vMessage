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

@property(nonatomic,unsafe_unretained) id delegate;
@property(nonatomic,readonly) NSString * session;

-(id) initWithSession:(NSString *) session;

-(BOOL) willRequest:(CFHTTPMessageRef) request;

-(BOOL) didHasSpaceStream:(NSOutputStream *) stream;

-(void) didFinishResponse:(CFHTTPMessageRef) response;

-(void) didFailError:(NSError *) error;

@end

@protocol vMessagePublishDelegate

@optional

-(void) vMessagePublish:(vMessagePublish *) publish didFinishResponse:(CFHTTPMessageRef) response;

-(void) vMessagePublish:(vMessagePublish *) publish didFailError:(NSError *) error;

@end