//
//  vMessageClient.h
//  vMessageClient
//
//  Created by zhang hailong on 13-6-26.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import <UIKit/UIKit.h>

#import <vMessageClient/vMessage.h>

@interface vMessageClient : NSObject<NSStreamDelegate>

@property(nonatomic,readonly) NSURL * url;
@property(nonatomic,readonly) NSString * user;
@property(nonatomic,readonly) NSString * password;
@property(nonatomic,unsafe_unretained) id delegate;
@property(nonatomic,readonly,getter = isStarted) BOOL started;
@property(nonatomic,readonly) NSRunLoop * runloop;
@property(nonatomic,assign) NSTimeInterval timestamp;
@property(nonatomic,assign) NSTimeInterval minIdle;
@property(nonatomic,assign) NSTimeInterval idle;
@property(nonatomic,assign) NSTimeInterval maxIdle;
@property(nonatomic,assign) NSTimeInterval addIdle;

-(id) initWithURL:(NSURL *) url user:(NSString *) user password:(NSString *) password;

-(BOOL) start:(NSRunLoop *) runloop;

-(void) stop;


@end

@protocol vMessageClientDelegate

@optional

-(void) vMessageClient:(vMessageClient *) client didRecvMessage:(vMessage *) message;

-(void) vMessageClient:(vMessageClient *) client didFailError:(NSError *) error;

@end