# 扎金花

基于 WeaveForge 本地工作区创建的游戏项目。

## 项目概览

单人三键扎金花，面向 FoloToy AI Passport（ESP32-C3、8 MB Flash）。对局包含三位 AI 对手，结算时展示四家的手牌。主页提供战绩、规则和独立待机时钟。

固件使用 ESP-IDF 5.5.3 和 LVGL 9。激活 ESP-IDF 后，运行 `bash tools/validate.sh --firmware` 构建并校验镜像。

## 素材维护

- `assets/原始素材/` 用于存放 WeaveForge 中可编辑的美术和音效节点。
- `assets/images/`、`assets/sounds/` 和 `assets/fonts/` 保存固件转换脚本读取的素材文件。
- `main/generated/` 保存由脚本生成的 LVGL 图像、PCM 音效和中文字库。ESP-IDF 直接编译这里的 C 文件。

在 WeaveForge 更新素材后，把要用于固件的版本导出到对应的 `assets/` 子目录，覆盖同名输入，再按改动运行转换脚本。

```bash
# 更换图像或音效后执行，需要 ffmpeg
python3 tools/zjh_assets.py

# 更换字体或新增中文界面文字后执行，需要 Node.js 和 lv_font_conv
python3 tools/gen_zjh_fonts.py
python3 tools/check_glyphs.py

# 在激活 ESP-IDF 5.5.3 后构建并验证固件
bash tools/validate.sh --firmware
```

Noto Sans SC 和 ZCOOL KuaiLe 字体及其生成字库使用 SIL Open Font License 1.1。许可证见 [Noto Sans SC](./assets/fonts/NotoSansSC-OFL.txt) 和 [ZCOOL KuaiLe](./assets/fonts/ZCOOLKuaiLe-OFL.txt)。

Noto Sans SC 和 ZCOOL KuaiLe 字体及其生成字库使用 SIL Open Font License 1.1。许可证见 [Noto Sans SC](./assets/fonts/NotoSansSC-OFL.txt) 和 [ZCOOL KuaiLe](./assets/fonts/ZCOOLKuaiLe-OFL.txt)。

AI 协作约定见 [AGENTS.md](./AGENTS.md)。
