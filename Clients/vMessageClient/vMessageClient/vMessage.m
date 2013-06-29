//
//  vMessage.m
//  vMessageClient
//
//  Created by zhang hailong on 13-6-26.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import "vMessage.h"

@implementation NSString (vMessage)

-(NSString *) stringByAddingMessageEscapes{
    
    NSUInteger length = [self length];
    
    NSMutableString * ms = [NSMutableString stringWithCapacity:MAX(length, 32)];
    
    for(NSUInteger i=0;i<length;i++){
        
        unichar uchar = [self characterAtIndex:i];
        
        if(uchar & 0xff00){
            [ms appendFormat:@"%%u%02x%02x",uchar >> 8, (uchar & 0x00ff)];
        }
        else if((uchar >='0' && uchar <='9') || (uchar >= 'A' && uchar <= 'Z') || (uchar >= 'a' && uchar <= 'z')){
            [ms appendFormat:@"%c",uchar];
        }
        else {
            [ms appendFormat:@"%%%02x",(uchar & 0x00ff)];
        }
    }

    return ms;
}

-(NSString *) stringByReplacingMessageEscapes{
    
    NSUInteger length = [self length];
    
    NSMutableString * ms = [NSMutableString stringWithCapacity:MAX(length, 32)];
    
    char * p = (char *)[self UTF8String];
    
    unsigned int lc,hc;
    unichar uchar;
    
    while(p && *p != '\0'){
        
        if(*p == '%'){
            if(p[1] == 'u' || p[1] == 'U'){
                lc = hc = 0;
                sscanf(p + 2, "%2x%2x",&hc,&lc);
                uchar = (hc << 8) | lc;
                [ms appendString:[NSString stringWithCharacters:&uchar length:1]];
                p += 5;
            }
            else{
                lc = hc = 0;
                sscanf(p + 1, "%2x",&hc);
                [ms appendFormat:@"%c",hc];
                p += 2;
            }
        }
        else {
            [ms appendFormat:@"%c",*p];
        }
        
        p++;
    }
    
    return ms;
    
}


@end

@implementation vMessage

@synthesize from = _from;
@synthesize contentType = _contentType;
@synthesize contentLength = _contentLength;
@synthesize timestamp = _timestamp;
@synthesize body = _body;
@synthesize dataObject = _dataObject;
@synthesize resourceURI = _resourceURI;

-(void) dealloc{
    [_from release];
    [_contentType release];
    [_body release];
    [_dataObject release];
    [_resourceURI release];
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
                    [_dataObject setValue:[[kv objectAtIndex:1] stringByReplacingMessageEscapes] forKey:[kv objectAtIndex:0]];
                }
            }
            [text release];
        }
    }
    return _dataObject;
}

@end
