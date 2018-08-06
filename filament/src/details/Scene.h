/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TNT_FILAMENT_DETAILS_SCENE_H
#define TNT_FILAMENT_DETAILS_SCENE_H

#include "upcast.h"
#include "components/LightManager.h"
#include "components/RenderableManager.h"
#include "components/TransformManager.h"

#include "details/Culler.h"
#include "details/LightData.h"

#include <filament/Box.h>
#include <filament/Scene.h>

#include <utils/compiler.h>
#include <utils/Entity.h>
#include <utils/Slice.h>
#include <utils/StructureOfArrays.h>
#include <utils/Range.h>

#include <cstddef>
#include <tsl/robin_set.h>

namespace filament {
namespace details {

struct CameraInfo;
class FEngine;
class FIndirectLight;
class FRenderer;
class FSkybox;
class LightData;


class FScene : public Scene {
public:

    /*
     * User Public API
     */

    void setSkybox(FSkybox const* skybox) noexcept;
    FSkybox const* getSkybox() const noexcept { return mSkybox; }

    void setIndirectLight(FIndirectLight const* ibl) noexcept { mIndirectLight = ibl; }
    FIndirectLight const* getIndirectLight() const noexcept { return mIndirectLight; }

    void addEntity(utils::Entity entity);
    void remove(utils::Entity entity);

    size_t getRenderableCount() const noexcept;
    size_t getLightCount() const noexcept;

public:
    /*
     * Filaments-scope Public API
     */

    // the directional light is always stored first in the LightSoA, so we need to account
    // for that in a few places.
    static constexpr size_t DIRECTIONAL_LIGHTS_COUNT = 1;

    explicit FScene(FEngine& engine);
    ~FScene() noexcept;
    void terminate(FEngine& engine);

    void prepare(const math::mat4f& worldOriginTansform);
    void prepareLights(const CameraInfo& camera) noexcept;
    void computeBounds(Aabb& castersBox, Aabb& receiversBox, uint32_t visibleLayers) const noexcept;

    /*
     * Storage for per-frame renderable data
     */

    enum {
        RENDERABLE_INSTANCE,    //  4 instance of the Renderable component
        WORLD_TRANSFORM,        // 16 instance of the Transform component
        VISIBILITY_STATE,       //  1 visibility data of the component
        UBH,                    //  4 uniform buffer handle
        BONES_UBH,              //  4 bones uniform buffer handle
        WORLD_AABB_CENTER,      // 12 world-space bounding box center of the renderable
        VISIBLE_MASK,           //  1 each bit represents a visibility in a pass

        // These are not needed anymore after culling
        LAYERS,                 //  1 layers
        WORLD_AABB_EXTENT,      // 12 world-space bounding box half-extent of the renderable

        // These are temporaries and should be stored out of line
        PRIMITIVES,             //  8 level-of-detail'ed primitives
        SUMMED_PRIMITIVE_COUNT, //  4 summed visible primitive counts
    };

    using RenderableSoa = utils::StructureOfArrays<
            utils::EntityInstance<RenderableManager>,
            math::mat4f,
            FRenderableManager::Visibility,
            Handle<HwUniformBuffer>,
            Handle<HwUniformBuffer>,
            math::float3,
            Culler::result_type,
            uint8_t,
            math::float3,
            utils::Slice<FRenderPrimitive>,
            uint32_t
    >;

    RenderableSoa const& getRenderableData() const noexcept { return mRenderableData; }
    RenderableSoa& getRenderableData() noexcept { return mRenderableData; }

    static inline uint32_t getPrimitiveCount(RenderableSoa const& soa,
            uint32_t first, uint32_t last) noexcept {
        // the caller must guarantee that last is dereferencable
        return soa.elementAt<SUMMED_PRIMITIVE_COUNT>(last) -
                soa.elementAt<SUMMED_PRIMITIVE_COUNT>(first);
    }

    static inline uint32_t getPrimitiveCount(RenderableSoa const& soa, uint32_t last) noexcept {
        // the caller must guarantee that last is dereferencable
        return soa.elementAt<SUMMED_PRIMITIVE_COUNT>(last);
    }

    /*
     * Storage for per-frame light data
     */

    enum {
        POSITION_RADIUS,
        DIRECTION,
        LIGHT_INSTANCE,
        VISIBILITY
    };

    using LightSoa = utils::StructureOfArrays<
            math::float4,
            math::float3,
            FLightManager::Instance,
            Culler::result_type
    >;

    LightSoa const& getLightData() const noexcept { return mLightData; }
    LightSoa& getLightData() noexcept { return mLightData; }

    void updateUBOs(utils::Range<uint32_t> visibleRenderables) const noexcept;

private:
    FEngine& mEngine;
    FSkybox const* mSkybox = nullptr;
    FIndirectLight const* mIndirectLight = nullptr;
    LightData mGpuLightData;

    // list of Entities in the scene. We use a robin_set<> so we can do efficient removes
    // (a vector<> could work, but removes would be O(n)). robin_set<> iterates almost as
    // nicely as vector<>, which is a good compromise.
    tsl::robin_set<utils::Entity> mEntities;
    RenderableSoa mRenderableData;
    LightSoa mLightData;
};

FILAMENT_UPCAST(Scene)

} // namespace details
} // namespace filament

#endif // TNT_FILAMENT_DETAILS_SCENE_H
