//
//  vMessage.h
//  vMessageClient
//
//  Created by zhang hailong on 13-6-26.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface vMessage : NSObject

@property(nonatomic,retain) NSString * from;
@property(nonatomic,retain) NSString * contentType;
@property(nonatomic,assign) NSUInteger contentLength;
@property(nonatomic,assign) NSTimeInterval timestamp;
@property(nonatomic,retain) NSData * body;
@property(nonatomic,readonly) id dataObject;

@end
