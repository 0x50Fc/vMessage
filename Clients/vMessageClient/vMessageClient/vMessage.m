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
@synthesize dataObject = _dataObject;

-(void) dealloc{
    [_from release];
    [_contentType release];
    [_body release];
    [_dataObject release];
    [super dealloc];
}

-(id) dataObject{
    if(_dataObject == nil){
        if([_contentType isEqualToString:@"application/x-www-form-urlencoded"] && _body){
            NSString * text = [[NSString alloc] initWithData:_body encoding:NSUTF8StringEncoding];
            _dataObject = [[NSMutableDictionary alloc] initWithCapacity:4];
            NSArray * items = [text componentsSeparatedByString:@"&"];
            for(NSString * item in items){
                NSArray * kv = [item componentsSeparatedByString:@"="];
                if([kv count] == 2){
                    [_dataObject setValue:[[kv objectAtIndex:1] stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding] forKey:[kv objectAtIndex:0]];
                }
            }
            [text release];
        }
    }
    return _dataObject;
}

@end
