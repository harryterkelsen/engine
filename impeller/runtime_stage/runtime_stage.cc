// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/runtime_stage/runtime_stage.h"

#include <array>
#include <memory>

#include "fml/mapping.h"
#include "impeller/base/validation.h"
#include "impeller/runtime_stage/runtime_stage_flatbuffers.h"
#include "runtime_stage_types_flatbuffers.h"

namespace impeller {

static RuntimeUniformType ToType(fb::UniformDataType type) {
  switch (type) {
    case fb::UniformDataType::kBoolean:
      return RuntimeUniformType::kBoolean;
    case fb::UniformDataType::kSignedByte:
      return RuntimeUniformType::kSignedByte;
    case fb::UniformDataType::kUnsignedByte:
      return RuntimeUniformType::kUnsignedByte;
    case fb::UniformDataType::kSignedShort:
      return RuntimeUniformType::kSignedShort;
    case fb::UniformDataType::kUnsignedShort:
      return RuntimeUniformType::kUnsignedShort;
    case fb::UniformDataType::kSignedInt:
      return RuntimeUniformType::kSignedInt;
    case fb::UniformDataType::kUnsignedInt:
      return RuntimeUniformType::kUnsignedInt;
    case fb::UniformDataType::kSignedInt64:
      return RuntimeUniformType::kSignedInt64;
    case fb::UniformDataType::kUnsignedInt64:
      return RuntimeUniformType::kUnsignedInt64;
    case fb::UniformDataType::kHalfFloat:
      return RuntimeUniformType::kHalfFloat;
    case fb::UniformDataType::kFloat:
      return RuntimeUniformType::kFloat;
    case fb::UniformDataType::kDouble:
      return RuntimeUniformType::kDouble;
    case fb::UniformDataType::kSampledImage:
      return RuntimeUniformType::kSampledImage;
  }
  FML_UNREACHABLE();
}

static RuntimeShaderStage ToShaderStage(fb::Stage stage) {
  switch (stage) {
    case fb::Stage::kVertex:
      return RuntimeShaderStage::kVertex;
    case fb::Stage::kFragment:
      return RuntimeShaderStage::kFragment;
    case fb::Stage::kCompute:
      return RuntimeShaderStage::kCompute;
  }
  FML_UNREACHABLE();
}

std::unique_ptr<RuntimeStage> RuntimeStage::RuntimeStageIfPresent(
    const fb::RuntimeStage* runtime_stage,
    const std::shared_ptr<fml::Mapping>& payload) {
  if (!runtime_stage) {
    return nullptr;
  }

  return std::unique_ptr<RuntimeStage>(
      new RuntimeStage(runtime_stage, payload));
}

RuntimeStage::Map RuntimeStage::DecodeRuntimeStages(
    const std::shared_ptr<fml::Mapping>& payload) {
  if (payload == nullptr || !payload->GetMapping()) {
    return {};
  }
  if (!fb::RuntimeStagesBufferHasIdentifier(payload->GetMapping())) {
    return {};
  }

  auto raw_stages = fb::GetRuntimeStages(payload->GetMapping());
  return {
      {RuntimeStageBackend::kSkSL,
       RuntimeStageIfPresent(raw_stages->sksl(), payload)},
      {RuntimeStageBackend::kMetal,
       RuntimeStageIfPresent(raw_stages->metal(), payload)},
      {RuntimeStageBackend::kOpenGLES,
       RuntimeStageIfPresent(raw_stages->opengles(), payload)},
      {RuntimeStageBackend::kVulkan,
       RuntimeStageIfPresent(raw_stages->vulkan(), payload)},
  };
}

RuntimeStage::RuntimeStage(const fb::RuntimeStage* runtime_stage,
                           const std::shared_ptr<fml::Mapping>& payload)
    : payload_(payload) {
  FML_DCHECK(runtime_stage);

  stage_ = ToShaderStage(runtime_stage->stage());
  entrypoint_ = runtime_stage->entrypoint()->str();

  auto* uniforms = runtime_stage->uniforms();
  if (uniforms) {
    for (auto i = uniforms->begin(), end = uniforms->end(); i != end; i++) {
      RuntimeUniformDescription desc;
      desc.name = i->name()->str();
      desc.location = i->location();
      desc.type = ToType(i->type());
      desc.dimensions = RuntimeUniformDimensions{
          static_cast<size_t>(i->rows()), static_cast<size_t>(i->columns())};
      desc.bit_width = i->bit_width();
      desc.array_elements = i->array_elements();
      uniforms_.emplace_back(std::move(desc));
    }
  }

  code_mapping_ = std::make_shared<fml::NonOwnedMapping>(
      runtime_stage->shader()->data(),     //
      runtime_stage->shader()->size(),     //
      [payload = payload_](auto, auto) {}  //
  );

  is_valid_ = true;
}

RuntimeStage::~RuntimeStage() = default;
RuntimeStage::RuntimeStage(RuntimeStage&&) = default;
RuntimeStage& RuntimeStage::operator=(RuntimeStage&&) = default;

bool RuntimeStage::IsValid() const {
  return is_valid_;
}

const std::shared_ptr<fml::Mapping>& RuntimeStage::GetCodeMapping() const {
  return code_mapping_;
}

const std::vector<RuntimeUniformDescription>& RuntimeStage::GetUniforms()
    const {
  return uniforms_;
}

const RuntimeUniformDescription* RuntimeStage::GetUniform(
    const std::string& name) const {
  for (const auto& uniform : uniforms_) {
    if (uniform.name == name) {
      return &uniform;
    }
  }
  return nullptr;
}

const std::string& RuntimeStage::GetEntrypoint() const {
  return entrypoint_;
}

RuntimeShaderStage RuntimeStage::GetShaderStage() const {
  return stage_;
}

bool RuntimeStage::IsDirty() const {
  return is_dirty_;
}

void RuntimeStage::SetClean() {
  is_dirty_ = false;
}

}  // namespace impeller
