// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/compiler/compiler.h"

#include <array>
#include <filesystem>
#include <memory>
#include <sstream>
#include <utility>

#include "flutter/fml/paths.h"
#include "impeller/base/allocation.h"
#include "impeller/compiler/compiler_backend.h"
#include "impeller/compiler/includer.h"
#include "impeller/compiler/logger.h"
#include "impeller/compiler/types.h"

namespace impeller {
namespace compiler {

const uint32_t kFragBindingBase = 128;
const size_t kNumUniformKinds =
    static_cast<int>(shaderc_uniform_kind::shaderc_uniform_kind_buffer) + 1;

static CompilerBackend CreateMSLCompiler(const spirv_cross::ParsedIR& ir,
                                         const SourceOptions& source_options) {
  auto sl_compiler = std::make_shared<spirv_cross::CompilerMSL>(ir);
  spirv_cross::CompilerMSL::Options sl_options;
  sl_options.platform =
      TargetPlatformToMSLPlatform(source_options.target_platform);
  // If this version specification changes, the GN rules that process the
  // Metal to AIR must be updated as well.
  sl_options.msl_version =
      spirv_cross::CompilerMSL::Options::make_msl_version(1, 2);
  sl_compiler->set_msl_options(sl_options);
  return CompilerBackend(sl_compiler);
}

static CompilerBackend CreateGLSLCompiler(const spirv_cross::ParsedIR& ir,
                                          const SourceOptions& source_options) {
  auto gl_compiler = std::make_shared<spirv_cross::CompilerGLSL>(ir);
  spirv_cross::CompilerGLSL::Options sl_options;
  sl_options.force_zero_initialized_variables = true;
  sl_options.vertex.fixup_clipspace = true;
  if (source_options.target_platform == TargetPlatform::kOpenGLES) {
    sl_options.version = 100;
    sl_options.es = true;
  } else {
    sl_options.version = 120;
    sl_options.es = false;
  }
  gl_compiler->set_common_options(sl_options);
  return CompilerBackend(gl_compiler);
}

static CompilerBackend CreateSkSLCompiler(const spirv_cross::ParsedIR& ir,
                                          const SourceOptions& source_options) {
  auto sksl_compiler = std::make_shared<CompilerSkSL>(ir);
  return CompilerBackend(sksl_compiler);
}

static bool EntryPointMustBeNamedMain(TargetPlatform platform) {
  switch (platform) {
    case TargetPlatform::kUnknown:
      FML_UNREACHABLE();
    case TargetPlatform::kMetalDesktop:
    case TargetPlatform::kMetalIOS:
    case TargetPlatform::kVulkan:
    case TargetPlatform::kRuntimeStageMetal:
      return false;
    case TargetPlatform::kSkSL:
    case TargetPlatform::kOpenGLES:
    case TargetPlatform::kOpenGLDesktop:
    case TargetPlatform::kRuntimeStageGLES:
      return true;
  }
  FML_UNREACHABLE();
}

static CompilerBackend CreateCompiler(const spirv_cross::ParsedIR& ir,
                                      const SourceOptions& source_options) {
  CompilerBackend compiler;
  switch (source_options.target_platform) {
    case TargetPlatform::kMetalDesktop:
    case TargetPlatform::kMetalIOS:
    case TargetPlatform::kRuntimeStageMetal:
    case TargetPlatform::kRuntimeStageGLES:
    case TargetPlatform::kVulkan:
      compiler = CreateMSLCompiler(ir, source_options);
      break;
    case TargetPlatform::kUnknown:
    case TargetPlatform::kOpenGLES:
    case TargetPlatform::kOpenGLDesktop:
      compiler = CreateGLSLCompiler(ir, source_options);
      break;
    case TargetPlatform::kSkSL:
      compiler = CreateSkSLCompiler(ir, source_options);
  }
  if (!compiler) {
    return {};
  }
  auto* backend = compiler.GetCompiler();
  if (!EntryPointMustBeNamedMain(source_options.target_platform)) {
    backend->rename_entry_point("main", source_options.entry_point_name,
                                ToExecutionModel(source_options.type));
  }
  return compiler;
}

static void SetLimitations(shaderc::CompileOptions& compiler_opts) {
  using Limit = std::pair<shaderc_limit, int>;
  static constexpr std::array<Limit, 83> limits = {
      Limit{shaderc_limit::shaderc_limit_max_lights, 8},
      Limit{shaderc_limit::shaderc_limit_max_clip_planes, 6},
      Limit{shaderc_limit::shaderc_limit_max_texture_units, 2},
      Limit{shaderc_limit::shaderc_limit_max_texture_coords, 8},
      Limit{shaderc_limit::shaderc_limit_max_vertex_attribs, 16},
      Limit{shaderc_limit::shaderc_limit_max_vertex_uniform_components, 4096},
      Limit{shaderc_limit::shaderc_limit_max_varying_floats, 60},
      Limit{shaderc_limit::shaderc_limit_max_vertex_texture_image_units, 16},
      Limit{shaderc_limit::shaderc_limit_max_combined_texture_image_units, 80},
      Limit{shaderc_limit::shaderc_limit_max_texture_image_units, 16},
      Limit{shaderc_limit::shaderc_limit_max_fragment_uniform_components, 1024},
      Limit{shaderc_limit::shaderc_limit_max_draw_buffers, 8},
      Limit{shaderc_limit::shaderc_limit_max_vertex_uniform_vectors, 256},
      Limit{shaderc_limit::shaderc_limit_max_varying_vectors, 15},
      Limit{shaderc_limit::shaderc_limit_max_fragment_uniform_vectors, 256},
      Limit{shaderc_limit::shaderc_limit_max_vertex_output_vectors, 16},
      Limit{shaderc_limit::shaderc_limit_max_fragment_input_vectors, 15},
      Limit{shaderc_limit::shaderc_limit_min_program_texel_offset, -8},
      Limit{shaderc_limit::shaderc_limit_max_program_texel_offset, 7},
      Limit{shaderc_limit::shaderc_limit_max_clip_distances, 8},
      Limit{shaderc_limit::shaderc_limit_max_compute_work_group_count_x, 65535},
      Limit{shaderc_limit::shaderc_limit_max_compute_work_group_count_y, 65535},
      Limit{shaderc_limit::shaderc_limit_max_compute_work_group_count_z, 65535},
      Limit{shaderc_limit::shaderc_limit_max_compute_work_group_size_x, 1024},
      Limit{shaderc_limit::shaderc_limit_max_compute_work_group_size_y, 1024},
      Limit{shaderc_limit::shaderc_limit_max_compute_work_group_size_z, 64},
      Limit{shaderc_limit::shaderc_limit_max_compute_uniform_components, 512},
      Limit{shaderc_limit::shaderc_limit_max_compute_texture_image_units, 16},
      Limit{shaderc_limit::shaderc_limit_max_compute_image_uniforms, 8},
      Limit{shaderc_limit::shaderc_limit_max_compute_atomic_counters, 8},
      Limit{shaderc_limit::shaderc_limit_max_compute_atomic_counter_buffers, 1},
      Limit{shaderc_limit::shaderc_limit_max_varying_components, 60},
      Limit{shaderc_limit::shaderc_limit_max_vertex_output_components, 64},
      Limit{shaderc_limit::shaderc_limit_max_geometry_input_components, 64},
      Limit{shaderc_limit::shaderc_limit_max_geometry_output_components, 128},
      Limit{shaderc_limit::shaderc_limit_max_fragment_input_components, 128},
      Limit{shaderc_limit::shaderc_limit_max_image_units, 8},
      Limit{shaderc_limit::
                shaderc_limit_max_combined_image_units_and_fragment_outputs,
            8},
      Limit{shaderc_limit::shaderc_limit_max_combined_shader_output_resources,
            8},
      Limit{shaderc_limit::shaderc_limit_max_image_samples, 0},
      Limit{shaderc_limit::shaderc_limit_max_vertex_image_uniforms, 0},
      Limit{shaderc_limit::shaderc_limit_max_tess_control_image_uniforms, 0},
      Limit{shaderc_limit::shaderc_limit_max_tess_evaluation_image_uniforms, 0},
      Limit{shaderc_limit::shaderc_limit_max_geometry_image_uniforms, 0},
      Limit{shaderc_limit::shaderc_limit_max_fragment_image_uniforms, 8},
      Limit{shaderc_limit::shaderc_limit_max_combined_image_uniforms, 8},
      Limit{shaderc_limit::shaderc_limit_max_geometry_texture_image_units, 16},
      Limit{shaderc_limit::shaderc_limit_max_geometry_output_vertices, 256},
      Limit{shaderc_limit::shaderc_limit_max_geometry_total_output_components,
            1024},
      Limit{shaderc_limit::shaderc_limit_max_geometry_uniform_components, 512},
      Limit{shaderc_limit::shaderc_limit_max_geometry_varying_components, 60},
      Limit{shaderc_limit::shaderc_limit_max_tess_control_input_components,
            128},
      Limit{shaderc_limit::shaderc_limit_max_tess_control_output_components,
            128},
      Limit{shaderc_limit::shaderc_limit_max_tess_control_texture_image_units,
            16},
      Limit{shaderc_limit::shaderc_limit_max_tess_control_uniform_components,
            1024},
      Limit{
          shaderc_limit::shaderc_limit_max_tess_control_total_output_components,
          4096},
      Limit{shaderc_limit::shaderc_limit_max_tess_evaluation_input_components,
            128},
      Limit{shaderc_limit::shaderc_limit_max_tess_evaluation_output_components,
            128},
      Limit{
          shaderc_limit::shaderc_limit_max_tess_evaluation_texture_image_units,
          16},
      Limit{shaderc_limit::shaderc_limit_max_tess_evaluation_uniform_components,
            1024},
      Limit{shaderc_limit::shaderc_limit_max_tess_patch_components, 120},
      Limit{shaderc_limit::shaderc_limit_max_patch_vertices, 32},
      Limit{shaderc_limit::shaderc_limit_max_tess_gen_level, 64},
      Limit{shaderc_limit::shaderc_limit_max_viewports, 16},
      Limit{shaderc_limit::shaderc_limit_max_vertex_atomic_counters, 0},
      Limit{shaderc_limit::shaderc_limit_max_tess_control_atomic_counters, 0},
      Limit{shaderc_limit::shaderc_limit_max_tess_evaluation_atomic_counters,
            0},
      Limit{shaderc_limit::shaderc_limit_max_geometry_atomic_counters, 0},
      Limit{shaderc_limit::shaderc_limit_max_fragment_atomic_counters, 8},
      Limit{shaderc_limit::shaderc_limit_max_combined_atomic_counters, 8},
      Limit{shaderc_limit::shaderc_limit_max_atomic_counter_bindings, 1},
      Limit{shaderc_limit::shaderc_limit_max_vertex_atomic_counter_buffers, 0},
      Limit{
          shaderc_limit::shaderc_limit_max_tess_control_atomic_counter_buffers,
          0},
      Limit{shaderc_limit::
                shaderc_limit_max_tess_evaluation_atomic_counter_buffers,
            0},
      Limit{shaderc_limit::shaderc_limit_max_geometry_atomic_counter_buffers,
            0},
      Limit{shaderc_limit::shaderc_limit_max_fragment_atomic_counter_buffers,
            0},
      Limit{shaderc_limit::shaderc_limit_max_combined_atomic_counter_buffers,
            1},
      Limit{shaderc_limit::shaderc_limit_max_atomic_counter_buffer_size, 32},
      Limit{shaderc_limit::shaderc_limit_max_transform_feedback_buffers, 4},
      Limit{shaderc_limit::
                shaderc_limit_max_transform_feedback_interleaved_components,
            64},
      Limit{shaderc_limit::shaderc_limit_max_cull_distances, 8},
      Limit{shaderc_limit::shaderc_limit_max_combined_clip_and_cull_distances,
            8},
      Limit{shaderc_limit::shaderc_limit_max_samples, 4},
  };
  for (auto& [limit, value] : limits) {
    compiler_opts.SetLimit(limit, value);
  }
}

void Compiler::SetBindingBase(shaderc::CompileOptions& compiler_opts) const {
  for (size_t uniform_kind = 0; uniform_kind < kNumUniformKinds;
       uniform_kind++) {
    compiler_opts.SetBindingBaseForStage(
        ToShaderCShaderKind(SourceType::kFragmentShader),
        static_cast<shaderc_uniform_kind>(uniform_kind), kFragBindingBase);
  }
}

Compiler::Compiler(const fml::Mapping& source_mapping,
                   const SourceOptions& source_options,
                   Reflector::Options reflector_options)
    : options_(source_options) {
  if (source_mapping.GetMapping() == nullptr) {
    COMPILER_ERROR
        << "Could not read shader source or shader source was empty.";
    return;
  }

  if (source_options.target_platform == TargetPlatform::kUnknown) {
    COMPILER_ERROR << "Target platform not specified.";
    return;
  }

  auto shader_kind = ToShaderCShaderKind(source_options.type);

  if (shader_kind == shaderc_shader_kind::shaderc_glsl_infer_from_source) {
    COMPILER_ERROR << "Could not figure out shader stage.";
    return;
  }

  shaderc::CompileOptions spirv_options;

  // Make sure reflection is as effective as possible. The generated shaders
  // will be processed later by backend specific compilers. So optimizations
  // here are irrelevant and get in the way of generating reflection code.
  spirv_options.SetGenerateDebugInfo();

  // Expects GLSL 4.60 (Core Profile).
  // https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.60.pdf
  spirv_options.SetSourceLanguage(
      shaderc_source_language::shaderc_source_language_glsl);
  spirv_options.SetForcedVersionProfile(460,
                                        shaderc_profile::shaderc_profile_core);
  SetLimitations(spirv_options);

  switch (source_options.target_platform) {
    case TargetPlatform::kMetalDesktop:
    case TargetPlatform::kMetalIOS:
    case TargetPlatform::kOpenGLES:
    case TargetPlatform::kOpenGLDesktop:
      spirv_options.SetOptimizationLevel(
          shaderc_optimization_level::shaderc_optimization_level_performance);
      spirv_options.SetTargetEnvironment(
          shaderc_target_env::shaderc_target_env_vulkan,
          shaderc_env_version::shaderc_env_version_vulkan_1_1);
      spirv_options.SetTargetSpirv(
          shaderc_spirv_version::shaderc_spirv_version_1_3);
      break;
    case TargetPlatform::kVulkan:
      spirv_options.SetOptimizationLevel(
          shaderc_optimization_level::shaderc_optimization_level_performance);
      spirv_options.SetTargetEnvironment(
          shaderc_target_env::shaderc_target_env_vulkan,
          shaderc_env_version::shaderc_env_version_vulkan_1_0);
      spirv_options.SetTargetSpirv(
          shaderc_spirv_version::shaderc_spirv_version_1_0);
      break;
    case TargetPlatform::kRuntimeStageMetal:
    case TargetPlatform::kRuntimeStageGLES:
      spirv_options.SetOptimizationLevel(
          shaderc_optimization_level::shaderc_optimization_level_performance);
      spirv_options.SetTargetEnvironment(
          shaderc_target_env::shaderc_target_env_opengl,
          shaderc_env_version::shaderc_env_version_opengl_4_5);
      spirv_options.SetTargetSpirv(
          shaderc_spirv_version::shaderc_spirv_version_1_0);
      break;
    case TargetPlatform::kSkSL:
      // When any optimization level above 'zero' is enabled, the phi merges at
      // loop continue blocks are rendered using syntax that is supported in
      // GLSL, but not in SkSL.
      // https://bugs.chromium.org/p/skia/issues/detail?id=13518.
      spirv_options.SetOptimizationLevel(
          shaderc_optimization_level::shaderc_optimization_level_zero);
      spirv_options.SetTargetEnvironment(
          shaderc_target_env::shaderc_target_env_opengl,
          shaderc_env_version::shaderc_env_version_opengl_4_5);
      spirv_options.SetTargetSpirv(
          shaderc_spirv_version::shaderc_spirv_version_1_0);
      break;
    case TargetPlatform::kUnknown:
      COMPILER_ERROR << "Target platform invalid.";
      return;
  }

  // Implicit definition that indicates that this compilation is for the device
  // (instead of the host).
  spirv_options.AddMacroDefinition("IMPELLER_DEVICE");
  for (const auto& define : source_options.defines) {
    spirv_options.AddMacroDefinition(define);
  }

  spirv_options.SetAutoBindUniforms(true);
#ifdef IMPELLER_ENABLE_VULKAN
  SetBindingBase(spirv_options);
#endif
  spirv_options.SetAutoMapLocations(true);

  std::vector<std::string> included_file_names;
  spirv_options.SetIncluder(std::make_unique<Includer>(
      options_.working_directory, options_.include_dirs,
      [&included_file_names](auto included_name) {
        included_file_names.emplace_back(std::move(included_name));
      }));

  shaderc::Compiler spv_compiler;
  if (!spv_compiler.IsValid()) {
    COMPILER_ERROR << "Could not initialize the GLSL to SPIRV compiler.";
    return;
  }

  // SPIRV Generation.
  spv_result_ = std::make_shared<shaderc::SpvCompilationResult>(
      spv_compiler.CompileGlslToSpv(
          reinterpret_cast<const char*>(
              source_mapping.GetMapping()),         // source_text
          source_mapping.GetSize(),                 // source_text_size
          shader_kind,                              // shader_kind
          source_options.file_name.c_str(),         // input_file_name
          source_options.entry_point_name.c_str(),  // entry_point_name
          spirv_options                             // options
          ));
  if (spv_result_->GetCompilationStatus() !=
      shaderc_compilation_status::shaderc_compilation_status_success) {
    COMPILER_ERROR << "GLSL to SPIRV failed; "
                   << ShaderCErrorToString(spv_result_->GetCompilationStatus())
                   << ". " << spv_result_->GetNumErrors() << " error(s) and "
                   << spv_result_->GetNumWarnings() << " warning(s).";
    if (spv_result_->GetNumErrors() > 0 || spv_result_->GetNumWarnings() > 0) {
      COMPILER_ERROR_NO_PREFIX << spv_result_->GetErrorMessage();
    }
    return;
  } else {
    included_file_names_ = std::move(included_file_names);
  }

  if (!TargetPlatformNeedsSL(source_options.target_platform)) {
    is_valid_ = true;
    return;
  }

  // SL Generation.
  spirv_cross::Parser parser(spv_result_->cbegin(),
                             spv_result_->cend() - spv_result_->cbegin());
  // The parser and compiler must be run separately because the parser contains
  // meta information (like type member names) that are useful for reflection.
  parser.parse();

  const auto parsed_ir =
      std::make_shared<spirv_cross::ParsedIR>(parser.get_parsed_ir());

  auto sl_compiler = CreateCompiler(*parsed_ir, options_);

  if (!sl_compiler) {
    COMPILER_ERROR << "Could not create compiler for target platform.";
    return;
  }

  // We need to invoke the compiler even if we don't use the SL mapping later
  // for Vulkan. The reflector needs information that is only valid after a
  // successful compilation call.
  auto sl_compilation_result =
      CreateMappingWithString(sl_compiler.GetCompiler()->compile());

  // If the target is Vulkan, our shading language is SPIRV which we already
  // have. If it isn't, we need to invoke the appropriate compiler to compile
  // the SPIRV to the target SL.
  sl_mapping_ = source_options.target_platform == TargetPlatform::kVulkan
                    ? GetSPIRVAssembly()
                    : sl_compilation_result;

  if (!sl_mapping_) {
    COMPILER_ERROR << "Could not generate SL from SPIRV";
    return;
  }

  reflector_ = std::make_unique<Reflector>(std::move(reflector_options),  //
                                           parsed_ir,                     //
                                           GetSLShaderSource(),           //
                                           sl_compiler                    //
  );

  if (!reflector_->IsValid()) {
    COMPILER_ERROR << "Could not complete reflection on generated shader.";
    return;
  }

  is_valid_ = true;
}

Compiler::~Compiler() = default;

std::unique_ptr<fml::Mapping> Compiler::GetSPIRVAssembly() const {
  if (!spv_result_) {
    return nullptr;
  }
  const auto data_length =
      (spv_result_->cend() - spv_result_->cbegin()) *
      sizeof(decltype(spv_result_)::element_type::element_type);

  return std::make_unique<fml::NonOwnedMapping>(
      reinterpret_cast<const uint8_t*>(spv_result_->cbegin()), data_length,
      [result = spv_result_](auto, auto) mutable { result.reset(); });
}

std::shared_ptr<fml::Mapping> Compiler::GetSLShaderSource() const {
  return sl_mapping_;
}

bool Compiler::IsValid() const {
  return is_valid_;
}

std::string Compiler::GetSourcePrefix() const {
  std::stringstream stream;
  stream << options_.file_name << ": ";
  return stream.str();
}

std::string Compiler::GetErrorMessages() const {
  return error_stream_.str();
}

const std::vector<std::string>& Compiler::GetIncludedFileNames() const {
  return included_file_names_;
}

static std::string JoinStrings(std::vector<std::string> items,
                               const std::string& separator) {
  std::stringstream stream;
  for (size_t i = 0, count = items.size(); i < count; i++) {
    const auto is_last = (i == count - 1);

    stream << items[i];
    if (!is_last) {
      stream << separator;
    }
  }
  return stream.str();
}

std::string Compiler::GetDependencyNames(const std::string& separator) const {
  std::vector<std::string> dependencies = included_file_names_;
  dependencies.push_back(options_.file_name);
  return JoinStrings(dependencies, separator);
}

std::unique_ptr<fml::Mapping> Compiler::CreateDepfileContents(
    std::initializer_list<std::string> targets_names) const {
  // https://github.com/ninja-build/ninja/blob/master/src/depfile_parser.cc#L28
  const auto targets = JoinStrings(targets_names, " ");
  const auto dependencies = GetDependencyNames(" ");

  std::stringstream stream;
  stream << targets << ": " << dependencies << "\n";

  auto contents = std::make_shared<std::string>(stream.str());
  return std::make_unique<fml::NonOwnedMapping>(
      reinterpret_cast<const uint8_t*>(contents->data()), contents->size(),
      [contents](auto, auto) {});
}

const Reflector* Compiler::GetReflector() const {
  return reflector_.get();
}

}  // namespace compiler
}  // namespace impeller
