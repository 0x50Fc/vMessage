//
//  vMessageBodyPublish.h
//  vMessageClient
//
//  Created by zhang hailong on 13-6-27.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import <vMessageClient/vMessagePublish.h>

@interface vMessageBodyPublish : vMessagePublish

@property(nonatomic,retain) id body;

-(id) initWithClient:(vMessageClient *) client to:(NSString *) to body:(id) body;

@end
