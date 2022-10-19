// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/compiler/switches.h"

#include <filesystem>
#include <map>

#include "flutter/fml/file.h"

namespace impeller {
namespace compiler {

static const std::map<std::string, TargetPlatform> kKnownPlatforms = {
    {"metal-desktop", TargetPlatform::kMetalDesktop},
    {"metal-ios", TargetPlatform::kMetalIOS},
    {"vulkan", TargetPlatform::kVulkan},
    {"opengl-es", TargetPlatform::kOpenGLES},
    {"opengl-desktop", TargetPlatform::kOpenGLDesktop},
    {"sksl", TargetPlatform::kSkSL},
    {"runtime-stage-metal", TargetPlatform::kRuntimeStageMetal},
    {"runtime-stage-gles", TargetPlatform::kRuntimeStageGLES},
};

static const std::map<std::string, SourceType> kKnownSourceTypes = {
    {"vert", SourceType::kVertexShader},
    {"frag", SourceType::kFragmentShader},
    {"tesc", SourceType::kTessellationControlShader},
    {"tese", SourceType::kTessellationEvaluationShader},
    {"comp", SourceType::kComputeShader},
};

void Switches::PrintHelp(std::ostream& stream) {
  stream << std::endl;
  stream << "ImpellerC is an offline shader processor and reflection engine."
         << std::endl;
  stream << "---------------------------------------------------------------"
         << std::endl;
  stream << "Valid Argument are:" << std::endl;
  stream << "One of [";
  for (const auto& platform : kKnownPlatforms) {
    stream << " --" << platform.first;
  }
  stream << " ]" << std::endl;
  stream << "--input=<glsl_file>" << std::endl;
  stream << "[optional] --input-kind={";
  for (const auto& source_type : kKnownSourceTypes) {
    stream << source_type.first << ", ";
  }
  stream << "}" << std::endl;
  stream << "--sl=<sl_output_file>" << std::endl;
  stream << "--spirv=<spirv_output_file>" << std::endl;
  stream << "[optional] --iplr (causes --sl file to be emitted in iplr format)"
         << std::endl;
  stream << "[optional] --reflection-json=<reflection_json_file>" << std::endl;
  stream << "[optional] --reflection-header=<reflection_header_file>"
         << std::endl;
  stream << "[optional] --reflection-cc=<reflection_cc_file>" << std::endl;
  stream << "[optional,multiple] --include=<include_directory>" << std::endl;
  stream << "[optional,multiple] --define=<define>" << std::endl;
  stream << "[optional] --depfile=<depfile_path>" << std::endl;
}

Switches::Switches() = default;

Switches::~Switches() = default;

static TargetPlatform TargetPlatformFromCommandLine(
    const fml::CommandLine& command_line) {
  auto target = TargetPlatform::kUnknown;
  for (const auto& platform : kKnownPlatforms) {
    if (command_line.HasOption(platform.first)) {
      // If the platform has already been determined, the caller may have
      // specified multiple platforms. This is an error and only one must be
      // selected.
      if (target != TargetPlatform::kUnknown) {
        return TargetPlatform::kUnknown;
      }
      target = platform.second;
      // Keep going to detect duplicates.
    }
  }
  return target;
}

static SourceType SourceTypeFromCommandLine(
    const fml::CommandLine& command_line) {
  auto source_type_option =
      command_line.GetOptionValueWithDefault("input-type", "");
  auto source_type_search = kKnownSourceTypes.find(source_type_option);
  if (source_type_search == kKnownSourceTypes.end()) {
    return SourceType::kUnknown;
  }
  return source_type_search->second;
}

Switches::Switches(const fml::CommandLine& command_line)
    : target_platform(TargetPlatformFromCommandLine(command_line)),
      working_directory(std::make_shared<fml::UniqueFD>(fml::OpenDirectory(
          ToUtf8(std::filesystem::current_path().native()).c_str(),
          false,  // create if necessary,
          fml::FilePermission::kRead))),
      source_file_name(command_line.GetOptionValueWithDefault("input", "")),
      input_type(SourceTypeFromCommandLine(command_line)),
      sl_file_name(command_line.GetOptionValueWithDefault("sl", "")),
      iplr(command_line.HasOption("iplr")),
      spirv_file_name(command_line.GetOptionValueWithDefault("spirv", "")),
      reflection_json_name(
          command_line.GetOptionValueWithDefault("reflection-json", "")),
      reflection_header_name(
          command_line.GetOptionValueWithDefault("reflection-header", "")),
      reflection_cc_name(
          command_line.GetOptionValueWithDefault("reflection-cc", "")),
      depfile_path(command_line.GetOptionValueWithDefault("depfile", "")) {
  if (!working_directory || !working_directory->is_valid()) {
    return;
  }

  for (const auto& include_dir_path : command_line.GetOptionValues("include")) {
    if (!include_dir_path.data()) {
      continue;
    }

    // fml::OpenDirectoryReadOnly for Windows doesn't handle relative paths
    // beginning with `../` well, so we build an absolute path.
    auto include_dir_absolute =
        ToUtf8(std::filesystem::absolute(std::filesystem::current_path() /
                                         include_dir_path)
                   .native());
    auto dir = std::make_shared<fml::UniqueFD>(fml::OpenDirectoryReadOnly(
        *working_directory, include_dir_absolute.c_str()));
    if (!dir || !dir->is_valid()) {
      continue;
    }

    IncludeDir dir_entry;
    dir_entry.name = include_dir_path;
    dir_entry.dir = std::move(dir);

    include_directories.emplace_back(std::move(dir_entry));
  }

  for (const auto& define : command_line.GetOptionValues("define")) {
    defines.emplace_back(define);
  }
}

bool Switches::AreValid(std::ostream& explain) const {
  bool valid = true;
  if (target_platform == TargetPlatform::kUnknown) {
    explain << "The target platform (only one) was not specified." << std::endl;
    valid = false;
  }

  if (!working_directory || !working_directory->is_valid()) {
    explain << "Could not figure out working directory." << std::endl;
    valid = false;
  }

  if (source_file_name.empty()) {
    explain << "Input file name was empty." << std::endl;
    valid = false;
  }

  if (sl_file_name.empty() && TargetPlatformNeedsSL(target_platform)) {
    explain << "Target shading language file name was empty." << std::endl;
    valid = false;
  }

  if (spirv_file_name.empty()) {
    explain << "Spirv file name was empty." << std::endl;
    valid = false;
  }
  return valid;
}

}  // namespace compiler
}  // namespace impeller
