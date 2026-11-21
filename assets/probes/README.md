# Cornell 探针烘焙资产（GPV4）

由 `tests/cornellBox/cornellProbeBake.cpp` 生成，**不要手改**。

- 网格：24×22×24 = 12672 探针，间距 0.228m，原点 (0.12, 0.12, 0.12)
- 每探针：SH9（36 float）+ 6 面 × 4×4 depth moments（mean/mean²，192 float）
- 文件：`cornell_probe_<lightMode>_24x22x24_4x16.bin`，约 12MB
  - `0` = sun，`1` = point
- 头部：GPV4 magic + counts/origin/spacing/faceSize/samples/lightMode/depthRes
- 加载顺序：`assets/probes/` → `.local/cornellbox/` → 重新烘焙
- 开启：`CORNELL_PROBES=1`；首次若资产缺失会烘焙并写 `.local`

## 重新生成

```bash
make cornellBox
# point
CORNELL_PROBES=1 CORNELL_LIGHT=point CORNELL_RT_CAPTURE=1 LD_LIBRARY_PATH=. ./build/cornellBox
# sun
CORNELL_PROBES=1 CORNELL_LIGHT=sun   CORNELL_RT_CAPTURE=1 LD_LIBRARY_PATH=. ./build/cornellBox
```

把生成的 `.local/cornellbox/probe_cache_<light>_24x22x24_4x16.bin`
复制到本目录并改名为 `cornell_probe_<light>_24x22x24_4x16.bin`。

缓存/资产受 `CORNELL_PROBE_CACHE=0` 影响：设为 0 时完全跳过读写并重新烘焙。
