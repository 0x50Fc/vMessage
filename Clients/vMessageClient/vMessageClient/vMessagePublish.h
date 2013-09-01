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

-(BOOL) willRequest:(CFHTTPMessageRef) request client:(vMessageClient *) client;

-(BOOL) didHasSpaceStream:(NSOutputStream *) stream client:(vMessageClient *) client;

-(void) didFinishResponse:(CFHTTPMessageRef) response client:(vMessageClient *) client;

-(void) didFailError:(NSError *) error client:(vMessageClient *) client;

@end

@protocol vMessagePublishDelegate

@optional

-(void) vMessagePublish:(vMessagePublish *) publish didFinishResponse:(CFHTTPMessageRef) response client:(vMessageClient *) client;

-(void) vMessagePublish:(vMessagePublish *) publish didFailError:(NSError *) error client:(vMessageClient *) client;

@end