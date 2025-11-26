#!/usr/bin/env python3
"""
Examine tree1.glb model structure, geometry, and textures.
"""

import struct
import json
import sys
from pathlib import Path

def examine_glb(filepath):
    """Parse and display GLB file structure."""
    print(f"=== Examining {filepath} ===\n")
    
    with open(filepath, 'rb') as f:
        # Read GLB header
        magic = f.read(4)
        if magic != b'glTF':
            print("Error: Not a valid GLB file")
            return
        
        version = struct.unpack('<I', f.read(4))[0]
        length = struct.unpack('<I', f.read(4))[0]
        
        print(f"GLB Version: {version}")
        print(f"File Length: {length} bytes\n")
        
        # Read JSON chunk
        json_chunk_length = struct.unpack('<I', f.read(4))[0]
        json_chunk_type = f.read(4)
        
        if json_chunk_type != b'JSON':
            print("Error: Expected JSON chunk")
            return
        
        json_data = f.read(json_chunk_length).decode('utf-8')
        gltf = json.loads(json_data)
        
        # Display asset info
        if 'asset' in gltf:
            print("Asset Info:")
            for key, value in gltf['asset'].items():
                print(f"  {key}: {value}")
            print()
        
        # Display scenes
        if 'scenes' in gltf:
            print(f"Scenes: {len(gltf['scenes'])}")
            for i, scene in enumerate(gltf['scenes']):
                name = scene.get('name', f'Scene {i}')
                nodes = scene.get('nodes', [])
                print(f"  {i}: {name} (nodes: {nodes})")
            print()
        
        # Display nodes
        if 'nodes' in gltf:
            print(f"Nodes: {len(gltf['nodes'])}")
            for i, node in enumerate(gltf['nodes']):
                name = node.get('name', f'Node {i}')
                mesh_idx = node.get('mesh', None)
                translation = node.get('translation', [0, 0, 0])
                rotation = node.get('rotation', [0, 0, 0, 1])
                scale = node.get('scale', [1, 1, 1])
                print(f"  {i}: {name}")
                if mesh_idx is not None:
                    print(f"     Mesh: {mesh_idx}")
                print(f"     Translation: {translation}")
                print(f"     Rotation: {rotation}")
                print(f"     Scale: {scale}")
            print()
        
        # Display meshes
        if 'meshes' in gltf:
            print(f"Meshes: {len(gltf['meshes'])}")
            for i, mesh in enumerate(gltf['meshes']):
                name = mesh.get('name', f'Mesh {i}')
                primitives = mesh.get('primitives', [])
                print(f"  {i}: {name}")
                print(f"     Primitives: {len(primitives)}")
                for j, prim in enumerate(primitives):
                    print(f"       Primitive {j}:")
                    attrs = prim.get('attributes', {})
                    for attr_name, accessor_idx in attrs.items():
                        print(f"         {attr_name}: accessor {accessor_idx}")
                    if 'indices' in prim:
                        print(f"         INDICES: accessor {prim['indices']}")
                    if 'material' in prim:
                        print(f"         MATERIAL: {prim['material']}")
            print()
        
        # Display materials
        if 'materials' in gltf:
            print(f"Materials: {len(gltf['materials'])}")
            for i, mat in enumerate(gltf['materials']):
                name = mat.get('name', f'Material {i}')
                print(f"  {i}: {name}")
                
                if 'pbrMetallicRoughness' in mat:
                    pbr = mat['pbrMetallicRoughness']
                    if 'baseColorTexture' in pbr:
                        tex_idx = pbr['baseColorTexture']['index']
                        print(f"     Base Color Texture: {tex_idx}")
                    if 'baseColorFactor' in pbr:
                        color = pbr['baseColorFactor']
                        print(f"     Base Color Factor: {color}")
                    if 'metallicFactor' in pbr:
                        print(f"     Metallic: {pbr['metallicFactor']}")
                    if 'roughnessFactor' in pbr:
                        print(f"     Roughness: {pbr['roughnessFactor']}")
                
                if 'normalTexture' in mat:
                    print(f"     Normal Texture: {mat['normalTexture']['index']}")
                if 'emissiveTexture' in mat:
                    print(f"     Emissive Texture: {mat['emissiveTexture']['index']}")
            print()
        
        # Display textures
        if 'textures' in gltf:
            print(f"Textures: {len(gltf['textures'])}")
            for i, tex in enumerate(gltf['textures']):
                name = tex.get('name', f'Texture {i}')
                source = tex.get('source', None)
                sampler = tex.get('sampler', None)
                print(f"  {i}: {name}")
                if source is not None:
                    print(f"     Image Source: {source}")
                if sampler is not None:
                    print(f"     Sampler: {sampler}")
            print()
        
        # Display images
        if 'images' in gltf:
            print(f"Images: {len(gltf['images'])}")
            for i, img in enumerate(gltf['images']):
                name = img.get('name', f'Image {i}')
                print(f"  {i}: {name}")
                if 'uri' in img:
                    print(f"     URI: {img['uri']}")
                if 'mimeType' in img:
                    print(f"     MIME Type: {img['mimeType']}")
                if 'bufferView' in img:
                    print(f"     Buffer View: {img['bufferView']} (embedded)")
            print()
        
        # Display accessors (geometry data info)
        if 'accessors' in gltf:
            print(f"Accessors: {len(gltf['accessors'])}")
            for i, acc in enumerate(gltf['accessors']):
                comp_type_map = {
                    5120: "BYTE", 5121: "UNSIGNED_BYTE",
                    5122: "SHORT", 5123: "UNSIGNED_SHORT",
                    5125: "UNSIGNED_INT", 5126: "FLOAT"
                }
                comp_type = comp_type_map.get(acc.get('componentType', 0), "UNKNOWN")
                acc_type = acc.get('type', 'UNKNOWN')
                count = acc.get('count', 0)
                print(f"  {i}: {acc_type} x {count} ({comp_type})")
                if 'min' in acc and 'max' in acc:
                    print(f"     Range: {acc['min']} to {acc['max']}")
            print()
        
        # Display buffer views
        if 'bufferViews' in gltf:
            print(f"Buffer Views: {len(gltf['bufferViews'])}")
            for i, bv in enumerate(gltf['bufferViews']):
                buffer = bv.get('buffer', 0)
                byte_length = bv.get('byteLength', 0)
                byte_offset = bv.get('byteOffset', 0)
                print(f"  {i}: Buffer {buffer}, Length: {byte_length}, Offset: {byte_offset}")
            print()
        
        # Display buffers
        if 'buffers' in gltf:
            print(f"Buffers: {len(gltf['buffers'])}")
            for i, buf in enumerate(gltf['buffers']):
                byte_length = buf.get('byteLength', 0)
                print(f"  {i}: {byte_length} bytes")
                if 'uri' in buf:
                    print(f"     URI: {buf['uri']}")
                else:
                    print(f"     (embedded in BIN chunk)")
            print()

if __name__ == '__main__':
    model_path = Path('assets/models/tree1.glb')
    
    if not model_path.exists():
        print(f"Error: {model_path} not found")
        sys.exit(1)
    
    examine_glb(model_path)
