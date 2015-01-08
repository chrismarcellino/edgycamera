//
//  EGCaptureController.m
//  ImageProcessing
//
//  Created by Chris Marcellino on 8/26/10.
//  Copyright 2010 Chris Marcellino. All rights reserved.
//

#import "EGCaptureController.h"
#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#import "ShareKit.h"
#import "opencv2/opencv.hpp"
#import "EGEdgyView.h"
#import "UIImage-OpenCVExtensions.h"
#import "Binarization.hpp"        // for static inlines
#import "ImageOrientationAccelerometer.h"
#import "EGSHKActionSheet.h"


@interface EGCaptureController ()

- (void)setDefaultSettings;

- (void)startRunning;
- (void)stopRunning;
- (void)stopRunningAndResetSettings;
- (void)updateConfiguration;
- (void)orientationDidChange;

- (void)thresholdChanged:(id)sender;
- (void)cameraToggled:(id)sender;
- (void)colorToggled:(id)sender;
- (void)torchToggled:(id)sender;
- (void)captureImage:(id)sender;

@end


@implementation EGCaptureController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    if ((self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil])) {
        sampleProcessingQueue = dispatch_queue_create("sample processing", NULL);
        // Set up the session and output
#if TARGET_OS_EMBEDDED
        session = [[AVCaptureSession alloc] init];
        
        captureVideoDataOuput = [[AVCaptureVideoDataOutput alloc] init];
        [captureVideoDataOuput setSampleBufferDelegate:(id<AVCaptureVideoDataOutputSampleBufferDelegate>)self queue:sampleProcessingQueue];
        
        // Try to use YpCbCr as a first option for performance
        fallBackToBGRA32Sampling = NO;
        double osVersion = [[[UIDevice currentDevice] systemVersion] doubleValue];
        if (osVersion == 0.0 || osVersion >= 4.2) {
            // Try to use bi-planar YpCbCr first so that we can quickly extract Y'
            NSDictionary *settings = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:kCVPixelFormatType_420YpCbCr8BiPlanarFullRange] 
                                                                 forKey:(id)kCVPixelBufferPixelFormatTypeKey];

            @try {
                [captureVideoDataOuput setVideoSettings:settings];
            } @catch (...) {
                fallBackToBGRA32Sampling = YES;
            }
        } else {
            fallBackToBGRA32Sampling = YES;
        }
        
        if (fallBackToBGRA32Sampling) {
            NSLog(@"Falling back to BGRA32 sampling");
            // Fall back to BGRA32
            NSDictionary *settings = [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:kCVPixelFormatType_32BGRA] 
                                                                 forKey:(id)kCVPixelBufferPixelFormatTypeKey];
            [captureVideoDataOuput setVideoSettings:settings];
        }
        
        [session addOutput:captureVideoDataOuput];
#endif
        [self setDefaultSettings];
    }
    return self;
}

- (void)dealloc
{
#if TARGET_OS_EMBEDDED
    [session removeOutput:captureVideoDataOuput];
#endif
}

- (void)setDefaultSettings
{
    // Default to the front camera and a moderate threshold
    colorEdges = YES;
    deviceIndex = 1;
    cannyThreshold = 120;
}

- (void)loadView
{
    // Create the preview layer and view
    EGEdgyView *view = [[EGEdgyView alloc] initWithFrame:CGRectZero];
    [self setView:view];
    
    [[view torchButton] addTarget:self action:@selector(torchToggled:) forControlEvents:UIControlEventTouchUpInside];
    [[view captureButton] addTarget:self action:@selector(captureImage:) forControlEvents:UIControlEventTouchUpInside];
    [[view cameraToggle] addTarget:self action:@selector(cameraToggled:) forControlEvents:UIControlEventTouchUpInside];
    [[view colorToggle] addTarget:self action:@selector(colorToggled:) forControlEvents:UIControlEventTouchUpInside];
    [[view cannyThresholdSlider] addTarget:self action:@selector(thresholdChanged:) forControlEvents:UIControlEventValueChanged];
    [[view bannerView] setDelegate:self];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    
    // Monitor for orientation updates
    [[ImageOrientationAccelerometer sharedInstance] beginGeneratingDeviceOrientationNotifications];
    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self selector:@selector(orientationDidChange) name:DeviceOrientationDidChangeNotification object:nil];
    
    // Listen for app relaunch
    [defaultCenter addObserver:self selector:@selector(stopRunningAndResetSettings) name:UIApplicationDidEnterBackgroundNotification object:nil];
    [defaultCenter addObserver:self selector:@selector(startRunning) name:UIApplicationDidBecomeActiveNotification object:nil];
    
    // Listen for device updates
    [defaultCenter addObserver:self selector:@selector(updateConfiguration) name:AVCaptureDeviceWasConnectedNotification object:nil];
    [defaultCenter addObserver:self selector:@selector(updateConfiguration) name:AVCaptureDeviceWasDisconnectedNotification object:nil];
    
    [self startRunning];
    [self orientationDidChange];
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];
    
    [self stopRunning];
    
    [[ImageOrientationAccelerometer sharedInstance] endGeneratingDeviceOrientationNotifications];
    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter removeObserver:self name:DeviceOrientationDidChangeNotification object:nil];    
    [defaultCenter removeObserver:self name:UIApplicationDidEnterBackgroundNotification object:nil];
    [defaultCenter removeObserver:self name:UIApplicationDidBecomeActiveNotification object:nil];
    [defaultCenter removeObserver:self name:AVCaptureDeviceWasConnectedNotification object:nil];
    [defaultCenter removeObserver:self name:AVCaptureDeviceWasDisconnectedNotification object:nil];
}

- (void)startRunning
{
    [self updateConfiguration];
    [self performSelector:@selector(updateConfiguration) withObject:nil afterDelay:2.0];    // work around OS torch bugs
#if TARGET_OS_EMBEDDED
    [session startRunning];
#endif
}

- (void)stopRunning
{
#if TARGET_OS_EMBEDDED
    [session stopRunning];
#endif
}

- (void)stopRunningAndResetSettings
{
    [self setDefaultSettings];
    [self stopRunning];
}

- (void)updateConfiguration
{
    EGEdgyView *view = (EGEdgyView *)[self view];
    
#if TARGET_OS_EMBEDDED
    // Create the session
    [session beginConfiguration];
    
    // Choose the proper device and hide the device button if there is 0 or 1 devices
    NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    [[view cameraToggle] setHidden:[devices count] <= 1];
    
    deviceIndex %= [devices count];
    if (!currentDevice || ![[devices objectAtIndex:deviceIndex] isEqual:currentDevice]) {
        currentDevice = [devices objectAtIndex:deviceIndex];
        
        // Create the input and add it to the session
        if (input) {
            [session removeInput:input];
        }
        NSError *error = nil;
        input = [[AVCaptureDeviceInput alloc] initWithDevice:currentDevice error:&error];
        NSAssert1(input, @"no AVCaptureDeviceInput available: %@", error);
        [session addInput:input];
    }
    
    // Set the configuration
    if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad && [session canSetSessionPreset:AVCaptureSessionPreset640x480]) {
        [session setSessionPreset:AVCaptureSessionPreset640x480];
    } else if ([session canSetSessionPreset:AVCaptureSessionPresetMedium]) {
        [session setSessionPreset:AVCaptureSessionPresetMedium];
    }
    
    [currentDevice lockForConfiguration:nil];
    BOOL hasTorch = [currentDevice hasTorch];
    [[view torchButton] setHidden:!hasTorch];
    if (hasTorch) {
        [currentDevice setTorchMode:AVCaptureTorchModeOff];     // work around OS bugs
        [currentDevice setTorchMode:torchOn ? AVCaptureTorchModeOn : AVCaptureTorchModeOff];
        [[view torchButton] setSelected:torchOn];
    }
    [currentDevice unlockForConfiguration];
    
    // Limit the frame rate
    for (AVCaptureConnection *connection in [captureVideoDataOuput connections]) {
        [connection setVideoMinFrameDuration:CMTimeMake(1, 10)];  // 10 fps max
    }
    
    // Ensure the image view is rotated properly
    BOOL front = [currentDevice position] == AVCaptureDevicePositionFront;
    CGAffineTransform transform = CGAffineTransformMakeRotation(front ? -M_PI_2 : M_PI_2);
    if (front) {
        transform = CGAffineTransformScale(transform, -1.0, 1.0);
    }    
    [[view imageView] setTransform:transform];
    [view setNeedsLayout];
    
    [session commitConfiguration];
#endif
    
    // Ensure the slider value matches the settings
    [[view cannyThresholdSlider] setValue:cannyThreshold];
    
    // Update the image orientation to include the mirroring value as appropriate
    [self orientationDidChange];
}

- (void)orientationDidChange
{
    UIDeviceOrientation orientation = [[ImageOrientationAccelerometer sharedInstance] deviceOrientation];
    
    if (UIDeviceOrientationIsValidInterfaceOrientation(orientation)) {
        CGFloat buttonRotation;
        UIInterfaceOrientation interfaceOrientation;
        // Store the last unambigous orientation if not in capture mode
        switch (orientation) {
            default:
            case UIDeviceOrientationPortrait:
                buttonRotation = 0.0;
                imageOrientation = UIImageOrientationRight;
                interfaceOrientation = UIInterfaceOrientationPortrait;
                break;
            case UIDeviceOrientationPortraitUpsideDown:
                buttonRotation = M_PI;
                imageOrientation = UIImageOrientationLeft;
                interfaceOrientation = UIInterfaceOrientationPortraitUpsideDown;
                break;
            case UIDeviceOrientationLandscapeLeft:
                buttonRotation = M_PI_2;
                imageOrientation = UIImageOrientationUp;
                interfaceOrientation = UIInterfaceOrientationLandscapeLeft;
                break;
            case UIDeviceOrientationLandscapeRight:
                buttonRotation = -M_PI_2;
                imageOrientation = UIImageOrientationDown;
                interfaceOrientation = UIInterfaceOrientationLandscapeRight;
                break;
        }
        
        // Adjust button orientations
        [(EGEdgyView *)[self view] setButtonImageTransform:CGAffineTransformMakeRotation(buttonRotation) animated:YES];
        
        // Adjust the (hidden) status bar orientation so that sheets and modal view controllers appear in the proper orientation
        // and so touch hystereses are more accurate
        [[UIApplication sharedApplication] setStatusBarOrientation:interfaceOrientation];
    }
}

- (void)torchToggled:(id)sender
{
    torchOn = !torchOn;
    [self updateConfiguration];
}

- (void)captureImage:(id)sender
{
    EGEdgyView *view = (EGEdgyView *)[self view];
    
    // Prevent redundant button pressing and ensure we capture the visible image
    pauseForCapture = YES;  // MUST COME FIRST
    [view setUserInteractionEnabled:NO];
    
#if TARGET_IPHONE_SIMULATOR
    UIImage *image = [UIImage imageNamed:@"Default"];   // test code
#else
    // Get the current image and add rotation metadata, rotating the raw pixels if necessary
    UIImage *image = [[view imageView] image];
    if (!image) {
        pauseForCapture = NO;
        [view setUserInteractionEnabled:YES];
        return;
    }
#endif
    
    image = [UIImage imageWithCGImage:[image CGImage] scale:1.0 orientation:imageOrientation];
    IplImage *pixels = [image createIplImageWithNumberOfChannels:3];
#if TARGET_OS_EMBEDDED
    if ([currentDevice position] == AVCaptureDevicePositionFront) {
        cvFlip(pixels, NULL, (imageOrientation == UIImageOrientationUp || imageOrientation == UIImageOrientationDown) ? 0 : 1);        // flip vertically
    }
#endif
    image = [[UIImage alloc] initWithIplImage:pixels];
    cvReleaseImage(&pixels);
        
    // Create the item to share
    NSString *shareFormatString = NSLocalizedString(@"Photo from Edgy Camera, free on the App Store", nil);
    NSString *title = [[NSString alloc] initWithFormat:shareFormatString, [[UIDevice currentDevice] model]];
	SHKItem *item = [SHKItem image:image title:title];
    
	// Get the ShareKit action sheet and display it. Use our subclass so we can know when it gets dismissed.
	EGSHKActionSheet *actionSheet = [EGSHKActionSheet actionSheetForItem:item];
    [actionSheet setTitle:nil];
    [actionSheet setEGDismissHandler:^{
        pauseForCapture = NO;
        [view setUserInteractionEnabled:YES];
        [view restartFadeTimer];
    }];
    
    [(EGEdgyView *)[self view] clearFadeTimer];
    [actionSheet showFromRect:[[view captureButton] frame] inView:view animated:YES];
}

- (void)thresholdChanged:(id)sender
{
    cannyThreshold = [(UISlider *)sender value];
}

- (void)cameraToggled:(id)sender
{
    deviceIndex++;
    [self updateConfiguration];
}

- (void)colorToggled:(id)sender
{
    colorEdges = !colorEdges;
}

#if TARGET_OS_EMBEDDED
// Called on the capture dispatch queue
- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
    if (pauseForCapture) {
        return;
    }
    
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    
    // Lock the image buffer and get information about the image's *Y plane*
    CVPixelBufferLockBaseAddress(imageBuffer, 0); 
    void *baseAddress = fallBackToBGRA32Sampling ? CVPixelBufferGetBaseAddress(imageBuffer) : CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0);
    size_t bytesPerRow = fallBackToBGRA32Sampling ? CVPixelBufferGetBytesPerRow(imageBuffer) : CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 0);
    size_t width = fallBackToBGRA32Sampling ? CVPixelBufferGetWidth(imageBuffer) : CVPixelBufferGetWidthOfPlane(imageBuffer, 0);
    size_t height = fallBackToBGRA32Sampling ? CVPixelBufferGetHeight(imageBuffer) : CVPixelBufferGetHeightOfPlane(imageBuffer, 0);
    CvSize size = cvSize(width, height);
    
    // Create an image header to hold the data. Vector copy the data since the image buffer has very slow random access performance.
    IplImage *grayscaleImage = cvCreateImageHeader(size, IPL_DEPTH_8U, fallBackToBGRA32Sampling ? 4 : 1);
    grayscaleImage->widthStep = bytesPerRow;
    grayscaleImage->imageSize = bytesPerRow * height;
    cvCreateData(grayscaleImage);
    memcpy(grayscaleImage->imageData, baseAddress, height * bytesPerRow);
    CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
    
    // If we fell back, we need to convert the image to grayscale
    if (fallBackToBGRA32Sampling) {
        IplImage *temp = cvCreateImage(size, IPL_DEPTH_8U, 1);
        cvCvtColor(grayscaleImage, temp, CV_BGRA2GRAY);
        cvReleaseImage(&grayscaleImage);
        grayscaleImage = temp;
    }
    
    // Get the Canny edge image
    IplImage *cannyEdgeImage = cvCreateImage(size, IPL_DEPTH_8U, 1);
    cvCanny(grayscaleImage, cannyEdgeImage, 40.0, cannyThreshold, 3 | CV_CANNY_L2_GRADIENT);
    cvReleaseImage(&grayscaleImage);
    
    // Find each unique contour
    CvContour *firstContour = NULL;
    CvMemStorage *storage = cvCreateMemStorage();
    cvFindContours(cannyEdgeImage, storage, (CvSeq **)&firstContour, sizeof(CvContour), CV_RETR_LIST);      // modifies images
    cvReleaseImage(&cannyEdgeImage);
    
    // Color each contour
    IplImage *colorEdgeImage = cvCreateImage(size, IPL_DEPTH_8U, 3);
    fastSetZero(colorEdgeImage);
    if (firstContour) {
        CvTreeNodeIterator iterator;
        cvInitTreeNodeIterator(&iterator, firstContour, INT_MAX);
        CvContour *contour;
        while ((contour = (CvContour*)cvNextTreeNode(&iterator)) != NULL) {
            CvScalar color = colorEdges ? randomRGBColor() : CV_RGB(255, 255, 255);
            cvDrawContours(colorEdgeImage, (CvSeq *)contour, color, color, 0);
        }
    }
    cvReleaseMemStorage(&storage);
    
    // Send the image data to the main thread for display. Block so we aren't drawing while processing.
    dispatch_sync(dispatch_get_main_queue(), ^{
        if (!pauseForCapture) {
            UIImageView *imageView = [(EGEdgyView *)[self view] imageView];
            UIImage *uiImage = [[UIImage alloc] initWithIplImage:colorEdgeImage];
            [imageView setImage:uiImage];
        }
    });
    
    cvReleaseImage(&colorEdgeImage);
    
#if PRINT_PERFORMANCE
    static CFAbsoluteTime lastUpdateTime = 0.0;
    CFAbsoluteTime currentTime = CACurrentMediaTime();
    if (lastUpdateTime) {
        NSLog(@"Processing time: %.3f (fps %.1f) size(%u,%u)",
              currentTime - lastUpdateTime,
              1.0 / (currentTime - lastUpdateTime),
              size.width,
              size.height);
    }
    lastUpdateTime = currentTime;
#endif
}
#endif

- (void)bannerViewDidLoadAd:(ADBannerView *)banner
{
    [banner setHidden:NO];
    [banner setAlpha:0.0];
    [[self view] setNeedsLayout];
    
    [UIView beginAnimations:nil context:NULL];
    [banner setAlpha:1.0];
    [UIView commitAnimations];
}

- (void)bannerView:(ADBannerView *)banner didFailToReceiveAdWithError:(NSError *)error
{
    [banner setHidden:YES];
    [[self view] setNeedsLayout];
}

- (BOOL)bannerViewActionShouldBegin:(ADBannerView *)banner willLeaveApplication:(BOOL)willLeave
{
    pauseForCapture = YES;
    return YES;
}

- (void)bannerViewActionDidFinish:(ADBannerView *)banner
{
    pauseForCapture = NO;
}

@end
