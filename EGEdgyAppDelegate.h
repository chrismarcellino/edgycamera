//
//  EGEdgyAppDelegate.h
//  ImageProcessing
//
//  Created by Chris Marcellino on 12/30/2010.
//  Copyright Chris Marcellino 2010. All rights reserved.
//

#import <UIKit/UIKit.h>

@class EGCaptureController;

@interface EGEdgyAppDelegate : NSObject <UIApplicationDelegate> {
    UIWindow *window;
    EGCaptureController *captureController;
}

@property(retain, nonatomic) UIWindow *window;
@property(retain, nonatomic) EGCaptureController *captureController;

@end
