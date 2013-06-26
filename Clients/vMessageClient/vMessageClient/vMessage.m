//
//  vMessage.m
//  vMessageClient
//
//  Created by zhang hailong on 13-6-26.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import "vMessage.h"

@implementation vMessage

@synthesize from = _from;
@synthesize contentType = _contentType;
@synthesize contentLength = _contentLength;
@synthesize timestamp = _timestamp;
@synthesize body = _body;

-(void) dealloc{
    [_from release];
    [_contentType release];
    [_body release];
    [super dealloc];
}

@end
