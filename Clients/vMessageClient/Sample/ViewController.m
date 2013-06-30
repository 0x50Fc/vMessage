//
//  ViewController.m
//  Sample
//
//  Created by zhang hailong on 13-6-26.
//  Copyright (c) 2013年 hailong.org. All rights reserved.
//

#import "ViewController.h"


@interface ViewController ()

@property(nonatomic,retain) vMessageClient * client;
@property(nonatomic,retain) vMessageResourcePublish * speakPublish;
@property(nonatomic,retain) vSpeexRecorder * speakRecorder;
@end

@implementation ViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
    
    self.client = [[[vMessageClient alloc] initWithURL:[NSURL URLWithString:@"http://www.9vteam.com:9777"] user:@"hailong" password:@"12345678"] autorelease];
    
    [_client setMaxConcurrentOperationCount:10];
    [_client setDelegate:self];
    
    [_client start];
    
               
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

-(void) vMessageClient:(vMessageClient *) client didRecvMessage:(vMessage *) message{
    
    NSString * text = _textView.text;
    
    if(text == nil){
        text = @"";
    }
    
    NSLog(@"%@",message.contentType);
    NSLog(@"%@",message.dataObject);
    NSLog(@"%@",message.resourceURI);
    
    if([message.contentType isEqualToString:@"audio/speex"] && message.resourceURI){
        
        NSString * filePath = [[NSHomeDirectory() stringByAppendingPathComponent:@"Documents"] stringByAppendingPathComponent:message.resourceURI];
        
        [[NSFileManager defaultManager] createDirectoryAtPath:[filePath stringByDeletingLastPathComponent] withIntermediateDirectories:YES attributes:nil error:nil];
        
        vMessageResource * res = [[vMessageResource alloc] initWithClient:_client uri:message.resourceURI filePath:filePath];
        
        [res setDelegate:self];
        
        [_client addOperation:res];
        
        [res release];
        
        _textView.text = [NSString stringWithFormat:@"%@\n[语音]",text  ];
    
    }
    else{
        _textView.text = [NSString stringWithFormat:@"%@\n%@",text ,[message.dataObject valueForKey:@"body"] ];
    }

}

-(void) vMessageClient:(vMessageClient *) client didFailError:(NSError *) error{
    NSLog(@"%@",error);
    [client stop];
    [client performSelector:@selector(start) withObject:nil afterDelay:1.2];
    
}

- (void)dealloc {
    [_textField release];
    [_textView release];
    [super dealloc];
}

-(BOOL) textFieldShouldReturn:(UITextField *)textField{
    
    vMessageBodyPublish * publish = [[vMessageBodyPublish alloc] initWithClient:_client to:@"hailong" body:[NSDictionary dictionaryWithObject:textField.text forKey:@"body"]];
    
    [publish setDelegate:self];
    
    [_client addOperation:publish];
    
    [publish release];
    
    [textField setEnabled:NO];
    
    return YES;
}

-(void) vMessagePublish:(vMessagePublish *) publish didFinishResponse:(CFHTTPMessageRef) response{
    
    NSLog(@"%ld",CFHTTPMessageGetResponseStatusCode(response));
    
    [_textField setText:@""];
    [_textField setEnabled:YES];
}

-(void) vMessagePublish:(vMessagePublish *) publish didFailError:(NSError *) error{
    
    [_textField setEnabled:YES];
    
    UIAlertView * alertView = [[UIAlertView alloc] initWithTitle:nil message:[error localizedDescription] delegate:nil cancelButtonTitle:@"确定" otherButtonTitles: nil];
    
    [alertView show];
    
    [alertView release];
}


-(void) vMessageResource:(vMessageResource *) resource didFailError:(NSError *) error{
    NSLog(@"%@",error);
}

-(void) vMessageResourceDidFinished:(vMessageResource *) resource{
    
    vSpeexStreamReader * reader = [[[vSpeexStreamReader alloc] initWithFilePath:[resource filePath]] autorelease];
    
    vSpeexPlayer * player = [[[vSpeexPlayer alloc] initWithReader:reader] autorelease];
    
    
    [_client addOperation:player];
    
}

- (IBAction)speakBeginAction:(id)sender {

        
    NSString * tmpFile = [NSTemporaryDirectory() stringByAppendingPathComponent:@"s.spx"];
    
    vSpeex * speex = [[[vSpeex alloc] initWithMode:vSpeexModeNB] autorelease];
    
    vSpeexStreamWriter * writer = [[[vSpeexStreamWriter alloc] initWithFilePath:tmpFile speex:speex] autorelease];
    
    self.speakRecorder = [[[vSpeexRecorder alloc] initWithWriter:writer] autorelease];
    
    [_speakRecorder setDelegate:self];
    
    [_client addOperation:_speakRecorder];
    

    self.speakPublish = [[[vMessageResourcePublish alloc] initWithClient:_client to:@"hailong" filePath:tmpFile] autorelease];
    
    [_speakPublish setContentType:@"audio/speex"];
    [_speakPublish setFileEOF:NO];
    [_speakPublish setDelegate:self];
    
    [_client addOperation:_speakPublish];
    
    NSLog(@"speakBeginAction");
}

- (IBAction)speakEndAction:(id)sender {
    
    [_speakRecorder cancel];
    
    NSLog(@"speakEndAction");
}

-(void) vSpeexRecorderDidStarted:(vSpeexRecorder *) recorder{
    
}

-(void) vSpeexRecorderDidStoped:(vSpeexRecorder *)recorder{
    
    [_speakPublish setFileEOF:YES];
}

@end
