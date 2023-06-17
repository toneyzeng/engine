// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../dom.dart';
import '../semantics.dart';
import '../util.dart';

/// Provides accessibility for dialogs.
///
/// See also [Role.dialog].
class Dialog extends RoleManager {
  Dialog(SemanticsObject semanticsObject) : super(Role.dialog, semanticsObject);

  @override
  void update() {
    // If semantic object corresponding to the dialog also provides the label
    // for itself it is applied as `aria-label`. See also [describeBy].
    if (semanticsObject.namesRoute) {
      final String? label = semanticsObject.label;
      assert(() {
        if (label == null || label.trim().isEmpty) {
          printWarning(
            'Semantic node ${semanticsObject.id} had both scopesRoute and '
            'namesRoute set, indicating a self-labelled dialog, but it is '
            'missing the label. A dialog should be labelled either by setting '
            'namesRoute on itself and providing a label, or by containing a '
            'child node with namesRoute that can describe it with its content.'
          );
        }
        return true;
      }());
      semanticsObject.element.setAttribute('aria-label', label ?? '');
      semanticsObject.setAriaRole('dialog', true);
    }
  }

  /// Sets the description of this dialog based on a [RouteName] descendant
  /// node, unless the dialog provides its own label.
  void describeBy(RouteName routeName) {
    if (semanticsObject.namesRoute) {
      // The dialog provides its own label, which takes precedence.
      return;
    }

    semanticsObject.setAriaRole('dialog', true);
    semanticsObject.element.setAttribute(
      'aria-describedby',
      routeName.semanticsObject.element.id,
    );
  }
}

/// Supplies a description for the nearest ancestor [Dialog].
class RouteName extends RoleManager {
  RouteName(SemanticsObject semanticsObject) : super(Role.routeName, semanticsObject);

  Dialog? _dialog;

  @override
  void update() {
    // NOTE(yjbanov): this does not handle the case when the node structure
    // changes such that this RouteName is no longer attached to the same
    // dialog. While this is technically expressible using the semantics API,
    // after discussing this case with customers I decided that this case is not
    // interesting enough to support. A tree restructure like this is likely to
    // confuse screen readers, and it would add complexity to the engine's
    // semantics code. Since reparenting can be done with no update to either
    // the Dialog or RouteName we'd have to scan intermediate nodes for
    // structural changes.
    if (semanticsObject.isLabelDirty) {
      final Dialog? dialog = _dialog;
      if (dialog != null) {
        // Already attached to a dialog, just update the description.
        dialog.describeBy(this);
      } else {
        // Setting the label for the first time. Wait for the DOM tree to be
        // established, then find the nearest dialog and update its label.
        semanticsObject.owner.addOneTimePostUpdateCallback(() {
          if (!isDisposed) {
            _lookUpNearestAncestorDialog();
            _dialog?.describeBy(this);
          }
        });
      }
    }
  }

  void _lookUpNearestAncestorDialog() {
    SemanticsObject? parent = semanticsObject.parent;
    while (parent != null && !parent.hasRole(Role.dialog)) {
      parent = parent.parent;
    }
    if (parent != null && parent.hasRole(Role.dialog)) {
      _dialog = parent.getRole<Dialog>(Role.dialog);
    }
  }
}
