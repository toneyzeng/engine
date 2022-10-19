// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

#include "flutter/fml/macros.h"
#include "flutter/testing/testing.h"
#include "impeller/playground/playground.h"

namespace impeller {

class PlaygroundTest : public Playground,
                       public ::testing::TestWithParam<PlaygroundBackend> {
 public:
  PlaygroundTest();

  virtual ~PlaygroundTest();

  void SetUp() override;

  void TearDown() override;

  // |Playground|
  std::unique_ptr<fml::Mapping> OpenAssetAsMapping(
      std::string asset_name) const override;

  // |Playground|
  std::string GetWindowTitle() const override;

 private:
  FML_DISALLOW_COPY_AND_ASSIGN(PlaygroundTest);
};

#define INSTANTIATE_PLAYGROUND_SUITE(playground)                            \
  INSTANTIATE_TEST_SUITE_P(                                                 \
      Play, playground,                                                     \
      ::testing::Values(PlaygroundBackend::kMetal,                          \
                        PlaygroundBackend::kOpenGLES,                       \
                        PlaygroundBackend::kVulkan),                        \
      [](const ::testing::TestParamInfo<PlaygroundTest::ParamType>& info) { \
        return PlaygroundBackendToString(info.param);                       \
      });

}  // namespace impeller
