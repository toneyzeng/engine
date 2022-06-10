// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "flutter/shell/platform/darwin/macos/framework/Headers/FlutterViewController.h"

#import "flutter/shell/platform/darwin/macos/framework/Source/FlutterKeyboardViewDelegate.h"
#import "flutter/shell/platform/darwin/macos/framework/Source/FlutterTextInputPlugin.h"
#import "flutter/shell/platform/darwin/macos/framework/Source/FlutterView.h"

@interface FlutterViewController () <FlutterKeyboardViewDelegate>

// The FlutterView for this view controller.
@property(nonatomic, readonly, nullable) FlutterView* flutterView;

/**
 * The text input plugin that handles text editing state for text fields.
 */
@property(nonatomic, readonly, nonnull) FlutterTextInputPlugin* textInputPlugin;

/**
 * Initializes this FlutterViewController with the specified `FlutterEngine`.
 *
 * The initialized viewcontroller will attach itself to the engine as part of this process.
 *
 * @param engine The `FlutterEngine` instance to attach to. Cannot be nil.
 * @param nibName The NIB name to initialize this controller with.
 * @param nibBundle The NIB bundle.
 */
- (nonnull instancetype)initWithEngine:(nonnull FlutterEngine*)engine
                               nibName:(nullable NSString*)nibName
                                bundle:(nullable NSBundle*)nibBundle NS_DESIGNATED_INITIALIZER;

@end

// Private methods made visible for testing
@interface FlutterViewController (TestMethods)
- (void)onAccessibilityStatusChanged:(BOOL)enabled;
@end
