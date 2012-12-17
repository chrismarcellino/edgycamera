//
//  CvRectUtilities.hpp
//  ImageProcessing
//
//  Created by Chris Marcellino on 1/24/11.
//  Copyright 2011 Chris Marcellino. All rights reserved.
//

#import "opencv2/opencv.hpp"
#import "CvRectUtilities.hpp"

class BvhNode {
    friend class Bvh;
private:
    BvhNode(const CvRect& rect) : rect (rect), left (NULL), right (NULL) { }
    ~BvhNode() { delete left; delete right; }
    
    BvhNode(const BvhNode& node) { *this = node; }
    BvhNode& operator=(const BvhNode& node);
    
    void insert(const CvRect& newRect, bool skipContainedRects);
    bool memberContains(int x, int y);
    
    // return value of true indicates that the node should be deleted by parent to achieve removal
    bool allMembersContaining(int x, int y, std::vector<CvRect>& members, bool remove);
    bool allMembersIntersecting(const CvRect& aRect, std::vector<CvRect>& members, bool remove);
    bool getAnyRect(CvRect& rect, bool remove);
    
    void removeChild(BvhNode *leaf);
    
    CvRect rect;        // bounding box if children, value if leaf
    BvhNode *left;      // left is non-NULL iff right is non-NULL
    BvhNode *right;
};

// Stores hierarchies of axis-aligned rects for fast intersection and containment testing
class Bvh {
public:
    Bvh() : node (NULL) {};
    ~Bvh() { delete node; }
    Bvh(const Bvh& bvh) { *this = bvh; }
    Bvh& operator=(const Bvh& bvh) { delete node; node = bvh.node; return *this; };
    
    bool empty() { return node == NULL; }
    void clear() { delete node; node = NULL; }
    void insert(const CvRect& rect, bool skipContainedRects = false) {
        if (node) {
            node->insert(rect, skipContainedRects);
        } else {
            node = new BvhNode(rect);
        }
    }
    bool memberContains(int x, int y) { return node ? node->memberContains(x, y) : false; }
    void allMembersContaining(int x, int y, std::vector<CvRect>& members, bool remove = false) {
        if (node && node->allMembersContaining(x, y, members, remove)) {
            clear();
        }
    }
    void allMembersIntersecting(const CvRect& rect, std::vector<CvRect>& members, bool remove = false) {
        if (node && node->allMembersIntersecting(rect, members, remove)) {
            clear();
        }
    }
    CvRect getAnyRect(bool remove = false) {
        if (!node) {
            throw std::exception();
        }
        CvRect rect;
        if (node->getAnyRect(rect, remove)) {
            clear();
        }
        return rect;
    }
    
private:
    BvhNode *node;
};
