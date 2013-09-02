//
//  vMessageResource.h
//  vMessageClient
//
//  Created by Zhang Hailong on 13-6-29.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <vMessageClient/vMessageClient.h>

@interface vMessageResource : NSOperation<NSStreamDelegate>

@property(nonatomic,readonly) NSString * filePath;
@property(nonatomic,readonly) NSString * uri;
@property(nonatomic,unsafe_unretained) id delegate;

-(id) initWithUri:(NSString *) uri filePath:(NSString *) filePath;

-(void) didResponse:(CFHTTPMessageRef) response client:(vMessageClient *) client;

-(void) didRecvBytes:(NSUInteger ) bytes contentLength:(NSUInteger) contentLength client:(vMessageClient *) client;

-(void) didFailError:(NSError *) error client:(vMessageClient *) client;

-(void) didFinished:(vMessageClient *) client;

@end

@protocol vMessageResourceDelegate

@optional

-(void) vMessageResource:(vMessageResource *) resource didResponse:(CFHTTPMessageRef) response client:(vMessageClient *) client;

-(void) vMessageResource:(vMessageResource *) resource didRecvBytes:(NSUInteger) bytes contentLength:(NSUInteger) contentLength client:(vMessageClient *) client;

-(void) vMessageResource:(vMessageResource *) resource didFailError:(NSError *) error client:(vMessageClient *) client;

-(void) vMessageResourceDidFinished:(vMessageResource *) resource client:(vMessageClient *) client;

@end
