//
//  vMessageResourcePublish.h
//  vMessageClient
//
//  Created by Zhang Hailong on 13-6-29.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import <vMessageClient/vMessagePublish.h>

@interface vMessageResourcePublish : vMessagePublish

@property(nonatomic,retain) NSString * filePath;
@property(nonatomic,assign,getter = isFileEOF) BOOL fileEOF;
@property(nonatomic,retain) NSString * contentType;
@property(nonatomic,retain) NSString * urlencoded;

-(id) initWithSession:(NSString *)session filePath:(NSString *) filePath;

@end
