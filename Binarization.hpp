//
//  Binarization.hpp
//  ImageProcessing
//
//  Created by Chris Marcellino on 12/31/10.
//  Copyright 2010 Chris Marcellino. All rights reserved.
//

#import "opencv2/opencv.hpp"

IplImage* createBinarizedImage(IplImage *img,
                               double cannyLowThreshold = 50.0,
                               double cannyHighThreshold = 100.0,
                               int apertureSize = 3);

CvMemStorage* createStorageWithContours(IplImage* cannyEdgeImg,     // modifies cannyEdgeImg
                                        CvContour** firstContour,
                                        IplImage* debugContourImage = NULL,
                                        bool drawRects = false);
IplImage* binarizeContours(IplImage* originalImg,
                           CvContour* firstContour,
                           int largerDimensionMinimum = 8,
                           int maxChildrenCount = 4,
                           bool drawRects = false);

std::vector<CvRect> findContigousIslands(CvContour* firstContour, int borderPadding, int minSize);

static inline void fastSetZero(IplImage *image)
{
    assert(image->nChannels != 4);
    memset(image->imageData, 0, image->imageSize);
}

static inline CvScalar randomRGBColor()
{
    return CV_RGB(random() % 200 + 55, random() % 200 + 55, random() % 200 + 55);
}
