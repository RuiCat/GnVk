"""fbx2glb — 用 Blender 无头模式把 FBX 角色（骨架 + 蒙皮 + 内嵌贴图）转成 glTF 2.0 GLB。

用法（由 Makefile 的 imoSceneAsset 目标调用，不直接进仓库构建）：
    blender -b --factory-startup --python tools/assetconvert/fbx2glb.py -- <in.fbx> <out.glb>

约定：
  * 导出 Y-up（glTF 规范），保留 skin（骨架/权重/逆绑定）与贴图（GLB 内嵌）；
  * 不导出动画（场景只显示导入姿势）与 morph 目标；
  * 不启用 Draco（引擎的 glTF 导入不解压缩扩展）；
  * 主网格与挂件保持 Blender 导入后的节点层级，坐标转换交给 glTF 导出器。
"""

import os
import sys
import time

import bpy


def parseArgs():
    """返回 (源 FBX, 目标 GLB)；参数不足时打印用法并退出。"""
    if "--" not in sys.argv:
        raise SystemExit("用法: blender -b --python fbx2glb.py -- <in.fbx> <out.glb>")
    argv = sys.argv[sys.argv.index("--") + 1:]
    if len(argv) < 2:
        raise SystemExit("用法: blender -b --python fbx2glb.py -- <in.fbx> <out.glb>")
    return argv[0], argv[1]


def describeScene():
    """打印导入后的对象/骨架/顶点统计，作为转换日志与验收证据。"""
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    armatures = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
    bones = sum(len(armature.data.bones) for armature in armatures)
    vertices = sum(len(mesh.data.vertices) for mesh in meshes)
    print("FBX2GLB objects=%d meshes=%d armatures=%d bones=%d vertices=%d materials=%d images=%d"
          % (len(bpy.context.scene.objects), len(meshes), len(armatures), bones, vertices,
             len(bpy.data.materials), len(bpy.data.images)))


def main():
    """导入 FBX 并导出 GLB，打印对象/顶点统计与输出大小。"""
    source, target = parseArgs()
    if not os.path.isfile(source):
        raise SystemExit("源 FBX 不存在: %s" % source)
    started = time.time()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=source)
    describeScene()
    os.makedirs(os.path.dirname(os.path.abspath(target)), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=target,
        export_format="GLB",
        export_yup=True,
        export_apply=False,
        export_skins=True,
        export_animations=False,
        export_morph=False,
        export_draco_mesh_compression_enable=False,
    )
    if not os.path.isfile(target):
        raise SystemExit("导出失败: %s" % target)
    print("FBX2GLB target=%s bytes=%d seconds=%.1f"
          % (target, os.path.getsize(target), time.time() - started))


if __name__ == "__main__":
    main()
