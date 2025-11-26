#!/usr/bin/env python3
"""Utility script to inspect GLB models for meshes, materials, and hand bones."""
from pathlib import Path
from pygltflib import GLTF2


def analyze(path: Path) -> GLTF2:
    gltf = GLTF2().load_binary(str(path))
    print(f"=== {path} ===")
    print(f"Nodes: {len(gltf.nodes)}, Meshes: {len(gltf.meshes)}, Materials: {len(gltf.materials)}")
    print(f"Textures: {len(gltf.textures)}")
    for idx, mat in enumerate(gltf.materials or []):
        name = mat.name or f"material_{idx}"
        base_tex = None
        if mat.pbrMetallicRoughness and mat.pbrMetallicRoughness.baseColorTexture:
            base_tex = mat.pbrMetallicRoughness.baseColorTexture.index
        normal_tex = mat.normalTexture.index if mat.normalTexture else None
        print(f"  Material {idx} '{name}': baseColorTex={base_tex}, normalTex={normal_tex}")
    return gltf


def list_hand_nodes(gltf: GLTF2):
    print("Nodes containing 'hand':")
    for idx, node in enumerate(gltf.nodes or []):
        name = node.name or f"node_{idx}"
        if 'hand' in name.lower():
            print(f"  Node {idx}: {name}")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    stick = analyze(root / "assets/models/stick.glb")
    sponge = analyze(root / "assets/models/sponge.glb")
    print()
    list_hand_nodes(sponge)
