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
#import <vMessageClient/vMessageResourcePublish.h>
#import <vMessageClient/vMessageResource.h>

#import <vSpeex/vSpeexStreamWriter.h>
#import <vSpeex/vSpeexRecorder.h>
#import <vSpeex/vSpeexStreamReader.h>
#import <vSpeex/vSpeexPlayer.h>

@interface ViewController : UIViewController<vMessageClientDelegate,UITextFieldDelegate,vMessagePublishDelegate,vSpeexRecorderDelegate,vMessageResourceDelegate>

@property (retain, nonatomic) IBOutlet UITextField *textField;
@property (retain, nonatomic) IBOutlet UITextView *textView;

- (IBAction)speakBeginAction:(id)sender;

- (IBAction)speakEndAction:(id)sender;

@end
