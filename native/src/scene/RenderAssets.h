#pragma once
// RenderAssets: how the scene renderer turns asset ids into GPU objects.
//
// MeshRenderer components hold ids, not pointers, so the scene can be saved
// and assets can be loaded lazily. But scene/ comes *before* assets/ in the
// module order (assets/ needs scene/ to save scenes), so scene code can't
// call the asset manager directly. Instead scene/ declares what it needs,
// this small interface, and assets/AssetManager implements it:
//
//     scene/  SceneRenderer ──uses──► RenderAssets (interface)
//                                          ▲
//     assets/ AssetManager ─implements─────┘
//
// This is "dependency inversion": the lower module owns the interface, the
// higher module plugs into it. Tests can plug in a fake that hands out meshes
// without any files.

#include "core/AssetId.h"
#include "render/Mesh.h"
#include "render/Texture.h"

namespace Engine {

class RenderAssets {
public:
    virtual ~RenderAssets() = default;
    // nullptr if the id is unknown or failed to load (the implementation
    // logs why, once). Not const: the first request may load the file.
    virtual const Mesh* mesh(AssetId id) = 0;
    virtual const Texture* texture(AssetId id) = 0;
};

} // namespace Engine
