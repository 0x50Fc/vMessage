//
//  vMessageBodyPublish.m
//  vMessageClient
//
//  Created by zhang hailong on 13-6-27.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import "vMessageBodyPublish.h"

#include <objc/runtime.h>


@interface vMessageBodyPublish()

@property(nonatomic,retain) NSData * bodyBytes;

@end

@implementation vMessageBodyPublish

@synthesize body = _body;
@synthesize bodyBytes = _bodyBytes;

-(void) dealloc{
    [_body release];
    [_bodyBytes release];
    [super dealloc];
}

-(id) initWithSession:(NSString *)session body:(id) body;{
    
    if(body == nil){
        [self autorelease];
        return nil;
    }
    
    if((self = [super initWithSession:session])){
        _body = [body retain];
        if([body isKindOfClass:[NSString class]]){
            self.bodyBytes = [(NSString *)body dataUsingEncoding:NSUTF8StringEncoding];
        }
        else if([body isKindOfClass:[NSArray class]]){
            NSMutableString * s = [NSMutableString stringWithCapacity:512];
            NSInteger index = 1;
            for(id item in body){
                if(![item isKindOfClass:[NSString class]]){
                    item = [item description];
                }
                [s appendFormat:@"&%d=%@",index ++ , [item stringByAddingMessageEscapes]];
            }
            self.bodyBytes = [s dataUsingEncoding:NSUTF8StringEncoding];
        }
        else if([body isKindOfClass:[NSDictionary class]]){
            NSMutableString * s = [NSMutableString stringWithCapacity:512];
            for(NSString * key in body){
                id value = [body valueForKey:key];
                if(![value isKindOfClass:[NSString class]]){
                    value = [value description];
                }
                [s appendFormat:@"&%@=%@",key ,[value stringByAddingMessageEscapes]];
            }
            self.bodyBytes = [s dataUsingEncoding:NSUTF8StringEncoding];
        }
        else {
            NSMutableString * s = [NSMutableString stringWithCapacity:512];
            Class clazz = [body class];
            while(clazz && clazz != [NSObject class]){
                
                NSUInteger c = 0;
                objc_property_t * prop = class_copyPropertyList(clazz, & c);
                
                while(c >0){
                    
                    NSString * key = [NSString stringWithCString:property_getName(* prop) encoding:NSUTF8StringEncoding];
                    
                    id value = [body valueForKey:key];
                    
                    if(value){
                        [s appendFormat:@"&%@=%@",key ,[value stringByAddingMessageEscapes]];
                    }
                    
                    prop ++;
                    c --;
                }
                
                clazz = class_getSuperclass(clazz);
            }
            self.bodyBytes = [s dataUsingEncoding:NSUTF8StringEncoding];
        }
    }
    return self;
}

-(BOOL) willRequest:(CFHTTPMessageRef)request{
    
    CFHTTPMessageSetHeaderFieldValue(request, (CFStringRef) @"Content-Type"
                                     , (CFStringRef) @"application/x-www-form-urlencoded");
    
    CFHTTPMessageSetHeaderFieldValue(request, (CFStringRef) @"Content-Length"
                                     , (CFStringRef) [NSString stringWithFormat:@"%u",[_bodyBytes length]]);
    
    CFHTTPMessageSetBody(request, (CFDataRef) _bodyBytes);
    
    
    
    return YES;
}

-(BOOL) didHasSpaceStream:(NSOutputStream *)stream{
    return NO;
}

@end
