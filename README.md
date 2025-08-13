## TC264-Peripheral-Flash 图像与参数帧式存储组件

本仓库提供在英飞凌 AURIX TC264 平台上，基于 W25N04KV NAND Flash 的“帧式存储”实现示例：
将图像和调试/控制数据按照“类型标识 + 负载”的记录格式写入页对齐的帧缓冲区，并支持读取回放与按类型解析。

项目仓库与分支：
- 主分支 main 仅保留代码目录 `code`，便于移植集成
- 历史完整快照保留在 `flash-upload` 分支（如需参考集成环境）
- 仓库主页与 PR 入口参见项目地址：[TC264-Peripheral-Flash](https://github.com/qq1438/TC264-Peripheral-Flash)

### 功能特性
- 基于 W25N04KV 的页/块读写与超时管理，支持程序执行与忙状态轮询
- 帧式存储：每帧占用固定页数，对齐写入；提供二分法探测最后有效帧计数
- 图像压缩与还原：将 MT9V03X 原始图像缩放至 `IPCH×IPCW` 后存储，读取时双线性插值还原
- 类型化数据格式：以 1 字节类型头 + 负载的数据段顺序写入并解析，便于扩展自定义段
- 典型多核协作：CPU1 负责采集/压缩，CPU0 负责触发刷写（通过全局标志协调）

## 目录结构
```
code/
  w25n04.h, w25n04.c          // W25N04KV 驱动与指令封装
  image_storage.h, .c         // 帧式存储与图像压缩/解析逻辑
```

## 硬件与连接
- SPI 实例与速率：在 `code/w25n04.h` 中通过以下宏配置
  - `W25N04_SPI`（默认 `SPI_3`）
  - `W25N04_SPI_SPEED`（默认 100MHz，按平台与布线实际下调）
- 引脚定义（默认）：
  - SCK `P22_3`，MOSI `P22_0`，MISO `P22_1`，CS `P22_2`
  - WP `P23_0`（上拉输出），HOLD `P23_1`（上拉输出）
- DMA 通道：`W25N04_DMA_CH = IfxDma_ChannelId_7`（可按工程分配调整）
- 如与既有工程不一致，请在 `w25n04.h` 中修改宏以适配硬件连接

## 软件依赖与编译
- 平台：英飞凌 AURIX TC264（Tasking/ADS 工具链环境均可参考）
- 头文件总入口：`zf_common_headfile.h`（需在工程中可用）
- 必要宏：`MT9V03X_W`、`MT9V03X_H`（摄像头原始图像宽高），`IPCH`、`IPCW`（压缩后高宽）
- OpenMP 指令：`image_storage.c` 中包含 `#pragma omp parallel for`，若工具链不支持可无视或移除

## 数据帧格式
- 每段记录以 1 字节“类型标识”开头，随后紧跟该类型的负载数据；多段顺序排列构成一帧
- 已定义类型（见 `image_storage.h`）：
  - `image_type (0x01)`: 负载为 `IPCH*IPCW` 字节的 8 位灰度图像（行优先）
  - `left_boundary_type (0x02)`: 负载为 `1B step + 2*step 个坐标(y,x)`
  - `right_boundary_type (0x03)`: 同上
- 解析流程：读取 `storage_buffer[address]` 得到类型 → `address += 1` → 按类型解析并推进 `address`
- 新类型扩展：分配唯一类型值，约定负载布局；写入端按“类型 + 负载”追加，解析端在 `switch` 中增加 `case` 并正确推进地址

## 快速开始
### 1) 初始化
```c
// 初始化 W25N04 与存储模块
w25n04_init();
image_storage_init();
```

### 2) 采集与写入（单核示例）
```c
// 将原始图像压缩写入内部缓冲区（仅写入 RAM，不落盘）
image_compress(mt9v03x_image);
// 如需追加参数段，可调用 parameter_compress_* 或 image_compress_boundary
// 最后整帧刷写至 Flash
store_compressed_image();
```

### 3) 读取与回放
```c
// 将下一帧搬运到内部缓冲区
read_compressed_image(go);
// 仅解析图像段并还原到显示缓冲
image_data_analysis();
// 此时可调用屏幕驱动显示 mt9v03x_image
```

### 4) 多核协作（建议方案）
- CPU1：采集帧 → `image_compress(...)` → 置位全局 `is_compress = true`
- CPU0：轮询 `is_compress` → 调用 `store_compressed_image()` 刷写 → 清零 `is_compress`

## API 速览（核心）
- 存储初始化与状态
  - `storage_error_t image_storage_init(void)`：初始化 W25N04 状态，进入空闲
  - `storage_state_t get_storage_state(void)`，`storage_error_t get_last_error(void)`
  - `uint32 get_frame_count(void)`，`void image_read_frame_count(void)`（二分探测最后有效帧）
- 帧写入与读取
  - `void image_compress(uint8 src[MT9V03X_H][MT9V03X_W])`：压缩图像并以 `image_type` 写入缓冲
  - `void image_compress_boundary(uint8 boundary[][2], uint8 step, uint8 type)`：写入边界段
  - `void parameter_compress_float/int/uint8(...)`：写入参数段
  - `storage_error_t store_compressed_image(void)`：整帧刷写，`frame_count++`
  - `storage_error_t read_compressed_image(video_process_t vp)` / `read_compressed_data()`：搬运到缓冲
- 解析
  - `void image_data_analysis(void)`：解析图像段并还原到图像缓冲
  - `void data_analysis(void)`：按类型遍历解析，便于扩展更多段
- W25N04 基础操作（节选，详见 `w25n04.h`）
  - `w25n04_init`、`w25n04_read_page`、`w25n04_write_page`、`w25n04_block_erase`、`w25n04_wait_busy`

## 移植与扩展指南
1) 硬件层
   - 修改 `w25n04.h` 中 SPI 实例、速率与引脚宏以适配实际硬件
   - 如需变更 DMA 通道或中断映射，请同步工程配置
2) 图像尺寸与压缩
   - 在工程公共头文件中正确定义 `MT9V03X_W/H`，按需求调整 `IPCH/IPCW`
3) 自定义数据段
   - 定义唯一类型宏（避免与已用值冲突）
   - 写入端：按“类型 + 负载”追加；解析端：在 `switch` 中增加 `case` 并推进地址
4) 时序与健壮性
   - 写前确保 `w25n04_write_enable()` 成功；操作后轮询 `w25n04_wait_busy()`
   - 根据 `storage_error_t` 做错误处理，必要时擦除重试

## 常见问题（FAQ）
- 读取返回 `STORAGE_END`：表示已到最后一帧，可重置帧号或擦除后重写
- 写入失败：检查写保护、`write_enable`、忙等待与供电稳定性；必要时降频 SPI
- ECC 状态：`w25n04_read_page` 返回码区分无错/可纠错/不可纠错，建议记录并评估介质健康

## 许可与版权
- 当前仓库未附带 License 文件；如需商用或二次分发，请先与作者确认许可条款

## 参考
- 项目仓库与 PR 创建入口：[TC264-Peripheral-Flash](https://github.com/qq1438/TC264-Peripheral-Flash)


