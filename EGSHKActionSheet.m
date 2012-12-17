//
//  EGSHKActionSheet.m
//  Edgy
//
//  Created by Chris Marcellino on 12/16/12.
//  Copyright (c) 2012 Chris Marcellino. All rights reserved.
//

#import "EGSHKActionSheet.h"

@implementation EGSHKActionSheet

- (void)dismissWithClickedButtonIndex:(NSInteger)buttonIndex animated:(BOOL)animated
{
    [super dismissWithClickedButtonIndex:buttonIndex animated:animated];
    if (dismissHandler) {
        dismissHandler();
        dismissHandler = nil;
    }
}

- (void)setEGDismissHandler:(void (^)(void))handler
{
    dismissHandler = [handler copy];
}

@end