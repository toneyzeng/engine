// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <OCMock/OCMock.h>
#import <XCTest/XCTest.h>

#include "flutter/common/constants.h"
#include "flutter/shell/platform/darwin/common/framework/Headers/FlutterMacros.h"
#import "flutter/shell/platform/darwin/ios/framework/Source/FlutterDartProject_Internal.h"

FLUTTER_ASSERT_ARC

@interface FlutterDartProjectTest : XCTestCase
@end

@implementation FlutterDartProjectTest

- (void)setUp {
}

- (void)tearDown {
}

- (void)testOldGenHeapSizeSetting {
  FlutterDartProject* project = [[FlutterDartProject alloc] init];
  int64_t old_gen_heap_size =
      std::round([NSProcessInfo processInfo].physicalMemory * .48 / flutter::kMegaByteSizeInBytes);
  XCTAssertEqual(project.settings.old_gen_heap_size, old_gen_heap_size);
}

- (void)testResourceCacheMaxBytesThresholdSetting {
  FlutterDartProject* project = [[FlutterDartProject alloc] init];
  CGFloat scale = [UIScreen mainScreen].scale;
  CGFloat screenWidth = [UIScreen mainScreen].bounds.size.width * scale;
  CGFloat screenHeight = [UIScreen mainScreen].bounds.size.height * scale;
  size_t resource_cache_max_bytes_threshold = screenWidth * screenHeight * 12 * 4;
  XCTAssertEqual(project.settings.resource_cache_max_bytes_threshold,
                 resource_cache_max_bytes_threshold);
}

- (void)testMainBundleSettingsAreCorrectlyParsed {
  NSBundle* mainBundle = [NSBundle mainBundle];
  NSDictionary* appTransportSecurity =
      [mainBundle objectForInfoDictionaryKey:@"NSAppTransportSecurity"];
  XCTAssertTrue([FlutterDartProject allowsArbitraryLoads:appTransportSecurity]);
  XCTAssertEqualObjects(
      @"[[\"invalid-site.com\",true,false],[\"sub.invalid-site.com\",false,false]]",
      [FlutterDartProject domainNetworkPolicy:appTransportSecurity]);
}

- (void)testLeakDartVMSettingsAreCorrectlyParsed {
  // The FLTLeakDartVM's value is defined in Info.plist
  NSBundle* mainBundle = [NSBundle mainBundle];
  NSNumber* leakDartVM = [mainBundle objectForInfoDictionaryKey:@"FLTLeakDartVM"];
  XCTAssertEqual(leakDartVM.boolValue, NO);

  auto settings = FLTDefaultSettingsForBundle();
  // Check settings.leak_vm value is same as the value defined in Info.plist.
  XCTAssertEqual(settings.leak_vm, NO);
}

- (void)testEnableImpellerSettingIsCorrectlyParsed {
  // The FLTEnableImpeller's value is defined in Info.plist
  NSBundle* mainBundle = [NSBundle mainBundle];
  NSNumber* enableImpeller = [mainBundle objectForInfoDictionaryKey:@"FLTEnableImpeller"];
  XCTAssertEqual(enableImpeller.boolValue, NO);

  auto settings = FLTDefaultSettingsForBundle();
  // Check settings.enable_impeller value is same as the value defined in Info.plist.
  XCTAssertEqual(settings.enable_impeller, NO);
}

- (void)testEnableTraceSystraceSettingIsCorrectlyParsed {
  NSBundle* mainBundle = [NSBundle mainBundle];
  NSNumber* enableTraceSystrace = [mainBundle objectForInfoDictionaryKey:@"FLTTraceSystrace"];
  XCTAssertNotNil(enableTraceSystrace);
  XCTAssertEqual(enableTraceSystrace.boolValue, NO);
  auto settings = FLTDefaultSettingsForBundle();
  XCTAssertEqual(settings.trace_systrace, NO);
}

- (void)testEnableDartProflingSettingIsCorrectlyParsed {
  NSBundle* mainBundle = [NSBundle mainBundle];
  NSNumber* enableTraceSystrace = [mainBundle objectForInfoDictionaryKey:@"FLTEnableDartProfiling"];
  XCTAssertNotNil(enableTraceSystrace);
  XCTAssertEqual(enableTraceSystrace.boolValue, NO);
  auto settings = FLTDefaultSettingsForBundle();
  XCTAssertEqual(settings.trace_systrace, NO);
}

- (void)testEmptySettingsAreCorrect {
  XCTAssertFalse([FlutterDartProject allowsArbitraryLoads:[[NSDictionary alloc] init]]);
  XCTAssertEqualObjects(@"", [FlutterDartProject domainNetworkPolicy:[[NSDictionary alloc] init]]);
}

- (void)testAllowsArbitraryLoads {
  XCTAssertFalse([FlutterDartProject allowsArbitraryLoads:@{@"NSAllowsArbitraryLoads" : @false}]);
  XCTAssertTrue([FlutterDartProject allowsArbitraryLoads:@{@"NSAllowsArbitraryLoads" : @true}]);
}

- (void)testProperlyFormedExceptionDomains {
  NSDictionary* domainInfoOne = @{
    @"NSIncludesSubdomains" : @false,
    @"NSExceptionAllowsInsecureHTTPLoads" : @true,
    @"NSExceptionMinimumTLSVersion" : @"4.0"
  };
  NSDictionary* domainInfoTwo = @{
    @"NSIncludesSubdomains" : @true,
    @"NSExceptionAllowsInsecureHTTPLoads" : @false,
    @"NSExceptionMinimumTLSVersion" : @"4.0"
  };
  NSDictionary* domainInfoThree = @{
    @"NSIncludesSubdomains" : @false,
    @"NSExceptionAllowsInsecureHTTPLoads" : @true,
    @"NSExceptionMinimumTLSVersion" : @"4.0"
  };
  NSDictionary* exceptionDomains = @{
    @"domain.name" : domainInfoOne,
    @"sub.domain.name" : domainInfoTwo,
    @"sub.two.domain.name" : domainInfoThree
  };
  NSDictionary* appTransportSecurity = @{@"NSExceptionDomains" : exceptionDomains};
  XCTAssertEqualObjects(@"[[\"domain.name\",false,true],[\"sub.domain.name\",true,false],"
                        @"[\"sub.two.domain.name\",false,true]]",
                        [FlutterDartProject domainNetworkPolicy:appTransportSecurity]);
}

- (void)testExceptionDomainsWithMissingInfo {
  NSDictionary* domainInfoOne = @{@"NSExceptionMinimumTLSVersion" : @"4.0"};
  NSDictionary* domainInfoTwo = @{
    @"NSIncludesSubdomains" : @true,
  };
  NSDictionary* domainInfoThree = @{};
  NSDictionary* exceptionDomains = @{
    @"domain.name" : domainInfoOne,
    @"sub.domain.name" : domainInfoTwo,
    @"sub.two.domain.name" : domainInfoThree
  };
  NSDictionary* appTransportSecurity = @{@"NSExceptionDomains" : exceptionDomains};
  XCTAssertEqualObjects(@"[[\"domain.name\",false,false],[\"sub.domain.name\",true,false],"
                        @"[\"sub.two.domain.name\",false,false]]",
                        [FlutterDartProject domainNetworkPolicy:appTransportSecurity]);
}

@end
