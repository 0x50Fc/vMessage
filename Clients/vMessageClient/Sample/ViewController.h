//
//  ViewController.h
//  Sample
//
//  Created by zhang hailong on 13-6-26.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import <UIKit/UIKit.h>

#import <vMessageClient/vMessageClient.h>
#import <vMessageClient/vMessageBodyPublish.h>

@interface ViewController : UIViewController<vMessageClientDelegate,UITextFieldDelegate,vMessagePublishDelegate>

@property (retain, nonatomic) IBOutlet UITextField *textField;
@property (retain, nonatomic) IBOutlet UITextView *textView;

@end
