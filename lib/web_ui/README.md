# Flutter Web Engine

This directory contains the source code for the Web Engine.

## Hacking on the Web Engine

If you are setting up a workspace for the first time, start by following the
instructions at [Setting up the Engine development environment][1]. In addition,
it is useful to add the following to your `PATH` environment variable:

- `ENGINE_ROOT/src/flutter/lib/web_ui/dev`, so you can run the `felt` command
  from anywhere.
- `FLUTTER_ROOT/bin`, so you can run `dart` and `flutter` commands from
  anywhere.

### Using `felt`

`felt` (stands for "Flutter Engine Local Tester") is a command-line tool that
aims to make development in the Flutter web engine more productive and pleasant.

To tell `felt` to do anything you call `felt SUBCOMMAND`, where `SUBCOMMAND` is
one of the available subcommands, which can be listed by running `felt help`. To
get help for a specific subcommand, run `felt help SUBCOMMAND`.

The most useful subcommands are:

- `felt build` - builds a local Flutter Web engine ready to be used by the
  Flutter framework. To use the local engine build, pass
  `--local-engine=host_debug_unopt` to the `flutter` command, or to
  `dev/bots/test.dart` when running a web shard, such as `web_tests`.
- `felt test` - runs web engine tests. By default, this runs all tests using
  Chromium. Passing one or more paths to specific tests would run just the
  specified tests. Run `felt help test` for more options.

`build` and `test` take the `--watch` option, which automatically reruns the
subcommand when a source file changes. This is handy when you are iterating
quickly.

#### Examples

Builds the web engine, the runs a Flutter app using it:

```
felt build
cd path/to/some/app
flutter --local-engine=host_debug_unopt run -d chrome
```

Runs all tests in Chromium:

```
felt test
```

Runs a specific test:

```
felt test test/engine/util_test.dart
```

Runs multiple specific tests:

```
felt test test/engine/util_test.dart test/alarm_clock_test.dart
```

Enable watch mode so that the test re-runs every time a source file changes:

```
felt test --watch test/engine/util_test.dart
```

Runs tests in Firefox (requires a Linux computer):

```
felt test --browser=firefox
```

Chromium and Firefox support debugging tests using the browser's developer
tools. To run tests in debug mode add `--debug` to the `test` command, e.g.:

```
felt test --debug --browser=firefox test/alarm_clock_test.dart
```

### Optimizing local builds

Concurrency of various build steps can be configured via environment variables:

- `FELT_DART2JS_CONCURRENCY` specifies the number of concurrent `dart2js`
  processes used to compile tests. Default value is 8.
- `FELT_TEST_CONCURRENCY` specifies the number of tests run concurrently.
  Default value is 10.

If you are a Google employee, you can use an internal instance of Goma (go/ma)
to parallelize your ninja builds. Because Goma compiles code on remote servers,
this option is particularly effective for building on low-powered laptops.

### Test browsers

Chromium, Firefox, and Safari for iOS are version-locked using the
[browser_lock.yaml][2] configuration file. Safari for macOS is supplied by the
computer's operating system. Tests can be run in Edge locally, but Edge is not
enabled on LUCI. Chromium is used as a proxy for Chrome, Edge, and other
Chromium-based browsers.

Changing parameters in the browser lock is effective immediately when running
tests locally. To make changes effective on LUCI follow instructions in
[Rolling Browsers][#rolling-browsers].

#### Local testing in Safari using the iOS Simulator

1. If you haven't already, install Xcode.
2. The iOS version and device type used by web engine tests are specified in
   the [browser_lock.yaml][2] file. Install the iOS Simulator version using:
   Xcode > Preferences > Components
3. Run `xcrun simctl list devices`. If the simulator you want is not installed
   use step 4.
4. Use felt to create a simulator:

```
felt create_simulator
```

To run tests on ios-safari use the one of the following commands:

```
felt test --browser=ios-safari
felt test --browser=ios-safari test/alarm_clock_test.dart
```

### Rolling browsers

When running tests on LUCI using Chromium, LUCI uses the version of Chromium
fetched from CIPD.

Since the engine code and infra recipes do not live in the same repository
there are few steps to follow in order to upgrade a browser's version.

#### Chromium

Chromium is an independent project that gets rolled into Flutter manually, and as needed.
Flutter consumes a pre-built Chromium version from chromium.org. When a new version of
Chromium (check [here](https://www.chromium.org/getting-involved/download-chromium/#downloading-old-builds-of-chrome-chromium))
is needed, follow these steps to roll the new version:

- Make sure you have `depot_tools` installed (if you are regularly hacking on
  the engine code, you probably do).
- If not already authenticated with CIPD, run `cipd auth-login` and follow
  instructions (this step requires sufficient privileges; contact
  #hackers-infra-🌡 on [Flutter's Discord server](https://github.com/flutter/flutter/wiki/Chat)).
- Edit `dev/browser_lock.yaml` and update the following values under `chrome`:
  - Set `Windows`, `Mac` and `Linux` to the `branch_base_position`s given [in this table](https://omahaproxy.appspot.com).
  (Pick from `linux`, `mac` and `win` as `os`, and the `stable` channel.)
  - Set `version` to a string composed of the Major Version of the browser, and
  the number of times that major version has been uploaded to CIPD. For example,
  start with `'99'` for version 99.0.4844.51 of Chromium, and update to `'99.1'`,
  `'99.2'` and so on if you need to upload newer bundles of the same major version.
  (This is required because tags can't be repeated in CIPD).
- Run `dart dev/browser_roller.dart` and make sure it completes successfully.
  The script uploads the specified versions of Chromium (and Chromedriver) to the
  right locations in CIPD: [Chrome](https://chrome-infra-packages.appspot.com/p/flutter_internal/browsers/chrome),
  [Chromedriver](https://chrome-infra-packages.appspot.com/p/flutter_internal/browser-drivers/chrome).
- Send a pull request containing the above file changes. Newer versions of Chromium
  might break some tests or Goldens. Get those fixed too!

If you have questions, contact the Flutter Web team on Flutter Discord on the
\#hackers-web-🌍 channel.

##### **browser_roller.dart**

The script has the following command-line options:

- `--dry-run` - The script will stop before uploading artifacts to CIPD. The location of the data will be reported at the end of the script, if the script finishes successfullyThe output of the script will be visible in /tmp/browser-roll-RANDOM_STRING
- `--verbose` - Greatly increase the amount of information printed to `stdout` by the script.

> Try the following!
>
> ```bash
> dart ./dev/browser_roller.dart --dry-run --verbose
> ```

#### **Other browsers / manual upload**

In general, the manual process goes like this:

1. Dowload the binaries for the new browser/driver for each operaing system
   (macOS, linux, windows).
2. Create CIPD packages for these packages (more documentation is available for
   Googlers at go/cipd-flutter-web)
3. Update the version in this repo. Do this by changing the related fields in
   `browser_lock.yaml` file.

Resources:

1. Browser and driver CIPD [packages][4] (requires special access; ping
   hackers-infra on Discord for more information)
2. LUCI web [recipe][5]
3. More general reading on CIPD packages [link][6]

### Rolling CanvasKit

CanvasKit is versioned separately from Skia and rolled manually. Flutter
consumes a pre-built CanvasKit provided by the Skia team, currently hosted on
unpkg.com. When a new version of CanvasKit is available (check
https://www.npmjs.com/package/canvaskit-wasm or consult the Skia team
directly), follow these steps to roll to the new version:

- Make sure you have `depot_tools` installed (if you are regularly hacking on
  the engine code, you probably do).
- If not already authenticated with CIPD, run `cipd auth-login` and follow
  instructions (this step requires sufficient privileges; contact
  #hackers-infra-🌡 on Flutter's Discord server).
- Edit `dev/canvaskit_lock.yaml` and update the value of `canvaskit_version`
  to the new version.
- Run `dart dev/canvaskit_roller.dart` and make sure it completes successfully.
  The script uploads the new version of CanvasKit to the
  `flutter/web/canvaskit_bundle` CIPD package, and writes the CIPD package
  instance ID to the DEPS file.
- Send a pull request containing the above file changes. If the new version
  contains breaking changes, the PR must also contain corresponding fixes.

If you have questions, contact the Flutter Web team on Flutter Discord on the
#hackers-web-🌍 channel.

### Configuration files

`browser_lock.yaml` contains the version of browsers we use to test Flutter for
web. Versions are not automatically updated whenever a new release is available.
Instead, we update this file manually once in a while.

`canvaskit_lock.yaml` locks the version of CanvasKit for tests and production
use.

## Troubleshooting

### Can't load Kernel binary: Invalid kernel binary format version.

Sometimes `.dart_tool` cache invalidation fails, and you'll end up with a
cached version of `felt` that is not compatible with the Dart SDK that you're
using.

In that case, any invocation to `felt` will fail with:

```
Can't load Kernel binary: Invalid kernel binary format version.
```

The solution is to delete the cached `felt.snapshot` files under `lib/web_ui`:

```
rm .dart_tool/felt.snapshot*
```

## Hacking on the `felt` tool itself

If you are making changes in the `felt` tool itself, you need to be aware of
Dart snapshots. We create a Dart snapshot of the `felt` tool to make the startup
faster.

To run `felt` from sources, disable the snapshot using the `FELT_USE_SNAPSHOT`
environment variable:

```
FELT_USE_SNAPSHOT=false felt <command>
```

## Building CanvasKit

To build CanvasKit locally, you must first set up your gclient config to
activate the Emscripten SDK, which is the toolchain used to build CanvasKit.
To do this, replace the contents of your .gclient file at the root of the
project (i.e. in the parent directory of the `src` directory) with:

```
solutions = [
  {
    "managed": False,
    "name": "src/flutter",
    "url": "git@github.com:<your_username_here>/engine.git",
    "custom_deps": {},
    "deps_file": "DEPS",
    "safesync_url": "",
    "custom_vars": {
      "download_emsdk": True,
    },
  },
]
```

Now run `gclient sync` and it should pull in the Emscripten SDK and activate it.

To build CanvasKit with `felt`, run:

```
felt build --build-canvaskit
```

This will build CanvasKit in `out/wasm_debug`. If you now run

```
felt test
```

it will detect that you have built CanvasKit and use that instead of the one
from CIPD to run the tests against.

### Upgrading the Emscripten SDK for the CanvasKit build

The version of the Emscripten SDK should be kept up to date with the version
used in the Skia build. That version can be found in
`third_party/skia/bin/activate-emsdk`. It will probably also be necessary to
roll the dependency on `third_party/emsdk` in DEPS to the same version as in
`third_party/skia/DEPS`.

Once you know the version for the Emscripten SDK, change the line in
`tools/activate_emsdk.py` which defines `EMSDK_VERSION` to match Skia.



[1]: https://github.com/flutter/flutter/wiki/Setting-up-the-Engine-development-environment
[2]: https://github.com/flutter/engine/blob/main/lib/web_ui/dev/browser_lock.yaml
[4]: https://chrome-infra-packages.appspot.com/p/flutter_internal
[5]: https://cs.opensource.google/flutter/recipes/+/master:recipes/engine/web_engine.py
[6]: https://chromium.googlesource.com/chromium/src.git/+/main/docs/cipd_and_3pp.md#What-is-CIPD
