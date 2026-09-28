"""Export the attributed E90 prepared asset for bounded software rasterization.
Run: blender --background --factory-startup --disable-autoexec
     assets/ui/e90/prepared.blend --python tools/assets/export_e90_mesh.py
No .blend file is saved. The generated header has world/rest coordinates,
front -Y, up +Z. Lamps and closure groups remain independently transformable.
"""
import hashlib
import json
import math
from collections import Counter
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'assets/ui/e90/prepared.blend'
HEADER = ROOT / 'firmware/wheel/components/wheel_ui/e90_mesh.h'
MANIFEST = ROOT / 'docs/design/e90-closure-camera/mesh-manifest.json'
MAX_TRIANGLES = 2500
source_hash = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
GROUPS = {'door_1': 1, 'door_2': 2, 'door_4': 3, 'door_8': 4,
          'panel_16': 5, 'panel_32': 6}

# Discard scene animation and only reset animated parent transforms. Child
# local matrices/inverse parents retain the already prepared hinge geometry.
for obj in bpy.data.objects:
    obj.animation_data_clear()
    if obj.name == 'Vehicle motion root':
        obj.location = (0, 0, 0)
        obj.rotation_euler = (0, 0, 0)
        obj.scale = (1, 1, 1)
    elif obj.name in GROUPS or obj.name.startswith('Wheel pivot '):
        obj.rotation_euler = (0, 0, 0)
bpy.context.view_layer.update()
wheel_pivots = [None] * 4
wheel_groups = {}
for obj in bpy.data.objects:
    if obj.name.startswith('Wheel pivot '):
        position = obj.matrix_world.translation
        index = (0 if position.y < 0 else 2) + (0 if position.x > 0 else 1)
        wheel_pivots[index] = tuple(position)
        wheel_groups[obj.name] = 9 + index
assert all(p is not None for p in wheel_pivots)

images = {}
for image in bpy.data.images:
    if image.name == 'e90.png':
        if not image.has_data:
            image.filepath = str(ROOT / 'assets/ui/e90/e90.png')
            image.reload()
        images[image.name] = (image.size[0], image.size[1], tuple(image.pixels))


def group_for(obj):
    node = obj
    while node:
        if node.name in GROUPS:
            return GROUPS[node.name]
        if node.name in wheel_groups:
            return wheel_groups[node.name]
        node = node.parent
    names = {m.name for m in obj.data.materials if m}
    if 'Angel eye emission' in names:
        return 7
    if 'High beam emission' in names:
        return 8
    return 0


def srgb(v):
    return 12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055


def face_rgb(mesh, triangle):
    mat = mesh.materials[triangle.material_index] if mesh.materials else None
    rgb = (.35, .4, .48)
    if mat:
        rgb = tuple(mat.diffuse_color[:3])
        if mat.use_nodes:
            bsdf = next((n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if bsdf:
                socket = bsdf.inputs['Base Color']
                rgb = tuple(socket.default_value[:3])
                image_node = socket.links[0].from_node if socket.is_linked else None
                if image_node and image_node.type == 'TEX_IMAGE' and image_node.image and mesh.uv_layers.active:
                    data = images.get(image_node.image.name)
                    if data:
                        width, height, pixels = data
                        uvs = [mesh.uv_layers.active.data[i].uv for i in triangle.loops]
                        # Three interior samples reduce UV-edge/atlas bleed.
                        colors = []
                        for weights in ((.6, .2, .2), (.2, .6, .2), (.2, .2, .6)):
                            u = sum(uv.x * w for uv, w in zip(uvs, weights))
                            v = sum(uv.y * w for uv, w in zip(uvs, weights))
                            x = min(width - 1, max(0, int(u * width)))
                            y = min(height - 1, max(0, int(v * height)))
                            offset = (y * width + x) * 4
                            colors.append(pixels[offset:offset + 3])
                        return tuple(sum(c[i] for c in colors) / 3 for i in range(3))
    return tuple(srgb(max(0, min(1, c))) for c in rgb)


def rgb565(rgb):
    r, g, b = (max(0, min(255, round(c * 255))) for c in rgb)
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


# Replace texture-averaged source badges with recognizable, bounded geometry.
# At the native panel scale these emphasize the roundel, not unreadable letters.
roundels = []
roundel_metadata = []
for name, group, radius in [('Circle', 6, .080), ('Circle.001', 5, .075)]:
    badge = bpy.data.objects[name]
    cap = max(badge.data.polygons, key=lambda polygon: polygon.area)
    normal = (badge.matrix_world.to_3x3().inverted().transposed() @ cap.normal).normalized()
    center = badge.matrix_world @ cap.center + normal * .004
    u = Vector((1, 0, 0))
    u = (u - normal * u.dot(normal)).normalized()
    v = normal.cross(u).normalized()
    def point(r, angle):
        return center + r * (u * math.cos(angle) + v * math.sin(angle))
    for i in range(16):
        a, b = 2 * math.pi * i / 16, 2 * math.pi * (i + 1) / 16
        for outer, inner, color in [(1, .89, 0xc618), (.89, .61, 0x0841)]:
            p0, p1 = point(radius * outer, a), point(radius * outer, b)
            p2, p3 = point(radius * inner, b), point(radius * inner, a)
            roundels.extend([([p0, p1, p2], color, group, 0), ([p0, p2, p3], color, group, 0)])
        # White upper-right/lower-left; blue upper-left/lower-right.
        color = 0xffff if (i // 4) % 2 == 0 else 0x045f
        roundels.append(([center.copy(), point(radius * .61, a), point(radius * .61, b)], color, group, 0))
    roundel_metadata.append({'source_object': name, 'group': group,
                             'center': list(center), 'normal': list(normal),
                             'radius': radius, 'triangles': 80})


entries = []
for original in list(bpy.data.objects):
    if original.type != 'MESH' or original.name in ('Circle', 'Circle.001'):
        continue
    obj = bpy.data.objects.new('export_' + original.name, original.data.copy())
    bpy.context.collection.objects.link(obj)
    obj.data.transform(original.matrix_world)
    obj.data.calc_loop_triangles()
    count = len(obj.data.loop_triangles)
    group = group_for(original)
    names = {m.name for m in obj.data.materials if m}
    weight = .28 if original.name.startswith('Circle.00') and original.parent and original.parent.name.startswith('Wheel pivot') else 1.0
    if group in (7, 8):
        weight = .13
    if 'Dark tinted glass' in names:
        weight = 1.5
    if original.name in ('Circle', 'Circle.001'):
        weight = .2
    modifier = obj.modifiers.new('Bounded runtime mesh', 'DECIMATE')
    modifier.decimate_type = 'COLLAPSE'
    modifier.use_collapse_triangulate = True
    entries.append((original.name, obj, modifier, count, group, weight))


def extract(scale, include_colors=False):
    for _, obj, modifier, count, _, weight in entries:
        modifier.ratio = min(1.0, max(4 / max(1, count), scale * weight))
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    triangles, counts = [], {}
    for name, obj, _, _, group, _ in entries:
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        mesh.calc_loop_triangles()
        kept = 0
        for triangle in mesh.loop_triangles:
            points = [mesh.vertices[i].co.copy() for i in triangle.vertices]
            if (points[1] - points[0]).cross(points[2] - points[0]).length_squared < 1e-14:
                continue
            if not all(math.isfinite(v) and abs(v) < 10 for p in points for v in p):
                raise ValueError('Unexpected model coordinate')
            face_group = group
            center_x = sum(point.x for point in points) / 3
            if name == 'lampudepan':
                face_group = 13 if center_x >= 0 else 14
            elif name == 'Plane.004':
                face_group = 15 if center_x >= 0 else 16
            material_name = mesh.materials[triangle.material_index].name if mesh.materials and mesh.materials[triangle.material_index] else ''
            material = 1 if material_name == 'Cockpit silver blue' else 2 if material_name == 'Dark tinted glass' else 0
            # Classification is per face, never per parent/closure: badges,
            # lamp lenses and wheels retain their original materials.
            rgb = (.14, .15, .16) if material == 1 else (.075, .09, .105) if material == 2 else face_rgb(mesh, triangle) if include_colors else (0, 0, 0)
            triangles.append((points, rgb565(rgb) if include_colors else 0, face_group, material))
            kept += 1
        counts[name] = kept
        evaluated.to_mesh_clear()
    return triangles, counts


lo, hi = 0.0, 1.0
for _ in range(14):
    mid = (lo + hi) / 2
    triangles, _ = extract(mid)
    if len(triangles) + len(roundels) > MAX_TRIANGLES:
        hi = mid
    else:
        lo = mid
triangles, counts = extract(lo, True)
triangles.extend(roundels)
counts['Roundel hood'] = 80
counts['Roundel trunk'] = 80
assert 1000 <= len(triangles) <= MAX_TRIANGLES
assert set(range(17)).issubset({group for _, _, group, _ in triangles})
# Quantization is deliberately fine enough to avoid moving visible panel edges.
lines = ['// BMW E90 by rifdanzz / Rifdan Adidan, CC BY 4.0.',
         '// Derived from assets/ui/e90/prepared.blend; see ATTRIBUTION.md and mesh-manifest.json.',
         '#pragma once', '#include <stdint.h>',
         'struct E90Triangle { float v[9]; uint16_t color; uint8_t group; uint8_t material; };',
         'static const E90Triangle e90_triangles[] = {']
for points, color, group, material in triangles:
    values = ','.join(f'{float(c):.6f}f' for point in points for c in point)
    lines.append('  {{' + values + f'}},0x{color:04x},{group},{material}' + '},')
lines += ['};', 'static const float e90_wheel_pivots[4][3] = {']
for pivot in wheel_pivots:
    lines.append('  {' + ','.join(f'{c:.6f}f' for c in pivot) + '},')
lines += ['};', '#define E90_TRIANGLE_COUNT (sizeof(e90_triangles) / sizeof(e90_triangles[0]))', '']
HEADER.write_text('\n'.join(lines), encoding='utf-8')
bounds = [[min(p[axis] for points, _, _, _ in triangles for p in points),
           max(p[axis] for points, _, _, _ in triangles for p in points)] for axis in range(3)]
manifest = {
    'contract': 'runtime3D1', 'source': 'assets/ui/e90/prepared.blend',
    'source_sha256': source_hash, 'source_modified': False,
    'attribution': 'BMW 320i E90 Low Polly by rifdanzz / Rifdan Adidan',
    'source_url': 'https://sketchfab.com/3d-models/bmw-320i-e90-low-polly-28f0bc2f0083417a8e521150529034d6',
    'license': 'CC BY 4.0', 'license_url': 'https://creativecommons.org/licenses/by/4.0/',
    'triangles': len(triangles), 'triangle_budget': MAX_TRIANGLES,
    'header_bytes': HEADER.stat().st_size, 'expected_struct_bytes': 40,
    'expected_array_bytes': len(triangles) * 40,
    'header_sha256': hashlib.sha256(HEADER.read_bytes()).hexdigest(),
    'coordinate_convention': 'world/rest coordinates; front -Y; up +Z; left +X',
    'bounds_xyz': bounds, 'groups': dict(sorted(Counter(g for _, _, g, _ in triangles).items())),
    'objects': counts, 'collapse_scale': lo,
    'wheel_group_order': '9 FL, 10 FR, 11 RL, 12 RR',
    'lamp_groups': {'13': 'lampudepan front left (+X)', '14': 'lampudepan front right (-X)', '15': 'Plane.004 rear left (+X)', '16': 'Plane.004 rear right (-X)'},
    'wheel_pivots_xyz': wheel_pivots,
    'roundels': roundel_metadata,
    'materials': {'0': 'other', '1': 'black body paint', '2': 'dark tinted glass'},
    'material_counts': dict(sorted(Counter(m for _, _, _, m in triangles).items())),
    'paint_base_srgb': [.14, .15, .16],
    'simplification': 'Per-object Blender collapse decimation, weighted lower for wheels/illustrative lamps, higher for windows; binary-searched total budget; source badge meshes replaced by 160 explicitly reserved roundel triangles; all other objects and closure groups retained.',
    'color': 'RGB565 base material/UV samples; body paint near-black sRGB (.14,.15,.16). Explicit per-face material ID allows runtime metallic/clearcoat lighting; material does not change geometry.',
    'rest_reset': 'Vehicle motion root translation/rotation/scale; six closure pivot rotations; wheel pivot rotations; scene animation cleared in memory.',
    'limitations': 'Simplified community model; not measured BMW geometry. Fine texture detail approximated by flat per-face colors. Hood/trunk roundels are exaggerated geometric derivatives (chrome outline, black ring, blue/white center), not readable lettering. No device performance claim.',
    'export_command': 'blender --background --factory-startup --disable-autoexec assets/ui/e90/prepared.blend --python tools/assets/export_e90_mesh.py'
}
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == source_hash
MANIFEST.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'triangles': len(triangles), 'array_bytes': len(triangles)*40, 'bounds': bounds, 'groups': manifest['groups']}))
