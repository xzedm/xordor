// Reads a .glb / .gltf file picked by the person and turns it into a project asset.
import { MAX_ASSET_BYTES, normalizeAsset } from '../core/project.js';
import { bufferToDataUrl, describeModel, parseModel, primeModel, renderModelThumbnail } from '../engine/models.js';

export async function readModelFile(file) {
  if (file.size > MAX_ASSET_BYTES) {
    throw new Error(`This file is ${(file.size / 1048576).toFixed(1)} MB. The limit is ${MAX_ASSET_BYTES / 1048576} MB.`);
  }
  const buffer = await file.arrayBuffer();
  const head = new TextDecoder().decode(new Uint8Array(buffer, 0, Math.min(4, buffer.byteLength)));
  const isGlb = head === 'glTF';
  if (!isGlb && !/\.gltf$/i.test(file.name)) throw new Error('Pick a .glb or .gltf file. Most 3D tools (Blender, Sketchfab, Mixamo...) can export one.');
  const gltf = await parseModel(buffer);
  const info = describeModel(gltf);
  const asset = normalizeAsset({
    name: file.name.replace(/\.(glb|gltf)$/i, '').replace(/[_-]+/g, ' ').trim() || 'Model',
    data: bufferToDataUrl(buffer, isGlb ? 'model/gltf-binary' : 'model/gltf+json'),
    bytes: buffer.byteLength,
    animations: info.animations,
    thumb: renderModelThumbnail(gltf),
  });
  primeModel(asset, gltf);
  return { asset, info };
}
