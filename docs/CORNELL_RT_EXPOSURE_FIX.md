# Cornell RT 预览过曝 / 二次 sRGB 编码 — 缺陷分析与修复

> **状态**：📊 快照 — 2026-11-20 Cornell RT 过曝 / 二次 sRGB 编码的缺陷分析与修复记录。

> 日期：2026-11-20
> 范围：`tests/cornellBox` 的 T/Y/U 光追预览（point 模式为主）
> 性质：CPU 参考可复算；窗口效果本会话无 GPU，未实测。

## 1. 摘要

HW RT point 模式整体接近纯白（CPU 参考均值为 236/255），金属球看起来像彩色环。
根因有两个，且叠加：

1. **点光强度 14 过高**：CPU/GPU/HW/引擎四路都硬编码 14，CPU 参考在 sun 模式均值 197，
   point 模式 236，曝光不一致。
2. **RT 图像在 sRGB 交换链上被二次编码**：RT 像素已经是 gamma 编码字节，上传为
   `R8G8B8A8_UNORM`；2D 采样不解码，直接写 sRGB 交换链时硬件再编码一次。

修复：点光 14→3；RT 图像在 sRGB 交换链上用 `R8G8B8A8_SRGB` 纹理（采样硬件解码）；
HUD 加深色阴影。金属球不是缺陷——CPU 参考里它本来就是镜面反射。

## 2. 现象

- 用户截图：`Cornell Box (HW RT) light=point`，HUD 显示 `RT 1.6 ms upload 3.0 ms spp 16`，
  墙面接近 255，红色/绿色墙面只剩很淡的粉色/绿色。
- 金属球呈红/白/绿/蓝拼块，像环而不是球。
- HUD 文本在白色背景上几乎不可读（不是重复绘制，是低对比度）。

## 3. 复现（CPU，无需 GPU）

- 用 `renderCpuRt(..., lightMode=1)` 渲染 Cornell 的 point 模式（pose 0，640×400，8 spp）：
  - intensity=14：均值 `(236.9, 230.8, 225.7)`；
  - intensity=3：均值 `(194.2, 191.8, 189.8)`；
  - sun 模式：均值 `(197.2, 197.1, 194.3)`。
- 结论：point 14 本身就过曝；与 HW RT 无关。CPU 参考图里金属球是正常的镜面反射
  （红/绿墙 + 白色高光 + 地面反射）。

## 4. 根因链

### 4.1 点光强度

- 场景：`tests/cornellBox/cornellScene.cpp:177` 的 `AddPoint(..., 14.0f)`。
- CPU：`tests/cornellBox/cornellCpuRt.cpp:189` 的 `ndl / PI * 14.0f * att`。
- GPU/HW：`assets/shaders/gn_cornell_rt.comp:176`、`gn_cornell_rq.comp:169` 同式。
- 点光在 `ROOM_HEIGHT - 0.2`，range 9；衰减到 4.8 m 仍有约 0.43，14 × 0.43 ≈ 6，
  经 `/π` 后仍大于 1，ACES + gamma 后接近白。
- 被破坏的不变量：**point 与 sun 两个光照变体应保持相近曝光**；14 未按这个约束取值。

### 4.2 二次 sRGB

- 交换链优先 sRGB：`src/graphics/_Vulkan/GN_Vk.cpp:39-50`。
- 2D 叠层 render pass 直接用交换链格式：`GN_Vk.cpp:380`。
- RT 图像走 `sIMAGE::DataFill`，格式固定 `R8G8B8A8_UNORM`：`vkImageData.cpp:338`。
- RT shader 已做 ACES + `pow(1/2.2)` 写入字节（`gn_cornell_rq.comp` 的 `toLinearByte`）。
- `gn2d.frag` 只做 `texture * fragColor`，不把 UNORM 里的 gamma 字节解码回线性；
  输出到 sRGB 交换链时硬件再做一次 linear→sRGB。
- 结果：`gamma(gamma(x))`，中间调明显提亮；白色墙面被推到 255。

## 5. 影响面

- 只影响 RT 预览的图像路径与 HUD；引擎 3D composite 走 HDR→composite→sRGB，路径正确。
- `T/Y/U` 三种预览共用 `toLinearByte` + `DataFill`，都会受二次 sRGB 影响。
- 其他 `sIMAGE` 默认 `vkSrgb=false`，行为不变。

## 6. 引入与检测

- point 模式与 14 的强度随 Cornell 测试程序引入；没有曝光回归测试。
- 2026-11-03 的 sRGB 交换链改动只修了 3D composite，没有同步 2D 层；RT 预览走 2D，
  因此暴露。
- 现有 `cornellRtProbe` 只测 sun 模式均值与非黑，测不到 point 过曝。

## 7. 修复

- 点光强度四路统一为 **3.0**：`cornellCpuRt.cpp:189`、`gn_cornell_rt.comp:176`、
  `gn_cornell_rq.comp:169`、`cornellScene.cpp:177`。
- `sIMAGE` 增加 `bool vkSrgb`（`GN_Vk.h:41`）；`DataFill` / `CreateEmptyToData` 在
  `vkSrgb` 时用 `VK_FORMAT_R8G8B8A8_SRGB`（`vkImageData.cpp:338`），采样硬件解码。
- `GN_VkSwapchainIsSrgb()`（`GN_Vk.cpp:53`）暴露格式判断；Cornell 创建 RT 图像时设置
  `gRtImage->vkSrgb`（`main.cpp:295`），交换链格式变化时 `Release` 后重建。
- 非 sRGB 交换链保持 UNORM 直通，避免反向变暗。
- HUD 每行先画深色阴影再画主色（`cornellHud.cpp:34`），浅色背景下可读。

## 8. 回归防护

- `tests/cornellBox/cornellCpuRtProbe.cpp` 新增 point 模式均值断言：
  `point mean > 205` 失败。intensity=14 时约 225（会失败），3.0 时约 176（通过）。
  纯 CPU，可在无 GPU 的 CI/本机跑。
- 该断言覆盖“把点光强度调回去/调大”的回归；不覆盖 sRGB 纹理格式，后者需要 GPU 窗口验证。

## 9. 验证结果

- `make shaders`：45 个，0 错误。
- `make -j8`：`libGN_mini.so` 重建成功。
- `make cornellBox` / `cornellGpuRtProbe` / `cornellHwRtProbe`：通过。
- `./build/cornellRtProbe`：sun mean≈163/204/174；point mean=176.3，阈值 205 通过。
- `lintCodeModularity`（改动文件）：0 违规 / 0 告警。
- 本会话无 GPU，但用 Mesa llvmpipe 软件 Vulkan 实际跑通了窗口：
  `swapchain format=50 (sRGB)`、point 模式曝光正常（不再纯白）、HUD 可读、
  `DataFill` 持久缓存下纹理显示正常。llvmpipe 只提供 FIFO，不能代表真机 FPS/upload。
  复现：
  ```bash
  DISPLAY=:2 WAYLAND_DISPLAY=wayland-1 \
    VK_ICD_FILENAMES=$PWD/.local/probes/lvp_icd.json \
    CORNELL_RT_CAPTURE=1 CORNELL_RT_HOLD=1 CORNELL_LIGHT=point \
    LD_LIBRARY_PATH=. ./build/cornellBox &
  sleep 8 && grim .local/tmp/point.png && kill %1
  ```
- 真机仍需按 `U` + `K` 看 point 模式与 FPS/upload。

## 10. 后续

- 真机确认：point 模式不再接近纯白；若仍偏亮，先看启动日志的
  `[GN_Vk] swapchain format=... (sRGB/non-sRGB)`，再决定是否调低强度到 2.0–2.5。
- HUD 若仍偏淡，可考虑把 2D 管线整体做 sRGB 解码；本轮只对 RT 图像做了纹理格式修正，
  避免影响所有 UI/图片。
- 金属球是镜面反射，不是几何缺陷；若在真机上形状仍不对，再单独查 AS 法线/实例变换。
