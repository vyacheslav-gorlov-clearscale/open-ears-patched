//
//  OEDynamicMethod.m
//  OpenEars
//
//  Created by Halle on 1/27/13.
//  Copyright (c) 2014 Politepix. All rights reserved.
//

#import "OEDynamicMethod.h"

@implementation OEDynamicMethod

+ (BOOL) callDynamicMethodFromName:(NSString *)name onObject:(id)object {

    SEL methodSelector = NSSelectorFromString(name);
    
    BOOL doesRespondToSelector = FALSE;
    
    if ([object respondsToSelector:methodSelector]) {
        
        doesRespondToSelector = TRUE;
        
        IMP imp = [object methodForSelector:methodSelector];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpedantic"          
        void (*func)(id, SEL) = (void *)imp;
#pragma clang diagnostic pop
        func(object, methodSelector);      
    }
    
    return doesRespondToSelector;
}

@end
