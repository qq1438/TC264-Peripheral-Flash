#ifndef _IMAGE_STORAGE_H_
#define _IMAGE_STORAGE_H_

#include "zf_common_headfile.h"
#include "w25n04.h"

/*
 * 模块概述
 * - 提供基于 W25N04 的“帧式”数据存储方案，支持将图像与参数等数据以“类型标识 + 负载”的方式
 *   写入内部缓冲区，再整帧写入 Flash；读取时再按类型进行解析与还原。
 * - 图像默认来自 MT9V03X，压缩到 IPCH×IPCW 尺寸后保存，减少存储占用。
 *
 * 依赖与移植说明
 * 1) 需要可用的 W25N04 驱动（见 w25n04.h/.c），并提供下列函数/常量：
 *    - 常量：W25N04_DATA_SIZE、W25N04_PAGES_PER_BLOCK、W25N04_TOTAL_PAGES 等
 *    - 函数：w25n04_reset、w25n04_disable_write_protection、w25n04_write_enable、
 *            w25n04_program_data_load、w25n04_program_execute、w25n04_wait_busy、
 *            w25n04_read_page、w25n04_block_erase
 * 2) 需要在工程公共头文件中定义 MT9V03X_W、MT9V03X_H（原始图像宽高）。
 * 3) 可按需要调整 IPCH、IPCW（压缩后尺寸）与 image_storage_page（每帧页数）。
 *
 * 典型使用流程
 * - 初始化：image_storage_init()
 * - 写一帧：先调用 image_compress/parameter_compress_* 依次写入“类型 + 负载”，
 *           再调用 store_compressed_image() 刷写至 Flash
 * - 读一帧：read_compressed_image(video_process) 将一帧搬运到缓冲区，随后执行
 *           image_data_analysis()/data_analysis() 进行解析
 * - 擦除：erase_storage_block()
 *
 * 扩展数据类型
 * - 为新数据分配唯一类型值；写入端以“类型字节 + 负载”顺序写入；
 * - 在解析函数的 switch 中添加 case，确保正确推进 address，避免越界。
 */

// 压缩后的图像尺寸
#define IPCH 60
#define IPCW 80
#define image_storage_page 4
#define STORAGE_BUFFER_SIZE  image_storage_page * W25N04_DATA_SIZE

/*
 * 类型标识与负载格式说明
 *
 * 通用规则
 * - 每段记录以 1 字节“类型标识”开头，随后紧跟该类型的负载数据；
 * - 解析流程：先读取类型 → address += 1 → 按类型解码负载并推进 address；
 * - 写入流程：按“类型字节 + 负载数据”的顺序依次写入内部缓冲区；
 * - 安全性：解析前应确保不会越界访问 STORAGE_BUFFER_SIZE。
 *
 * 1) image_type (0x01)
 *    - 语义：压缩后的 8-bit 灰度图像，尺寸为 IPCH×IPCW；按行优先顺序存储；
 *    - 负载：COMPRESSED_IMAGE_SIZE 字节（等于 IPCH*IPCW）；
 *    - 写入：调用 image_compress(src) 自动完成（内部会先写入 image_type 再写像素数据）；
 *    - 解析：
 *        address += 1;
 *        image_decompress(storage_buffer + address, mt9v03x_image);
 *        address += COMPRESSED_IMAGE_SIZE;
 *
 * 2) left_boundary_type (0x02)
 *    - 语义：左边界的若干个坐标点，坐标存储顺序为 (y, x)；
 *    - 负载：1B step（点数 N） + 2N 字节坐标序列（每点 2 字节：y 后 x，均为 uint8）；
 *    - 写入：image_compress_boundary(boundary, step, left_boundary_type)；
 *    - 解析：
 *        address += 1;
 *        address = image_decompress_boundary(boundary, address);
 *
 * 3) right_boundary_type (0x03)
 *    - 语义：右边界的若干个坐标点，格式同 left_boundary_type；
 *    - 负载：1B step + 2N 字节坐标序列；
 *    - 写入：image_compress_boundary(boundary, step, right_boundary_type)；
 *    - 解析：
 *        address += 1;
 *        address = image_decompress_boundary(boundary, address);
 *
 * 扩展类型定义指南
 * - 为新类型分配唯一值（建议避免与 0x01~0x03 冲突，可自定义在更高的编号范围）；
 * - 明确负载布局（长度、字段顺序与类型），并在注释中记录；
 * - 写入端：按“类型 + 负载”顺序追加到缓冲区；
 * - 解析端：在 data_analysis()/image_data_analysis() 的 switch 中添加 case，并正确推进 address；
 * - 建议为自定义结构编写 image_decompress_xxx()/parameter_compress_xxx() 以统一读写逻辑。
 */
#define image_type 0x01
#define left_boundary_type 0x02
#define right_boundary_type 0x03

typedef enum
{
    go = 0,
    back = 1,
    go_go = 2,
    back_back = 3
}video_process_t;

// 存储系统状态码
typedef enum {
    STORAGE_IDLE = 0,        // 空闲状态
    STORAGE_WRITING = 1,     // 正在写入
    STORAGE_READING = 2,     // 正在读取
    STORAGE_ERROR = 3        // 错误状态
} storage_state_t;

// 错误码定义
typedef enum {
    STORAGE_OK = 0,          // 操作成功
    STORAGE_INIT_FAILED,     // 初始化失败
    STORAGE_WRITE_FAILED,    // 写入失败
    STORAGE_READ_FAILED,     // 读取失败
    STORAGE_END,             // 读取结束
    STORAGE_BUSY,           // 设备忙
    STORAGE_INVALID_PARAM   // 无效参数
} storage_error_t;

// 存储系统配置结构体
typedef struct {
    uint32 frame_count;    // 帧数
    uint32 current_num;   // 当前读写地址
    storage_state_t state;   // 当前状态
    storage_error_t error;   // 错误码
} storage_config_t;

extern bool is_compress;

/**
 * @brief 初始化图像存储系统
 * @return storage_error_t 错误码
 * 说明：完成 W25N04 复位与解除写保护，成功后状态置为 STORAGE_IDLE。
 */
storage_error_t image_storage_init(void);

/**
 * @brief 图像压缩（区域采样/插值缩放至 IPCH×IPCW）并写入缓冲区
 * @param src 源图像数据指针（MT9V03X_H × MT9V03X_W）
 * 说明：仅写入内部缓冲区且以 image_type 作为段头；不直接写入 Flash。
 */
void image_compress(uint8 src[MT9V03X_H][MT9V03X_W]);

/**
 * @brief 压缩写入边界数据段
 * @param boundary 边界数组，格式为 step×2 的坐标对（y, x）
 * @param step 坐标点数量
 * @param type 段类型（如 left_boundary_type/right_boundary_type）
 * 说明：以“type + step + 坐标序列”的格式追加到内部缓冲区。
 */ 
void image_compress_boundary(uint8 boundary[][2],uint8 step,uint8 type);

/**
 * @brief 参数压缩（float）
 * @param parameter 参数值（float）
 * @param parameter_type 段类型
 * 说明：写入格式为“parameter_type + 4B IEEE754（大端序）”。
 */
void parameter_compress_float(float parameter,uint8 parameter_type);

/**
 * @brief 参数压缩（int）
 * @param parameter 参数值（int）
 * @param parameter_type 段类型
 * 说明：写入格式为“parameter_type + 4B 大端序整数”。
 */
void parameter_compress_int(int parameter,uint8 parameter_type);

/**
 * @brief 参数压缩（uint8）
 * @param parameter 参数值（uint8）
 * @param parameter_type 段类型
 */
void parameter_compress_uint8(uint8 parameter,uint8 parameter_type);

/**
 * @brief 将内部缓冲区内容作为“一帧”写入 Flash
 * @return storage_error_t 错误码
 * 前置：需已按序写入若干“类型 + 负载”段；成功后 frame_count++。
 */
storage_error_t store_compressed_image(void);

/**
 * @brief 读取一帧压缩数据到内部缓冲区
 * @param video_process 帧序移动策略（go/back/go_go/back_back）
 * @return storage_error_t 错误码
 * 说明：读取后可调用 image_data_analysis()/data_analysis() 进行解码。
 */
storage_error_t read_compressed_image(video_process_t video_process);

/**
 * @brief 读取一帧压缩数据（仅搬运，不解析）
 * 返回 STORAGE_END 表示已无更多帧。
 */
storage_error_t read_compressed_data(void);

/**
 * @brief 数据分析（解析缓冲区内各类段）
 * 说明：遍历“类型 + 负载”记录，按业务需要进行解析与还原，可扩展更多类型。
 */
void data_analysis(void);

/**
 * @brief 图像数据分析（仅处理图像段）
 * 说明：将图像段解码并还原到图像缓冲区。
 */
void image_data_analysis(void);


/**
 * @brief 获取当前帧数
 * @return uint32_t 当前帧数
 */
uint32 get_frame_count(void);

/**
 * @brief 擦除存储块（覆盖现有帧）
 * @return storage_error_t 错误码
 * 注意：仅在 STORAGE_IDLE 状态下调用；操作耗时较长。
 */
storage_error_t erase_storage_block(void);

/**
 * @brief 探测 flash 中已写入的帧数
 * 说明：通过二分搜索定位最后一帧的起始页，掉电后可恢复 frame_count。
 */
void image_read_frame_count(void);


#endif /* _IMAGE_STORAGE_H_ */
