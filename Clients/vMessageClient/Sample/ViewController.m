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
    
    self.client = [[[vMessageClient alloc] initWithURL:[NSURL URLWithString:@"http://localhost:61185"] user:@"hailong" password:@"12345678"] autorelease];
    
    [_client setDelegate:self];
    
    [_client start:[NSRunLoop currentRunLoop]];
    
    
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

-(void) vMessageClient:(vMessageClient *) client didRecvMessage:(vMessage *) message{
    
}

-(void) vMessageClient:(vMessageClient *) client didFailError:(NSError *) error{
    
}

@end
