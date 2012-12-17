//
//  EGSHKActionSheet.h
//  Edgy
//
//  Created by Chris Marcellino on 12/16/12.
//  Copyright (c) 2012 Chris Marcellino. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "SHKActionSheet.h"

@interface EGSHKActionSheet : SHKActionSheet {
    void (^dismissHandler)(void);
}

- (void)setEGDismissHandler:(void (^)(void))handler;

@end