//
//  Bvh.cpp
//  ImageProcessing
//
//  Created by Chris Marcellino on 1/24/11.
//  Copyright 2011 Chris Marcellino. All rights reserved.
//

#import "opencv2/opencv.hpp"
#import "CvRectUtilities.hpp"
#import "Bvh.hpp"

BvhNode& BvhNode::operator=(const BvhNode& node)
{
    if (this != &node) {
        rect = node.rect;
        BvhNode *l = NULL;
        BvhNode *r = NULL;
        if (node.left) {
            try {
                l = new BvhNode(*node.left);
                r = new BvhNode(*node.right);
            } catch (...) {
                delete l;
                delete r;
                throw;
            }
        }
        delete left;
        left = l;
        delete right;
        right = r;
    }
    return *this;
}

void BvhNode::insert(const CvRect& newRect, bool skipContainedRects)
{
    if (skipContainedRects && !left && rectContainsRect(rect, newRect)) {
        return;
    }
    
    CvRect newBoundingBox = rectUnion(rect, newRect);
    
    if (!left) {
        assert(!right);
        left = new BvhNode(rect);
        right = new BvhNode(newRect);
        rect = newBoundingBox;
    } else {        
        int perimeter = rectPerimeter(newBoundingBox);
        CvRect ifLeftRect = rectUnion(left->rect, newRect);
        int ifLeftDifference = rectPerimeter(ifLeftRect) - rectPerimeter(left->rect);
        CvRect ifRightRect = rectUnion(right->rect, newRect);
        int ifRightDifference = rectPerimeter(ifRightRect) - rectPerimeter(right->rect);
        
        rect = newBoundingBox;
        
        if (ifLeftDifference < ifRightDifference && ifLeftDifference < perimeter / 8) {
            left->insert(newRect, skipContainedRects);
        } else if (ifRightDifference < perimeter / 8) {
            right->insert(newRect, skipContainedRects);
        } else if (ifLeftDifference < ifRightDifference) {
            BvhNode *temp = left;
            left = new BvhNode(ifLeftRect);
            left->left = temp;
            left->right = new BvhNode(newRect);
        } else {
            BvhNode *temp = right;
            right = new BvhNode(ifRightRect);
            right->left = temp;
            right->right = new BvhNode(newRect);
        }
    }
}

bool BvhNode::memberContains(int x, int y)
{
    if (!rectContainsPoint(rect, x, y)) {
        return false;
    }
    return !left || left->memberContains(x, y) || right->memberContains(x, y);
}

bool BvhNode::allMembersContaining(int x, int y, std::vector<CvRect>& members, bool remove)
{
    if (!rectContainsPoint(rect, x, y)) {
        return false;
    }
    if (!left) {
        members.push_back(rect);
        return remove;
    }
    bool removeLeft = left->allMembersContaining(x, y, members, remove);
    bool removeRight = right->allMembersContaining(x, y, members, remove);
    if (removeLeft && removeRight) {
        return true;
    } else if (removeLeft) {
        removeChild(left);
    } else if (removeRight) {
        removeChild(right);
    }
    return false;
}

bool BvhNode::allMembersIntersecting(const CvRect& aRect, std::vector<CvRect>& members, bool remove)
{
    if (!rectIntersectsRect(rect, aRect)) {
        return false;
    }
    if (!left) {
        members.push_back(rect);
        return remove;
    }
    bool removeLeft = left->allMembersIntersecting(aRect, members, remove);
    bool removeRight = right->allMembersIntersecting(aRect, members, remove);
    if (removeLeft && removeRight) {
        return true;
    } else if (removeLeft) {
        removeChild(left);
    } else if (removeRight) {
        removeChild(right);
    }
    return false;
}

bool BvhNode::getAnyRect(CvRect& rect, bool remove)
{
    BvhNode *node = this;
    BvhNode *prev = NULL;
    while (node->left) {
        prev = node;
        node = node->left;
    }
    rect = node->rect;
    
    if (remove && prev) {
        prev->removeChild(node);
        return false;
    }
    return remove;
}

void BvhNode::removeChild(BvhNode *leaf)
{
    assert(leaf == this->left || leaf == this->right);
    BvhNode *remaining = (leaf == this->left) ? right : left;
    rect = remaining->rect;
    
    BvhNode* l = remaining->left;
    BvhNode* r = remaining->right;
    remaining->left = NULL;     // avoid double deallocation
    remaining->right = NULL;
    
    delete left;
    delete right;
    left = l;
    right = r;
}
