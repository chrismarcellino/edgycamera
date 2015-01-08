//
//  ImageOrientationAccelerometer.m
//  ImageProcessing
//
//  Created by Chris Marcellino on 1/17/11.
//  Copyright 2011 Chris Marcellino. All rights reserved.
//

#import "ImageOrientationAccelerometer.h"
#import <QuartzCore/QuartzCore.h>
#import <CoreMotion/CoreMotion.h>
#import "opencv2/opencv.hpp"

NSString *const DeviceOrientationDidChangeNotification = @"DeviceOrientationDidChangeNotification";
static const float orientationChangeHysteresis = 0.35;

@implementation ImageOrientationAccelerometer

@synthesize deviceOrientation;

+ (ImageOrientationAccelerometer *)sharedInstance
{
    static ImageOrientationAccelerometer *sharedInstance = nil;
    if (!sharedInstance) {
        sharedInstance = [[ImageOrientationAccelerometer alloc] init];
    }
    return sharedInstance;
}

- (void)beginGeneratingDeviceOrientationNotifications
{
    listeners++;
    
    if (listeners == 1) {
        if (!motionManager) {
            motionManager = [[CMMotionManager alloc] init];
        }
        [motionManager setAccelerometerUpdateInterval:0.1];
        [motionManager startAccelerometerUpdatesToQueue:[NSOperationQueue mainQueue] withHandler:^(CMAccelerometerData *accelerometerData, NSError *error) {
            [self accelerometerDataReceived:accelerometerData];
        }];
        
        // See the orientation from the OS if possible
        UIDevice *currentDevice = [UIDevice currentDevice];
        [currentDevice beginGeneratingDeviceOrientationNotifications];
        deviceOrientation = [currentDevice orientation];
        [currentDevice endGeneratingDeviceOrientationNotifications];
    }
}

- (void)endGeneratingDeviceOrientationNotifications
{
    NSAssert(listeners > 0, @"listener underflow");
    listeners--;
    
    if (listeners == 0) {
        [motionManager setAccelerometerUpdateInterval:0.0];
    }
}

- (void)accelerometerDataReceived:(CMAccelerometerData *)accelerometerData
{
    CMAcceleration acceleration = [accelerometerData acceleration];
    cv::Point3f accel(acceleration.x, acceleration.y, acceleration.z);
    
    // Bail if the acceleration magnitude is supernormal
    if (accel.dot(accel) > 1.2 * 1.2) {
        return;
    }
    
    cv::Point3f vectors[6] = {
        cv::Point3f(0.0, 0.0, -1.0),    // up
        cv::Point3f(0.0, 0.0, 1.0),     // down
        cv::Point3f(0.0, -1.0, 0.0),    // top
        cv::Point3f(0.0, 1.0, 0.0),     // bottom
        cv::Point3f(-1.0, 0.0, 0.0),    // left
        cv::Point3f(1.0, 0.0, 0.0)      // right
    };
    
    UIDeviceOrientation orientations[6] = {
        UIDeviceOrientationFaceUp,
        UIDeviceOrientationFaceDown,
        UIDeviceOrientationPortrait,
        UIDeviceOrientationPortraitUpsideDown,
        UIDeviceOrientationLandscapeLeft,
        UIDeviceOrientationLandscapeRight
    };
    
    // strongly weight the non-flat angles so that we rotate the image correctly whenever possible
    float weight[6] = {
        0.3,
        0.3,
        1.0,
        1.0,
        1.0,
        1.0
    };
    
    UIDeviceOrientation bestOrientation = UIDeviceOrientationPortrait;
    float bestDotProduct = -FLT_MAX;
    for (unsigned i = 0; i < 6; i++) {
        float dotProduct = accel.dot(vectors[i]) * weight[i];
        if (dotProduct > bestDotProduct) {
            bestOrientation = orientations[i];
            bestDotProduct = dotProduct;
        }
    }
    
    CFAbsoluteTime now = CACurrentMediaTime();
    if (pendingDeviceOrientation != bestOrientation) {
        pendingDeviceOrientation = bestOrientation;
        pendingTime = now;
    }
    
    if ((pendingDeviceOrientation != deviceOrientation && now - pendingTime > orientationChangeHysteresis) ||
        deviceOrientation == UIDeviceOrientationUnknown) {
        deviceOrientation = pendingDeviceOrientation;
        [[NSNotificationCenter defaultCenter] postNotificationName:DeviceOrientationDidChangeNotification object:self];
    }
}

@end
