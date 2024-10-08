// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:js_interop';

import 'package:js/js_util.dart' as js_util;
import 'package:test/bootstrap/browser.dart';
import 'package:test/test.dart';
import 'package:ui/src/engine.dart' as engine;
import 'package:ui/ui.dart' as ui;

@JS('_flutter')
external set _loader(JSAny? loader);
set loader(Object? l) => _loader = l?.toJSAnyShallow;

@JS('_flutter.loader.didCreateEngineInitializer')
external set didCreateEngineInitializer(Object? callback);

void main() {
  // Prepare _flutter.loader.didCreateEngineInitializer, so it's ready in the page ASAP.
  loader = js_util.jsify(<String, Object>{
    'loader': <String, Object>{
      'didCreateEngineInitializer': js_util.allowInterop(() { print('not mocked'); }),
    },
  });
  internalBootstrapBrowserTest(() => testMain);
}

void testMain() {
  test('webOnlyWarmupEngine calls _flutter.loader.didCreateEngineInitializer callback', () async {
    Object? engineInitializer;

    void didCreateEngineInitializerMock(Object? obj) {
      engineInitializer = obj;
    }

    // Prepare the DOM for: _flutter.loader.didCreateEngineInitializer
    didCreateEngineInitializer = js_util.allowInterop(didCreateEngineInitializerMock);

    // Reset the engine
    engine.debugResetEngineInitializationState();

    await ui.webOnlyWarmupEngine(
      registerPlugins: () {},
      runApp: () {},
    );

    // Check that the object we captured is actually a loader
    expect(engineInitializer, isNotNull);
    expect(js_util.hasProperty(engineInitializer!, 'initializeEngine'), isTrue, reason: 'Missing FlutterEngineInitializer method: initializeEngine.');
    expect(js_util.hasProperty(engineInitializer!, 'autoStart'), isTrue, reason: 'Missing FlutterEngineInitializer method: autoStart.');
  });

  test('webOnlyWarmupEngine does auto-start when _flutter.loader.didCreateEngineInitializer does not exist', () async {
    loader = null;

    bool pluginsRegistered = false;
    bool appRan = false;
    void registerPluginsMock() {
      pluginsRegistered = true;
    }
    void runAppMock() {
      appRan = true;
    }

    // Reset the engine
    engine.debugResetEngineInitializationState();

    await ui.webOnlyWarmupEngine(
      registerPlugins: registerPluginsMock,
      runApp: runAppMock,
    );

    // Check that the object we captured is actually a loader
    expect(pluginsRegistered, isTrue, reason: 'Plugins should be immediately registered in autoStart mode.');
    expect(appRan, isTrue, reason: 'App should run immediately in autoStart mode');
  });
  // We cannot test anymore, because by now the engine has registered some stuff that can't be rewound back.
  // Like the `ext.flutter.disassemble` developer extension.
}
