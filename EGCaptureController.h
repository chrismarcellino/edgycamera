//
//  EGCaptureController.h
//  ImageProcessing
//
//  Created by Chris Marcellino on 8/26/10.
//  Copyright 2010 Chris Marcellino. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <CoreMedia/CoreMedia.h>
#import <iAd/iAd.h>

@class AVCaptureSession;
@class AVCaptureDevice;
@class AVCaptureDeviceInput;
@class AVCaptureVideoDataOutput;
@class EGEdgyView;

@interface EGCaptureController : UIViewController <ADBannerViewDelegate> {
    AVCaptureSession *session;
    AVCaptureDevice *currentDevice;
    AVCaptureDeviceInput *input;
    AVCaptureVideoDataOutput *captureVideoDataOuput;
    dispatch_queue_t sampleProcessingQueue;
    UIImageOrientation imageOrientation;
    
    NSUInteger deviceIndex;
    BOOL torchOn;
    BOOL colorEdges;
    NSUInteger cannyThreshold;
    
    BOOL pauseForCapture;
    BOOL fallBackToBGRA32Sampling;
}

@end
