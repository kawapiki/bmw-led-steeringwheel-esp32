"""Run in Blender5.2 with --factory-startup --disable-autoexec source.blend.
Produces real-model RGB renders; no embedded source-file scripts are enabled.
"""
import bpy,bmesh,math,sys
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'.test-build/e90-render';OUT.mkdir(parents=True,exist_ok=True)
img=bpy.data.images.get('e90.png');img.filepath=str(ROOT/'assets/ui/e90/e90.png');img.reload()
for o in list(bpy.data.objects):
 if o.type in ('FONT','LIGHT','CAMERA'): bpy.data.objects.remove(o,do_unlink=True)
# Apply mirroring before splitting vehicle sides; retain the downloaded original.
for o in list(bpy.data.objects):
 if o.type=='MESH':
  bpy.context.view_layer.objects.active=o
  for mod in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=mod.name)
  o.data.transform(o.matrix_world);o.matrix_world=Matrix.Identity(4)

def mat(name,col,metal=0,rough=.5,emission=0):
 m=bpy.data.materials.new(name);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF')
 p.inputs['Base Color'].default_value=(*col,1);p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=rough
 p.inputs['Emission Color'].default_value=(*col,1);p.inputs['Emission Strength'].default_value=emission
 return m
body=mat('Cockpit silver blue',(.23,.34,.47),.55,.36)
dark=mat('Opaque interior',(.012,.018,.028),0,.85)
glass=mat('Dark tinted glass',(.023,.049,.073),.35,.22)
for o in bpy.data.objects:
 if o.type!='MESH':continue
 for i,m in enumerate(o.data.materials):
  if m.name=='Material.003':o.data.materials[i]=body
  elif m.name=='glass':o.data.materials[i]=glass
  elif o.name.startswith('Cube') and o.name!='Cube.004':o.data.materials[i]=dark
# A closed inner floor/tub removes see-through gaps behind opened panels.
bpy.ops.mesh.primitive_cube_add(size=1,location=(0,.32,.39));tub=bpy.context.object;tub.name='Interior blackout tub';tub.dimensions=(1.47,2.12,.34);tub.data.materials.append(dark)

def empty(name,loc):
 o=bpy.data.objects.new(name,None);bpy.context.collection.objects.link(o);o.location=loc;return o
parts={}
for bit,side,front in [(1,1,True),(2,-1,True),(4,1,False),(8,-1,False)]:
 pivot=empty('door_'+str(bit),(side*.82,-.73 if front else .34,.6));parts[bit]=pivot
 names=['Plane.009','Plane.015','Cube.004'] if front else ['Plane.010','Plane.017']
 for name in names:
  original=bpy.data.objects.get(name)
  if original is None:continue
  copy=original.copy();copy.data=original.data.copy();bpy.context.collection.objects.link(copy);copy.name=name+('_L' if side>0 else '_R')
  bm=bmesh.new();bm.from_mesh(copy.data);bmesh.ops.delete(bm,geom=[v for v in bm.verts if v.co.x*side < -.001],context='VERTS');bm.to_mesh(copy.data);bm.free()
  copy.parent=pivot;copy.matrix_parent_inverse=pivot.matrix_world.inverted()
  # Update inverse explicitly: new empty matrix may not be evaluated yet.
  copy.matrix_parent_inverse=Matrix.Translation(-pivot.location)
for name in ['Plane.009','Plane.015','Cube.004','Plane.010','Plane.017']:
 o=bpy.data.objects.get(name)
 if o:bpy.data.objects.remove(o,do_unlink=True)
for bit,name,loc in [(16,'Plane.008',(0,1.76,.92)),(32,'Kapdepan',(0,-.72,.83))]:
 pivot=empty('panel_'+str(bit),loc);parts[bit]=pivot;o=bpy.data.objects.get(name);o.parent=pivot;o.matrix_parent_inverse=Matrix.Translation(-pivot.location)
# Keep the two bonnet/trunk badges attached to their moving panel.
for name,bit in [('Circle',32),('Circle.001',16)]:
 o=bpy.data.objects.get(name)
 if o:o.parent=parts[bit];o.matrix_parent_inverse=Matrix.Translation(-parts[bit].location)
# Illustrative angel eyes/high-beam lamps. No live lamp state is fabricated.
ringmat=mat('Angel eye emission',(.75,.9,1),0,.3,0)
beammat=mat('High beam emission',(.88,.95,1),0,.3,0)
for side in [-1,1]:
 for x in [.54,.70]:
  y=-2.015+(x-.54)*.35
  bpy.ops.mesh.primitive_torus_add(major_segments=24,minor_segments=6,location=(side*x,y,.50),rotation=(math.pi/2,0,0),major_radius=.041,minor_radius=.007)
  bpy.context.object.data.materials.append(ringmat)
 bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,radius=.032,location=(side*.54,-2.025,.50));bpy.context.object.data.materials.append(beammat)
sc=bpy.context.scene;sc.render.engine='CYCLES';sc.cycles.samples=24;sc.cycles.use_denoising=True
sc.world.use_nodes=True;sc.world.node_tree.nodes['Background'].inputs[0].default_value=(.09,.12,.18,1);sc.world.node_tree.nodes['Background'].inputs[1].default_value=.4
for loc,energy,size in [((1,-4,6),1200,5),((-4,2,4),1500,4),((3,4,3),900,3)]:
 bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=energy;o.data.shape='DISK';o.data.size=size;o.rotation_euler=(Vector((0,0,.5))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add();cam=bpy.context.object;cam.data.type='ORTHO';cam.data.ortho_scale=6.5;sc.camera=cam
sc.render.resolution_x=576;sc.render.resolution_y=312;sc.render.resolution_percentage=100;sc.render.film_transparent=True
sc.view_settings.view_transform='AgX';sc.render.image_settings.file_format='PNG';sc.render.image_settings.color_mode='RGBA'

# Optical high-beam flare is rendered in Blender, never synthesized as live data.
comp=bpy.data.node_groups.new('E90 lamp optics','CompositorNodeTree')
comp.interface.new_socket(name='Image',in_out='OUTPUT',socket_type='NodeSocketColor')
layer=comp.nodes.new('CompositorNodeRLayers');glare=comp.nodes.new('CompositorNodeGlare')
glare.inputs['Type'].default_value='Fog Glow';glare.inputs['Threshold'].default_value=2.0
glare.inputs['Size'].default_value=.25
out=comp.nodes.new('NodeGroupOutput');comp.links.new(layer.outputs['Image'],glare.inputs['Image']);comp.links.new(glare.outputs['Image'],out.inputs['Image']);sc.compositing_node_group=comp
# Vehicle translation and wheel spin make the entry a roll-in, not a camera pan.
vehicle=empty('Vehicle motion root',(0,0,0))
for o in list(bpy.data.objects):
 if o is not vehicle and o.parent is None and o.type in ('MESH','EMPTY'):
  o.parent=vehicle
wheels=[]
for name in ['Circle.002','Circle.003','Circle.004','Circle.005']:
 o=bpy.data.objects.get(name)
 center=sum((Vector(v) for v in o.bound_box),Vector())/8
 pivot=empty('Wheel pivot '+name,center);pivot.parent=vehicle
 o.parent=pivot;o.matrix_parent_inverse=Matrix.Translation(-center);wheels.append(pivot)

def pose(mask):
 for bit,p in parts.items():
  p.rotation_euler=(0,0,0)
  if mask&bit:
   if bit<16:p.rotation_euler.z=math.radians(-52 if bit in (1,4) else 52)
   else:p.rotation_euler.x=math.radians(38 if bit==16 else -34)
def camera(yaw,elev,shift=0):
 a=math.radians(yaw);e=math.radians(elev);target=Vector((shift,.12,.52))
 cam.location=target+Vector((8*math.sin(a)*math.cos(e),-8*math.cos(a)*math.cos(e),8*math.sin(e)))
 cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
def render(name):
 sc.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)
def closure(mask):
 left=mask&5;right=mask&10
 if left and not right: yaw,e=58,17
 elif right and not left:yaw,e=-58,17
 elif mask==16:yaw,e=155,19
 else:yaw,e=0,24
 pose(mask);camera(yaw,e);render('closure_%02d'%mask)
if '--preview' in sys.argv:
 for m in [0,1,3,4,16,32]:closure(m)
else:
 if '--boot-only' not in sys.argv:
  for m in range(64):closure(m)
 pose(0)
 for f in range(28):
  if f<6:
   t=f/5;distance=12*(1-t)*(1-t);vehicle.location.y=distance
   for wheel in wheels:wheel.rotation_euler.x=distance/.32
   camera(0,18)
  elif f<24:
   vehicle.location.y=0
   camera(360*(f-6)/17,18)
  else:camera(0,18)
  flash=f in (24,26)
  ringmat.node_tree.nodes.get('Principled BSDF').inputs['Emission Strength'].default_value=60 if flash else 0
  beammat.node_tree.nodes.get('Principled BSDF').inputs['Emission Strength'].default_value=150 if flash else 0
  render('boot_%02d'%f)
img.pack();img.filepath='//e90.png'
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'assets/ui/e90/prepared.blend'))
