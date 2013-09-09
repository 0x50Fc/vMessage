//
//  vMessageClient.h
//  vMessageClient
//
//  Created by zhang hailong on 13-6-26.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import <UIKit/UIKit.h>

#import <vMessageClient/vMessage.h>

#import <vMessageClient/vMessageReceive.h>

@interface vMessageClient : NSOperationQueue<NSStreamDelegate>

@property(nonatomic,readonly) NSURL * url;
@property(nonatomic,readonly) NSString * user;
@property(nonatomic,retain) NSString * password;

-(id) initWithURL:(NSURL *) url user:(NSString *) user password:(NSString *) password;

@end
