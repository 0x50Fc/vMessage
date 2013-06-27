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

@end

@implementation ViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
    
    self.client = [[[vMessageClient alloc] initWithURL:[NSURL URLWithString:@"http://localhost:49307"] user:@"hailong" password:@"12345678"] autorelease];
    
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
    
    _textView.text = [NSString stringWithFormat:@"%@\n%@",text ,[message.dataObject valueForKey:@"body"] ];
    
    NSLog(@"%@",message.dataObject);
    
}

-(void) vMessageClient:(vMessageClient *) client didFailError:(NSError *) error{
    NSLog(@"%@",error);
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
    
    [_textField setText:@""];
    [_textField setEnabled:YES];
}

-(void) vMessagePublish:(vMessagePublish *) publish didFailError:(NSError *) error{
    
    [_textField setEnabled:YES];
    
    UIAlertView * alertView = [[UIAlertView alloc] initWithTitle:nil message:[error localizedDescription] delegate:nil cancelButtonTitle:@"确定" otherButtonTitles: nil];
    
    [alertView show];
    
    [alertView release];
}

@end
