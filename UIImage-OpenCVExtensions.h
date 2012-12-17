//
//  UIImage-OpenCVExtensions.h
//  ImageProcessing
//
//  Created by Chris Marcellino on 1/1/11.
//  Copyright 2011 Chris Marcellino. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "opencv2/opencv.hpp"

@interface UIImage (OpenCVExtensions)

// Creates an IplImage in gray, BGR or BGRA format. It is the caller's responsibility to cvReleaseImage() the return value.
- (IplImage *)createIplImageWithNumberOfChannels:(int)channels;

// Returns a UIImage by copying the IplImage's bitmap data. 
- (id)initWithIplImage:(IplImage *)iplImage;
- (id)initWithIplImage:(IplImage *)iplImage orientation:(UIImageOrientation)orientation;

// Returns an affine transform that takes into account the image orientation when drawing a scaled image
- (CGAffineTransform)transformForOrientationDrawnTransposed:(BOOL *)drawTransposed;

@end
