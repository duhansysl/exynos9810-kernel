
# DS-萤火虫内核适用于 Exynos9810

这是我第一次处理内核，请不要嘲笑。仅在使用 Project Startlet（OneUI 6.1）的 Starlte 上测试过，理论上适用于其他设备，但不保证。我也将脚本翻译成了中文。

从 Duhan 的 Exynos9810 Apollo 内核分叉而来
## 编译
仅在 Deepin 上测试过，但应该适用于任何基于 Debian/Ubuntu 的发行版。

```bash
 git clone https://github.com/mrcxlinux/exynos9810-kernel-yhc
 cd exynos9810-kernel-yhc
 ./apollo.sh
```
## 安装
只需将 IMG 文件刷入“boot”分区，或在 TWRP 中刷入 ZIP 文件。

## 真诚的感谢

- [@ananjaser1211](https://https://github.com/ananjaser1211)
- [@duhansysl](https://github.com/duhansysl)
- [@Florine0928](https://www.github.com/Florine0928)
- [@ExtremeXT](https://github.com/ExtremeXT)
