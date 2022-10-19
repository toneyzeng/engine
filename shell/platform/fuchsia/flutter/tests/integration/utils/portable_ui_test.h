// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_FUCHSIA_FLUTTER_TESTS_INTEGRATION_UTILS_PORTABLE_UI_TEST_H_
#define FLUTTER_SHELL_PLATFORM_FUCHSIA_FLUTTER_TESTS_INTEGRATION_UTILS_PORTABLE_UI_TEST_H_

#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/ui/test/input/cpp/fidl.h>
#include <fuchsia/ui/test/scene/cpp/fidl.h>
#include <lib/async-loop/testing/cpp/real_loop.h>
#include <lib/sys/component/cpp/testing/realm_builder.h>
#include <lib/sys/component/cpp/testing/realm_builder_types.h>
#include <zircon/status.h>

#include <optional>
#include <vector>

namespace fuchsia_test_utils {
class PortableUITest : public ::loop_fixture::RealLoop {
 public:
  // The FIDL bindings for these services are not exposed in the Fuchsia SDK so
  // we must encode the names manually here.
  static constexpr auto kVulkanLoaderServiceName =
      "fuchsia.vulkan.loader.Loader";
  static constexpr auto kProfileProviderServiceName =
      "fuchsia.sheduler.ProfileProvider";
  static constexpr auto kTestUIStack = "ui";
  static constexpr auto kTestUIStackRef =
      component_testing::ChildRef{kTestUIStack};

  void SetUp();

  // Attaches a client view to the scene, and waits for it to render.
  void LaunchClient();

  // Returns true when the specified view is fully connected to the scene AND
  // has presented at least one frame of content.
  bool HasViewConnected(zx_koid_t view_ref_koid);

  // Registers a fake touch screen device with an injection coordinate space
  // spanning [-1000, 1000] on both axes.
  void RegisterTouchScreen();

  // Simulates a tap at location (x, y).
  void InjectTap(int32_t x, int32_t y);

 protected:
  component_testing::RealmBuilder* realm_builder() { return &realm_builder_; }
  component_testing::RealmRoot* realm_root() { return realm_.get(); }

  int touch_injection_request_count() const {
    return touch_injection_request_count_;
  }

 private:
  void SetUpRealmBase();

  // Configures the test-specific component topology.
  virtual void ExtendRealm() = 0;

  // Returns the test-ui-stack component url to use in this test.
  virtual std::string GetTestUIStackUrl() = 0;

  // Helper method to watch watch for view geometry updates.
  void WatchViewGeometry();

  // Helper method to process a view geometry update.
  void ProcessViewGeometryResponse(
      fuchsia::ui::observation::geometry::WatchResponse response);

  fuchsia::ui::test::input::RegistryPtr input_registry_;
  fuchsia::ui::test::input::TouchScreenPtr fake_touchscreen_;
  fuchsia::ui::test::scene::ControllerPtr scene_provider_;
  fuchsia::ui::observation::geometry::ViewTreeWatcherPtr view_tree_watcher_;

  component_testing::RealmBuilder realm_builder_ =
      component_testing::RealmBuilder::Create();
  std::unique_ptr<component_testing::RealmRoot> realm_;

  // Counts the number of completed requests to inject touch reports into input
  // pipeline.
  int touch_injection_request_count_ = 0;

  // The KOID of the client root view's `ViewRef`.
  std::optional<zx_koid_t> client_root_view_ref_koid_;

  // Holds the most recent view tree snapshot received from the view tree
  // watcher.
  //
  // From this snapshot, we can retrieve relevant view tree state on demand,
  // e.g. if the client view is rendering content.
  std::optional<fuchsia::ui::observation::geometry::ViewTreeSnapshot>
      last_view_tree_snapshot_;
};

}  // namespace fuchsia_test_utils

#endif  // FLUTTER_SHELL_PLATFORM_FUCHSIA_FLUTTER_TESTS_INTEGRATION_UTILS_PORTABLE_UI_TEST_H_
