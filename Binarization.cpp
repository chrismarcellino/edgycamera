//
//  Binarization.cpp
//  ImageProcessing
//
//  Created by Chris Marcellino on 12/31/10.
//  Copyright 2010 Chris Marcellino. All rights reserved.
//

#import "Binarization.hpp"
#import "CvRectUtilities.hpp"
#import "Bvh.hpp"
#import <vector>

static inline bool aspectRatioIsWithinTenthToTen(const CvRect& rect);
static int countOfContainedChildrenWithMinSize(CvContour* contour, int maxChildrenToCount, int minSize);
static int meanIntensityOfPixelsInContourInSubimage(IplImage* grayImg, CvContour* contour);
static inline uchar* pixelAddr(IplImage *img, CvPoint pt);
static inline uchar bgr2Gray(uchar bgr[3]);
static int median(std::vector<int> values);
static CvScalar randomRGBColor();

// Based on "Font and Background Color Independent Text Binarization", T Kasar, J Kumar and A G Ramakrishnan, 2007.
IplImage* createBinarizedImage(IplImage *img, double cannyLowThreshold, double cannyHighThreshold, int apertureSize)
{
    assert(img->nChannels >= 3);    // BGR image is required
    
    // Perform edge detection on each channel separately
    IplImage* channelImage = cvCreateImage(cvGetSize(img), img->depth, 1);
    IplImage* cannyEdgeOr = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
    IplImage* temp = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
    for (int i = 1; i <= 3; i++) {
        // Extract the channel data for this channel
        cvSetImageCOI(img, i);
        cvCopy(img, channelImage);
        cvResetImageROI(img);
        
        // Populate destination on first pass, then use 'temp' and logically OR the results
        cvCanny(channelImage, (i == 1) ? cannyEdgeOr : temp, cannyLowThreshold, cannyHighThreshold, apertureSize | CV_CANNY_L2_GRADIENT);
        if (i > 1) {
            cvOr(cannyEdgeOr, temp, cannyEdgeOr);
        }
    }
    cvReleaseImage(&temp);
    cvReleaseImage(&channelImage);
    
    // Get the contours and binarize the image
    CvContour* firstContour = NULL;
    CvMemStorage* storage = createStorageWithContours(cannyEdgeOr, &firstContour);      // modifies image
    IplImage* result = binarizeContours(img, firstContour);
    cvReleaseMemStorage(&storage);
    
    cvReleaseImage(&cannyEdgeOr);
    
    return result;
}

CvMemStorage* createStorageWithContours(IplImage* cannyEdgeImg, CvContour** firstContour, IplImage* debugContourImage, bool drawRects)
{
    // Find all of the conneted components in the image
    CvMemStorage* storage = cvCreateMemStorage();
    *firstContour = NULL;
    cvFindContours(cannyEdgeImg, storage, (CvSeq**)firstContour, sizeof(CvContour), CV_RETR_TREE);      // modifies image
    
    if (debugContourImage) {
        fastSetZero(debugContourImage);
        
        if (*firstContour) {
            CvTreeNodeIterator iterator;
            cvInitTreeNodeIterator(&iterator, *firstContour, INT_MAX);
            CvContour* contour;
            while ((contour = (CvContour*)cvNextTreeNode(&iterator)) != NULL) {
                cvDrawContours(debugContourImage, (CvSeq*)contour, randomRGBColor(), randomRGBColor(), 0);
                if (drawRects) {
                    CvRect rect = cvBoundingRect(contour);
                    cvRectangle(debugContourImage,
                                cvPoint(rect.x, rect.y),
                                cvPoint(rect.x + rect.width, rect.y + rect.height),
                                CV_RGB(0, 255, 255));
                }
            }
        }
    }
    
    return storage;
}

IplImage* binarizeContours(IplImage* originalImg,
                           CvContour* firstContour,
                           int largerDimensionMinimum,
                           int maxChildrenCount,
                           bool drawRects)
{
    // Iterate through the tree and locate all contours satisfying ALL of the following (in approx. optimized order):
    // 1. Largest dimension at least 8 pixels
    // 2. Have a bounding box whose aspect ratios are contained in (0.1, 1.0)
    // 3. Largest dimension at most 1/5 of the entire image
    // 4. Have a bounding box that does not intersect image border
    // 5. Has at most 4 components within that meet conditions 1-3 (since no Roman character has more than 2 interior components)
    // 6. Not contained by a contour that satisfies all other conditions
    // Note that condition 6 is satisifed if the tree is searched parent before child and the child branches pruned
    // when conditions 1-5 are matched.
    
    std::vector<CvContour*> acceptedContours;
    acceptedContours.reserve(1024);
    
    int imageLargerDimension = MAX(originalImg->width, originalImg->height);
    
    if (firstContour) {
        CvTreeNodeIterator iterator;
        cvInitTreeNodeIterator(&iterator, firstContour, INT_MAX);
        CvContour* contour;
        while ((contour = (CvContour*)cvNextTreeNode(&iterator)) != NULL) {
            // Condition 1
            CvRect rect = cvBoundingRect(contour);
            int largerDimension = MAX(rect.width, rect.height);
            if (largerDimension < largerDimensionMinimum) {
                continue;
            }
            
            // Condition 2
            if (!aspectRatioIsWithinTenthToTen(rect)) {
                continue;
            }
            
            // Condition 3
            if (largerDimension > imageLargerDimension / 5) {
                continue;
            }
            
            // Condition 4
            if (rect.x <= 1 || rect.x + rect.width >= originalImg->width - 1 || rect.y <= 1 || rect.y + rect.height >= originalImg->height - 1) {
                continue;
            }
            
            // Condition 5 (ensures children also satifies condition 1-4)
            if (countOfContainedChildrenWithMinSize(contour, maxChildrenCount + 1, largerDimensionMinimum) > maxChildrenCount) {
                continue;
            }
            
            // At this point, condition 6 has been satisfied since this node was reached.
            // To ensure this, we must delete all of our children nodes
            acceptedContours.push_back(contour);
            
            cvPrevTreeNode(&iterator);      // returned the next (potentially child) node, now pointing at current
            contour->v_next = NULL;
            cvNextTreeNode(&iterator);      // returned current, now pointing at next valid node            
        }
    }
    
    IplImage* result = cvCreateImage(cvGetSize(originalImg), IPL_DEPTH_8U, 1);
    memset(result->imageData, UCHAR_MAX, result->imageSize);
    
    // Iterate through all of the accepted contours
    for (size_t i = 0; i < acceptedContours.size(); i++) {
        CvContour* contour = acceptedContours[i];
        CvRect rect = cvBoundingRect(contour);
        
        // Create a grayscale subimage
        cvSetImageROI(originalImg, rect);
        IplImage* graySubimage = cvCreateImage(cvGetSize(originalImg), IPL_DEPTH_8U, 1);
        cvCvtColor(originalImg, graySubimage, (originalImg->nChannels == 3) ? CV_BGR2GRAY : CV_BGRA2GRAY);
        cvResetImageROI(originalImg);    
        
        // Estimate the foreground intensity of each box using the mean gray-level intensity of the pixels corresponding to the contour
        int foregroundIntensity = meanIntensityOfPixelsInContourInSubimage(graySubimage, contour);
        
        // Estimate the background intensity by sampling the 3 pixels at each corner of the bounding box.
        // Use the entire image since these points fall outside of the subimage rect.
        // This is safe because we exlcuded all rects that were within a pixel of the border in the selection phase.
        int cornerPoints[12] = {
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x - 1, rect.y - 1))),                              // ul
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x - 1, rect.y))),
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x, rect.y - 1))),
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x + rect.width + 1, rect.y - 1))),                 // ur
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x + rect.width, rect.y - 1))),
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x + rect.width + 1, rect.y))),
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x - 1, rect.y + rect.height + 1))),                // ll
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x - 1, rect.y + rect.height))),
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x, rect.y + rect.height + 1))),
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x + rect.width + 1, rect.y + rect.height + 1))),   // lr
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x + rect.width, rect.y + rect.height + 1))),
            bgr2Gray(pixelAddr(originalImg, cvPoint(rect.x + rect.width + 1, rect.y + rect.height)))
        };
        int backgroundIntensity = median(std::vector<int>(cornerPoints, cornerPoints + 12));
        
        // Threshold and copy to the destination bitmap and invert the output if the background has a higher intensity than the foreground
        IplImage* bwSubimage = cvCreateImage(cvGetSize(graySubimage), IPL_DEPTH_8U, 1);
        bool invert = foregroundIntensity > backgroundIntensity;
        cvThreshold(graySubimage, bwSubimage, foregroundIntensity, 255, invert ? CV_THRESH_BINARY_INV : CV_THRESH_BINARY);
        
        // Insert thresholded image into result
        cvSetImageROI(result, rect);
        cvCopy(bwSubimage, result);
        cvResetImageROI(result);
        
        if (drawRects) {
            cvRectangle(result, cvPoint(rect.x, rect.y), cvPoint(rect.x + rect.width, rect.y + rect.height), CV_RGB(255, 255, 0));
        }
        
        cvReleaseImage(&graySubimage);
        cvReleaseImage(&bwSubimage);
    }
    
    return result;
}

static inline bool aspectRatioIsWithinTenthToTen(const CvRect& rect)
{
    const int thousand = 1 << 10;
    int aspectRatioTimesThousand = thousand * rect.width / rect.height;
    return aspectRatioTimesThousand >= thousand / 10 && aspectRatioTimesThousand <= thousand * 10;
}

static int countOfContainedChildrenWithMinSize(CvContour* contour, int maxChildrenToCount, int minSize)
{
    int count = 0;
    
    for (CvContour* child = (CvContour*)contour->v_next; child && count < maxChildrenToCount; child = (CvContour*)child->h_next) {
        CvRect childRect = cvBoundingRect(child);
        int largerDimension = MAX(childRect.width, childRect.height);
        // Child must satisfy conditions 1-4 to be included in count. Conditions 3-4 are implicitly satisfied by the parent and do
        // not need to be checked. The child edge box must be completely cotained with the parent edge box.
        if (largerDimension > minSize &&
            aspectRatioIsWithinTenthToTen(childRect) &&
            rectContainsRect(cvBoundingRect(contour), childRect)) {
            count++;
            if (count < maxChildrenToCount) {
                count += countOfContainedChildrenWithMinSize(child, maxChildrenToCount - count, minSize);
            }
        }
    }
    
    return count;
}

// grayImg's origin is the origin of the cvBoundingRect(contour) subimage
static int meanIntensityOfPixelsInContourInSubimage(IplImage* grayImg, CvContour* contour)
{
    int intensitySum = 0;
    int totalPointsSampled = 0;
    
    CvSeqReader reader;        
    cvStartReadSeq((CvSeq*)contour, &reader);
    CvPoint pt1, pt2;
    int count = contour->total - !CV_IS_SEQ_CLOSED(contour);
    CV_READ_SEQ_ELEM(pt1, reader);
    // move pt1 into the subrect coordinate frame
    
    CvRect rect = cvBoundingRect(contour);
    pt1.x -= rect.x;
    pt1.y -= rect.y;
    for (int i = 0; i < count; i++) {
        CV_READ_SEQ_ELEM(pt2, reader);
        // move pt2 into the subrect coordinate frame
        pt2.x -= rect.x;
        pt2.y -= rect.y;
        
        // Add pt1's intensity
        intensitySum += *pixelAddr(grayImg, pt1);
        totalPointsSampled++;
        
        // Iterate through the points in the line exclusive of pt1 and pt2, summing the grayscale intensity
        CvLineIterator iterator;
        int points = cvInitLineIterator(grayImg, pt1, pt2, &iterator, 8);
        for (int j = 0; j < points; j++){
            intensitySum += iterator.ptr[0];
            totalPointsSampled++;
            CV_NEXT_LINE_POINT(iterator);
        }
        
        pt1 = pt2;
    }
    
    // Add final pt2 value if the contour is not closed
    if (!CV_IS_SEQ_CLOSED(contour)) {
        intensitySum += *pixelAddr(grayImg, pt2);
        totalPointsSampled++;
    }
    
    return totalPointsSampled ? (intensitySum / totalPointsSampled) : -1;
}

static inline uchar* pixelAddr(IplImage* img, CvPoint pt)
{
    assert(pt.x >= 0 && pt.x < img->width && pt.y >= 0 && pt.y < img->height);
    return &CV_IMAGE_ELEM(img, uchar, pt.y, pt.x * img->nChannels);
}

static inline uchar bgr2Gray(uchar bgr[3])
{
    return ((bgr[0] * 114 + bgr[1] * 587 + bgr[2] * 299) + 500) / 1000;
}

static int median(std::vector<int> values)
{
    std::sort(values.begin(), values.end());
    int middle = (int)(values.size() / 2);
    return (values.size() % 2) ? values[middle] : (values[middle - 1] + values[middle] + 1) / 2;
}

std::vector<CvRect> findContigousIslands(CvContour* firstContour, int borderPadding, int minSize)
{
    if (!firstContour) {
        return std::vector<CvRect>();
    }
    
    Bvh bvh;
    std::vector<CvRect> islands;

    // Iterate over every contour and create the bounding volume hierarchy    
    CvTreeNodeIterator iterator;
    cvInitTreeNodeIterator(&iterator, firstContour, INT_MAX);
    CvContour* contour;
    while ((contour = (CvContour*)cvNextTreeNode(&iterator)) != NULL) {
        bvh.insert(cvBoundingRect(contour), true);
    }
    
    // Iterate through all remaining contour rects, making each a new island
    std::vector<CvRect> intersecting;
    while (!bvh.empty()) {
        // Get an arbitary rect and remove it from the BVH
        CvRect rect = bvh.getAnyRect(true);
        
        intersecting.clear();
        intersecting.push_back(rect);
        CvRect boundingBox = rect;
        // Collect all transitively itersecting rects, iteratively, removing them from the BVH
        for (size_t i = 0; i < intersecting.size(); i++) {
            CvRect outset = outsetRect(intersecting[i], borderPadding, borderPadding);
            bvh.allMembersIntersecting(outset, intersecting, true);
            boundingBox = rectUnion(boundingBox, intersecting[i]);
        }
        
        // Add the bounding box to islands if it is large enough
        if (boundingBox.width > minSize || boundingBox.height > minSize) {
            islands.push_back(boundingBox);
        }
    }
    
    return islands;
}
