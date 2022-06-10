// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/aiks/aiks_playground.h"

#include "impeller/aiks/aiks_context.h"

namespace impeller {

AiksPlayground::AiksPlayground() = default;

AiksPlayground::~AiksPlayground() = default;

bool AiksPlayground::OpenPlaygroundHere(const Picture& picture) {
  return OpenPlaygroundHere(
      [&picture](AiksContext& renderer, RenderTarget& render_target) -> bool {
        return renderer.Render(picture, render_target);
      });
}

bool AiksPlayground::OpenPlaygroundHere(AiksPlaygroundCallback callback) {
  if (!Playground::is_enabled()) {
    return true;
  }

  AiksContext renderer(GetContext());

  if (!renderer.IsValid()) {
    return false;
  }

  return Playground::OpenPlaygroundHere(
      [&renderer, &callback](RenderTarget& render_target) -> bool {
        return callback(renderer, render_target);
      });
}

}  // namespace impeller
