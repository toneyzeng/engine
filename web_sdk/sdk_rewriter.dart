// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';

import 'package:args/args.dart';
import 'package:path/path.dart' as path;

final ArgParser argParser = ArgParser()
  ..addOption('output-dir')
  ..addOption('input-dir')
  ..addFlag('ui')
  ..addOption('library-name')
  ..addOption('api-file')
  ..addMultiOption('source-file')
  ..addOption('stamp');

final List<Replacer> uiPatterns = <Replacer>[
  AllReplacer(RegExp(r'library\s+ui;'), 'library dart.ui;'),
  AllReplacer(RegExp(r'part\s+of\s+ui;'), 'part of dart.ui;'),
  AllReplacer(RegExp(r'''
import\s*'src/engine.dart'\s*as\s+engine;
'''), r'''
import 'dart:_engine' as engine;
'''),
  AllReplacer(RegExp(
    r'''
export\s*'src/engine.dart'
'''),
    r'''
export 'dart:_engine'
''',
  ),
];

List<Replacer> generateApiFilePatterns(String libraryName, String extraImports) {
  return <Replacer>[
    AllReplacer(RegExp('library\\s+$libraryName;'), '''
@JS()
library dart._$libraryName;

import 'dart:async';
import 'dart:collection';
import 'dart:convert' hide Codec;
import 'dart:developer' as developer;
import 'dart:js_util' as js_util;
import 'dart:_js_annotations';
import 'dart:math' as math;
import 'dart:typed_data';
import 'dart:ui' as ui;
$extraImports
'''
    ),
    // Replace exports of engine files with "part" directives.
    MappedReplacer(RegExp('''
export\\s*'$libraryName/(.*)';
'''), (Match match) => '''
part '$libraryName/${match.group(1)}';
'''
    ),
  ];
}

List<Replacer> generatePartsPatterns(String libraryName) {
  return <Replacer>[
    AllReplacer(RegExp('part\\s+of\\s+$libraryName;'), 'part of dart._$libraryName;'),
    // Remove library-level JS annotations.
    AllReplacer(RegExp(r'\n@JS(.*)\nlibrary .+;'), ''),
    // Remove library directives.
    AllReplacer(RegExp(r'\nlibrary .+;'), ''),
    // Remove imports/exports from all part files.
    AllReplacer(RegExp(r'\nimport\s*.*'), ''),
    AllReplacer(RegExp(r'\nexport\s*.*'), ''),
  ];
}

final List<Replacer> stripMetaPatterns = <Replacer>[
  AllReplacer(RegExp(r"import\s*'package:meta/meta.dart';"), ''),
  AllReplacer('@required', ''),
  AllReplacer('@protected', ''),
  AllReplacer('@mustCallSuper', ''),
  AllReplacer('@immutable', ''),
  AllReplacer('@visibleForTesting', ''),
];

// Rewrites the "package"-style web ui library into a dart:ui implementation.
// So far this only requires a replace of the library declarations.
void main(List<String> arguments) {
  final ArgResults results = argParser.parse(arguments);
  final Directory directory = Directory(results['output-dir'] as String);
  final String inputDirectoryPath = results['input-dir'] as String;

  String Function(String source)? preprocessor;
  List<Replacer> replacementPatterns;
  String? libraryName;
  if (results['ui'] as bool) {
    replacementPatterns = uiPatterns;
  } else {
    libraryName = results['library-name'] as String;
    if (libraryName == null) {
      throw Exception('library-name must be specified if not rewriting ui');
    }
    preprocessor = (String source) => preprocessPartFile(source, libraryName!);
    replacementPatterns = generatePartsPatterns(libraryName);
  }
  for (final String inputFilePath in results['source-file'] as Iterable<String>) {
    String pathSuffix = inputFilePath.substring(inputDirectoryPath.length);
    if (libraryName != null) {
      pathSuffix = path.join(libraryName, pathSuffix);
    }
    final String outputFilePath = path.join(directory.path, pathSuffix);
    processFile(inputFilePath, outputFilePath, preprocessor, replacementPatterns);
  }

  if (results['api-file'] != null) {
    if (libraryName == null) {
      throw Exception('library-name must be specified if api-file is specified');
    }

    final String inputFilePath = results['api-file'] as String;
    final String outputFilePath = path.join(
        directory.path, path.basename(inputFilePath));
    String extraImports = '';
    if (libraryName == 'engine') {
      extraImports = "import 'dart:_skwasm_stub' if (dart.library.ffi) 'dart:_skwasm_impl';\n";
    } else if (libraryName == 'skwasm_stub' || libraryName == 'skwasm_impl') {
      extraImports = "import 'dart:_engine';\n";
    }
    replacementPatterns = generateApiFilePatterns(libraryName, extraImports);

    processFile(
      inputFilePath,
      outputFilePath,
      (String source) => validateApiFile(inputFilePath, source, libraryName!),
      replacementPatterns
    );
  }


  if (results['stamp'] != null) {
    File(results['stamp'] as String).writeAsStringSync('stamp');
  }
}

void processFile(String inputFilePath, String outputFilePath, String Function(String source)? preprocessor, List<Replacer> replacementPatterns) {
  final File inputFile = File(inputFilePath);
  final File outputFile = File(outputFilePath)
    ..createSync(recursive: true);
  outputFile.writeAsStringSync(processSource(
    inputFile.readAsStringSync(),
    preprocessor,
    replacementPatterns));
}

String processSource(String source, String Function(String source)? preprocessor, List<Replacer> replacementPatterns) {
  if (preprocessor != null) {
    source = preprocessor(source);
  }
  for (final Replacer replacer in stripMetaPatterns) {
    source = replacer.perform(source);
  }
  for (final Replacer replacer in replacementPatterns) {
    source = replacer.perform(source);
  }
  return source;
}

// Enforces a particular structure in top level api files for sublibraries.
//
// Code in api files must only be made of the library directive, exports,
// and code comments. Imports are disallowed. Instead, the required imports are
// added by this script during the rewrite.
String validateApiFile(String apiFilePath, String apiFileCode, String libraryName) {
  final List<String> expectedLines = <String>[
    'library $libraryName;',
  ];

  final List<String> lines = apiFileCode.split('\n');
  for (int i = 0; i < lines.length; i += 1) {
    final int lineNumber = i + 1;
    final String line = lines[i].trim();

    if (line.isEmpty) {
      // Emply lines are OK
      continue;
    }

    if (expectedLines.contains(line)) {
      // Expected; let it pass.
      continue;
    }

    if (line.startsWith('//')) {
      // Comments are OK
      continue;
    }

    if (line.startsWith('export')) {
      // Exports are OK
      continue;
    }

    throw Exception(
      'on line $lineNumber: unexpected code in $apiFilePath. This file '
      'may only contain comments and exports. Found:\n'
      '$line'
    );
  }
  return apiFileCode;
}

String preprocessPartFile(String source, String libraryName) {
  return 'part of $libraryName;\n$source';
}

/// Responsible for performing string replacements.
abstract class Replacer {
  /// Performs the replacement in the provided [text].
  String perform(String text);
}

/// Replaces all occurrences of a pattern with a fixed string.
class AllReplacer implements Replacer {
  /// Creates a new tuple with the given [pattern] and [replacement] string.
  AllReplacer(this._pattern, this._replacement);

  /// The pattern to be replaced.
  final Pattern _pattern;

  /// The replacement string.
  final String _replacement;

  @override
  String perform(String text) {
    return text.replaceAll(_pattern, _replacement);
  }
}

/// Uses a callback to replace each occurrence of a pattern.
class MappedReplacer implements Replacer {
  MappedReplacer(this._pattern, this._replace);

  /// The pattern to be replaced.
  final RegExp _pattern;

  /// A callback to replace each occurrence of [_pattern].
  final String Function(Match match) _replace;

  @override
  String perform(String text) {
    return text.replaceAllMapped(_pattern, _replace);
  }
}
