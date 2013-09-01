//
//  vMessageReceive.h
//  vMessageClient
//
//  Created by Zhang Hailong on 13-8-31.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <vMessageClient/vMessage.h>

@class vMessageClient;

@interface vMessageReceive : NSOperation<NSStreamDelegate>{
    
}

@property(nonatomic,assign) id delegate;

@property(nonatomic,assign) NSTimeInterval timestamp;
@property(nonatomic,assign) NSTimeInterval minIdle;
@property(nonatomic,assign) NSTimeInterval idle;
@property(nonatomic,assign) NSTimeInterval maxIdle;
@property(nonatomic,assign) NSTimeInterval addIdle;

@end


@protocol vMessageReceiveDelegate

@optional

-(void) vMessageReceive:(vMessageReceive *) receive didRecvMessage:(vMessage *) message;

-(void) vMessageReceive:(vMessageReceive *) receive didFailError:(NSError *) error;

@end