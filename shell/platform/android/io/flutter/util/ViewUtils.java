// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.util;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import androidx.annotation.Nullable;

public final class ViewUtils {
  /**
   * Retrieves the {@link Activity} from a given {@link Context}.
   *
   * <p>This method will recursively traverse up the context chain if it is a {@link ContextWrapper}
   * until it finds the first instance of the base context that is an {@link Activity}.
   */
  @Nullable
  public static Activity getActivity(@Nullable Context context) {
    if (context == null) {
      return null;
    }
    if (context instanceof Activity) {
      return (Activity) context;
    }
    if (context instanceof ContextWrapper) {
      // Recurse up chain of base contexts until we find an Activity.
      return getActivity(((ContextWrapper) context).getBaseContext());
    }
    return null;
  }

  /**
   * Generates a view id.
   *
   * <p>In API level 17 and above, this ID is unique. Below 17, the fallback id is used instead.
   *
   * @param fallbackId the fallback id.
   * @return the view id.
   */
  public static int generateViewId(int fallbackId) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
      return View.generateViewId();
    }
    return fallbackId;
  }

  /**
   * Determines if the current view or any descendant view has focus.
   *
   * @param root The root view.
   * @return True if the current view or any descendant view has focus.
   */
  public static boolean childHasFocus(@Nullable View root) {
    if (root == null) {
      return false;
    }
    if (root.hasFocus()) {
      return true;
    }
    if (root instanceof ViewGroup) {
      final ViewGroup viewGroup = (ViewGroup) root;
      for (int idx = 0; idx < viewGroup.getChildCount(); idx++) {
        if (childHasFocus(viewGroup.getChildAt(idx))) {
          return true;
        }
      }
    }
    return false;
  }
}
