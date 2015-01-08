//
//  EGEdgyAppDelegate.h
//  ImageProcessing
//
//  Created by Chris Marcellino on 12/30/2010.
//  Copyright Chris Marcellino 2010. All rights reserved.
//

#import "EGEdgyAppDelegate.h"
#import "EGCaptureController.h"
#import "EdgySHKConfigurator.h"
#import "SHK.h"
#import "SHKConfiguration.h"
#import "SHKFacebook.h"

@implementation EGEdgyAppDelegate

@synthesize window, captureController;

- (void)applicationDidFinishLaunching:(UIApplication *)application 
{
    // Load ShareKit configuration
    DefaultSHKConfigurator *configurator = [[EdgySHKConfigurator alloc] init];
    [SHKConfiguration sharedInstanceWithConfigurator:configurator];
    
    // Hide the status bar
    [application setStatusBarHidden:YES];
    
    // Create the window and main view controller
    window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    [window setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    
    captureController = [[EGCaptureController alloc] initWithNibName:nil bundle:nil];
    [window setRootViewController:captureController];
    [window makeKeyAndVisible];
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    [SHK flushOfflineQueue];
    [SHKFacebook handleDidBecomeActive];
}

// Facebook SSO

- (void)applicationWillTerminate:(UIApplication *)application
{
    // Save data if appropriate
    [SHKFacebook handleWillTerminate];
}

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
    NSString* scheme = [url scheme];
    NSString* prefix = [NSString stringWithFormat:@"fb%@", SHKCONFIG(facebookAppId)];
    if ([scheme hasPrefix:prefix])
        return [SHKFacebook handleOpenURL:url sourceApplication:sourceApplication];
    return YES;
}

@end
