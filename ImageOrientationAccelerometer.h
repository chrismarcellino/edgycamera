//
//  ImageOrientationAccelerometer.h
//  ImageProcessing
//
//  Created by Chris Marcellino on 1/17/11.
//  Copyright 2011 Chris Marcellino. All rights reserved.
//

#import <UIKit/UIKit.h>


@interface ImageOrientationAccelerometer : NSObject <UIAccelerometerDelegate> {
    UIDeviceOrientation deviceOrientation;
    UIDeviceOrientation pendingDeviceOrientation;
    CFAbsoluteTime pendingTime;
    unsigned listeners;
}

+ (ImageOrientationAccelerometer *)sharedInstance;

@property(nonatomic, readonly) UIDeviceOrientation deviceOrientation;

- (void)beginGeneratingDeviceOrientationNotifications;
- (void)endGeneratingDeviceOrientationNotifications;

@end

extern NSString *const DeviceOrientationDidChangeNotification;
