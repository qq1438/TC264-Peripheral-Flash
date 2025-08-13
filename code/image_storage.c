#include "image_storage.h"
#include "zf_common_headfile.h"
#include <string.h>

bool is_compress = false;

// 图像数据存储相关定义
#define IMAGE_SIZE             (MT9V03X_W * MT9V03X_H)  // 一帧图像大小
#define COMPRESSED_IMAGE_SIZE  (IPCH * IPCW)  // 压缩后图像大小
#define COMPRESSED_PAGES_PER_FRAME ((COMPRESSED_IMAGE_SIZE + W25N04_DATA_SIZE - 1) / W25N04_DATA_SIZE)  // 压缩后每帧所需页数

// 定点数计算相关定义
#define FIXED_POINT_BITS 8
#define FIXED_POINT_SCALE (1 << FIXED_POINT_BITS)

// 临时存储缓冲区
static uint8 storage_buffer[STORAGE_BUFFER_SIZE];
storage_config_t storage_config;

/*
 * 图像压缩与数据写入接口
 * - image_compress: 将原始图像缩放至 IPCH×IPCW，并以 image_type 开头写入内部缓冲区
 * - image_compress_boundary/parameter_compress_*: 以“类型 + 负载”的方式追加其他段
 * 注意：上述接口仅写入到内部缓冲区，需调用 store_compressed_image() 才会真正写入 Flash。
 */
// 图像压缩函数
void image_compress(uint8 src[MT9V03X_H][MT9V03X_W])
{
    memset(storage_buffer, 0xff, STORAGE_BUFFER_SIZE);
    storage_config.current_num = 0;
    storage_buffer[storage_config.current_num] = image_type;
    storage_config.current_num++;
    const int32_t scale_h = ((int32_t)(MT9V03X_H << FIXED_POINT_BITS)) / IPCH;
    const int32_t scale_w = ((int32_t)(MT9V03X_W << FIXED_POINT_BITS)) / IPCW;
    
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < IPCH; i++)
    {
        for (int j = 0; j < IPCW; j++)
        {
            int32_t y = (i * scale_h + (scale_h >> 1)) >> FIXED_POINT_BITS;
            int32_t x = (j * scale_w + (scale_w >> 1)) >> FIXED_POINT_BITS;
            
            if (y >= MT9V03X_H) y = MT9V03X_H - 1;
            if (x >= MT9V03X_W) x = MT9V03X_W - 1;
            
            storage_buffer[storage_config.current_num] = src[y][x];
            storage_config.current_num++;
        }
    }
}

void image_compress_boundary(uint8 boundary[][2],uint8 step,uint8 type)
{
    storage_buffer[storage_config.current_num] = type;
    storage_config.current_num++;
    storage_buffer[storage_config.current_num] = step;
    storage_config.current_num++;
    for(uint8 i = 0; i < step; i++)
    {
        storage_buffer[storage_config.current_num] = boundary[i][0];
        storage_config.current_num++;
        storage_buffer[storage_config.current_num] = boundary[i][1];
        storage_config.current_num++;
    }
}

void parameter_compress_float(float parameter,uint8 parameter_type)
{
    storage_buffer[storage_config.current_num] = parameter_type;
    storage_config.current_num++;
    uint32_t *p = (uint32_t *)&parameter;
    storage_buffer[storage_config.current_num] = (*p >> 24) & 0xFF;
    storage_config.current_num++;
    storage_buffer[storage_config.current_num] = (*p >> 16) & 0xFF; 
    storage_config.current_num++;
    storage_buffer[storage_config.current_num] = (*p >> 8) & 0xFF;
    storage_config.current_num++;
    storage_buffer[storage_config.current_num] = *p & 0xFF;
    storage_config.current_num++;
}

void parameter_compress_uint8(uint8 parameter,uint8 parameter_type)
{
    storage_buffer[storage_config.current_num] = parameter_type;
    storage_config.current_num++;
    storage_buffer[storage_config.current_num] = parameter;
    storage_config.current_num++;
}

void parameter_compress_int(int parameter, uint8 parameter_type)
{
    storage_buffer[storage_config.current_num] = parameter_type;
    storage_config.current_num++;
    storage_buffer[storage_config.current_num] = (parameter >> 24) & 0xFF;
    storage_config.current_num++;
    storage_buffer[storage_config.current_num] = (parameter >> 16) & 0xFF;
    storage_config.current_num++;
    storage_buffer[storage_config.current_num] = (parameter >> 8) & 0xFF;
    storage_config.current_num++;
    storage_buffer[storage_config.current_num] = parameter & 0xFF;
    storage_config.current_num++;
}


// 图像还原函数
void image_decompress(uint8 *src, uint8 dst[MT9V03X_H][MT9V03X_W])
{
    const int32_t scale_h = ((int32_t)(IPCH << FIXED_POINT_BITS)) / MT9V03X_H;
    const int32_t scale_w = ((int32_t)(IPCW << FIXED_POINT_BITS)) / MT9V03X_W;

    #pragma omp parallel for collapse(2)
    for (int i = 0; i < MT9V03X_H; i++)
    {
        for (int j = 0; j < MT9V03X_W; j++)
        {
            int32_t y = (i * scale_h);
            int32_t x = (j * scale_w);

            int y_int = y >> FIXED_POINT_BITS;
            int x_int = x >> FIXED_POINT_BITS;

            if (y_int >= IPCH - 1) y_int = IPCH - 2;
            if (x_int >= IPCW - 1) x_int = IPCW - 2;

            int32_t wy = y & (FIXED_POINT_SCALE - 1);
            int32_t wx = x & (FIXED_POINT_SCALE - 1);

            uint8 p00 = src[y_int * IPCW + x_int];
            uint8 p01 = src[y_int * IPCW + x_int + 1];
            uint8 p10 = src[(y_int + 1) * IPCW + x_int];
            uint8 p11 = src[(y_int + 1) * IPCW + x_int + 1];

            int32_t col0 = p00 * (FIXED_POINT_SCALE - wx) + p01 * wx;
            int32_t col1 = p10 * (FIXED_POINT_SCALE - wx) + p11 * wx;
            
            int32_t value = col0 * (FIXED_POINT_SCALE - wy) + col1 * wy;
            
            dst[i][j] = ((value + (FIXED_POINT_SCALE * FIXED_POINT_SCALE / 2)) 
                         >> (FIXED_POINT_BITS * 2));
        }
    }
}

uint32 image_decompress_boundary(uint8 boundary[][2],uint32 address)
{
    uint8 step = storage_buffer[address];
    address++;
    for(uint8 i = 0; i < step; i++)
    {
        boundary[i][0] = storage_buffer[address];
        address++;
        boundary[i][1] = storage_buffer[address];
        address++;
    }
    return address;
}


uint32 image_decompress_float(float *error,uint32 address)
{
    uint32_t *p = (uint32_t *)error;
    *p = (storage_buffer[address] << 24) | (storage_buffer[address + 1] << 16) | (storage_buffer[address + 2] << 8) | storage_buffer[address + 3];
    address += 4;
    return address;
}

uint32 image_decompress_uint8(uint8 *parameter,uint32 address)
{
    *parameter = storage_buffer[address];
    address++;
    return address;
}

uint32 image_decompress_int(int *parameter, uint32 address)
{
    *parameter = (storage_buffer[address] << 24) | (storage_buffer[address + 1] << 16) | 
                 (storage_buffer[address + 2] << 8) | storage_buffer[address + 3];
    address += 4;
    return address;
}


/*
 * 存储系统初始化
 * - 完成 W25N04 复位与解除写保护，设置状态与计数。
 * - 失败时设置错误码与错误状态，调用者可通过 get_last_error() 获取原因。
 */
// 初始化存储系统
storage_error_t image_storage_init(void)
{
    // 复位设备
    if (!w25n04_reset(1))
    {
        storage_config.error = STORAGE_INIT_FAILED;
        storage_config.state = STORAGE_ERROR;
        return STORAGE_INIT_FAILED;
    }
    
    // 解除写保护
    if (!w25n04_disable_write_protection())
    {
        storage_config.error = STORAGE_INIT_FAILED;
        storage_config.state = STORAGE_ERROR;
        return STORAGE_INIT_FAILED;
    }
    
    storage_config.state = STORAGE_IDLE;
    storage_config.error = STORAGE_OK;
    storage_config.current_num = 0;
    storage_config.frame_count = 0;
    
    return STORAGE_OK;
}

/*
 * 存储压缩图像
 * - 将内部缓冲区的数据按页写入 Flash。
 * - 写入完成后 frame_count++，状态置为 IDLE。
 * - 若 current_num 为 0，返回 STORAGE_INVALID_PARAM。
 */
// 存储压缩图像
storage_error_t store_compressed_image(void)
{
    if (storage_config.state == STORAGE_WRITING || storage_config.state == STORAGE_READING) return STORAGE_BUSY;
    
    // 检查是否有数据需要写入
    if (storage_config.current_num == 0) {
        storage_config.error = STORAGE_INVALID_PARAM;
        return STORAGE_INVALID_PARAM;  // 不允许写入空数据
    }
    
    storage_config.state = STORAGE_WRITING;
    
    // 写入数据
    for (uint16 i = 0; i < storage_config.current_num; i += W25N04_DATA_SIZE)
    {
        if (!w25n04_write_enable())
        {
            storage_config.error = STORAGE_WRITE_FAILED;
            storage_config.state = STORAGE_ERROR;
            return STORAGE_WRITE_FAILED;
        }
        
        if (!w25n04_program_data_load(0, storage_buffer + i, W25N04_DATA_SIZE))
        {
            storage_config.error = STORAGE_WRITE_FAILED;
            storage_config.state = STORAGE_ERROR;
            return STORAGE_WRITE_FAILED;
        }
        
        uint8 exec_result = w25n04_program_execute((storage_config.frame_count * STORAGE_BUFFER_SIZE + i) / W25N04_DATA_SIZE);
        if (exec_result != 1)
        {
            storage_config.error = STORAGE_WRITE_FAILED;
            storage_config.state = STORAGE_ERROR;
            return STORAGE_WRITE_FAILED;
        }
        
        if (!w25n04_wait_busy(20))
        {
            storage_config.error = STORAGE_WRITE_FAILED;
            storage_config.state = STORAGE_ERROR;   
            return STORAGE_WRITE_FAILED;
        }
    }
    
    storage_config.frame_count++;
    storage_config.state = STORAGE_IDLE;
    return STORAGE_OK;
}

/*
 * 读取压缩图像
 * - 将指定帧搬运到内部缓冲区，可通过 video_process 控制帧号自增/跳转。
 * - 读取完成后可调用 image_data_analysis()/data_analysis() 解析。
 */
// 读取压缩图像
uint32 video_frame = 0;
storage_error_t read_compressed_image(video_process_t video_process)
{
    if (storage_config.state == STORAGE_WRITING || storage_config.state == STORAGE_READING) return STORAGE_BUSY;
    if (storage_config.frame_count == 0) return STORAGE_OK;  // 没有存储的帧
    
    storage_config.state = STORAGE_READING;
    
        // 读取压缩数据
    for (uint16 i = 0; i < STORAGE_BUFFER_SIZE; i += W25N04_DATA_SIZE)
    {
        if (!w25n04_read_page((video_frame * STORAGE_BUFFER_SIZE + i) / W25N04_DATA_SIZE, 0, storage_buffer + i, W25N04_DATA_SIZE))
        {
            storage_config.error = STORAGE_READ_FAILED;
            storage_config.state = STORAGE_ERROR;
            return STORAGE_READ_FAILED;
        }
    }
    switch(video_process) {
        case go:
            video_frame = video_frame + 1;
            break;
        case go_go:
            video_frame = video_frame + 20;
            break;
        case back_back:
            video_frame = video_frame > 20 ? video_frame - 20 : 0;
            break;
        case back:
            video_frame = video_frame > 0 ? video_frame - 1 : 0;
            break;
        default:
            break;
    }
    video_frame = video_frame > storage_config.frame_count - 1 ? storage_config.frame_count - 1 : video_frame;
    storage_config.state = STORAGE_IDLE;
    return STORAGE_OK;
}

storage_error_t read_compressed_data()
{
    if (storage_config.state == STORAGE_WRITING || storage_config.state == STORAGE_READING) return STORAGE_BUSY;
    if (storage_config.frame_count == 0) return STORAGE_OK;  // 没有存储的帧
    
    storage_config.state = STORAGE_READING;
    
        // 读取压缩数据
    for (uint16 i = 0; i < STORAGE_BUFFER_SIZE; i += W25N04_DATA_SIZE)
    {
        if (!w25n04_read_page((video_frame * STORAGE_BUFFER_SIZE + i) / W25N04_DATA_SIZE, 0, storage_buffer + i, W25N04_DATA_SIZE))
        {
            storage_config.error = STORAGE_READ_FAILED;
            storage_config.state = STORAGE_ERROR;
            return STORAGE_READ_FAILED;
        }
    }
    video_frame++;
    if(video_frame==storage_config.frame_count)return STORAGE_END;
    storage_config.state = STORAGE_IDLE;
    return STORAGE_OK;
}

/*
 * 图像数据分析
 * - 遍历缓冲区内记录，仅处理 image_type 对应的数据，将其还原到图像缓冲区。
 * - 扩展解析请参考此函数中部的注释块说明与 data_analysis() 的写法。
 */
void image_data_analysis(void)
{
    uint32 address = 0;
    while(address < STORAGE_BUFFER_SIZE)
    {
        switch(storage_buffer[address])
        {
            case image_type:
                address += 1;
                image_decompress(storage_buffer + address, mt9v03x_image);
                address += COMPRESSED_IMAGE_SIZE;
                break;

            /*
            数据存储格式说明：每段数据以1字节“类型标识”开头，随后是该类型对应的负载数据。
            解析流程：读取 storage_buffer[address] 获取类型 → address 前进 1 → 按类型解码负载并推进 address。

            新增一种数据解析的步骤：
            1) 定义新的类型标识常量（如 my_type），确保与现有类型不冲突。
            2) 在写入端按“类型字节 + 负载数据”的顺序写入，可复用 parameter_compress_* 或自定义序列化。
            3) 在此处的 switch 中增加分支，并保证 address 正确前移。例如：
               case my_type:
                   address += 1;                       // 跳过类型字节
                   // 若是自定义结构，调用解码函数并返回新的 address：
                   // address = image_decompress_xxx(&target, address);
                   // 若为定长数据，可直接拷贝并手动推进 address：
                   // memcpy(&target, &storage_buffer[address], N);
                   // address += N;
                   break;

            注意：
            - 每解析一段数据后，必须将 address 推进到下一段起始位置；
            - 若没有对应类型或数据不完整，应避免越界访问，可走 default 分支结束解析；
            - 可参考下方 data_analysis() 中对多种类型的完整解析示例。
            */
                
            default:
                address += STORAGE_BUFFER_SIZE;
                break;
        }
    }
}

/*
 * 通用数据分析
 * - 遍历缓冲区内“类型 + 负载”记录，根据类型推进 address 并执行相应解码。
 * - 可在 switch 中添加更多类型，以扩展参数/标记等数据的解析。
 */
void data_analysis(void)
{
    uint32 address = 0;
    while(address < STORAGE_BUFFER_SIZE)
    {
        switch(storage_buffer[address])
        {
            case image_type:
                address += 1;
                address += COMPRESSED_IMAGE_SIZE;
                break;

            default:
                address = STORAGE_BUFFER_SIZE;
        }
    }
}

// 获取存储状态
storage_state_t get_storage_state(void)
{
    return storage_config.state;
}

// 获取最后一次错误
storage_error_t get_last_error(void)
{
    return storage_config.error;
}

// 获取当前帧数
uint32 get_frame_count(void)
{
    return storage_config.frame_count;
}

/*
 * 擦除存储块
 * - 仅在 STORAGE_IDLE 状态下允许；按帧分布计算需擦除的块数。
 * - 操作耗时，避免在实时路径调用。
 */
storage_error_t erase_storage_block(void)
{
    // 仅在空闲状态下允许擦除
    if (storage_config.state != STORAGE_IDLE) return STORAGE_BUSY;

    // 先读取当前已存帧数
    image_read_frame_count();

    uint32 total_frames = storage_config.frame_count;

    // 如果没有数据，直接返回
    if (total_frames == 0) {
        return STORAGE_OK;
    }

    // 计算每个块可容纳的帧数
    const uint32 frames_per_block = W25N04_PAGES_PER_BLOCK / image_storage_page; // 64/4 = 16

    // 计算需要擦除的块数量（向上取整）
    uint32 blocks_to_erase = (total_frames + frames_per_block - 1) / frames_per_block;

    for (uint16 block = 0; block < blocks_to_erase; block++)
    {
        if (w25n04_block_erase(block) != 1)
        {
            storage_config.error = STORAGE_WRITE_FAILED;
            storage_config.state = STORAGE_ERROR;
            return STORAGE_WRITE_FAILED;
        }

        // 等待擦除完成
        if (w25n04_wait_busy(100) == 0)
        {
            storage_config.error = STORAGE_WRITE_FAILED;
            storage_config.state = STORAGE_ERROR;
            return STORAGE_WRITE_FAILED;
        }
    }

    // 更新状态信息
    storage_config.frame_count = 0;
    storage_config.error = STORAGE_OK;
    return STORAGE_OK;
}

/*
 * 读取 Flash 中的帧数
 * - 通过二分查找定位最后有效帧，掉电后可用于恢复 frame_count。
 */
void image_read_frame_count(void)
{
    uint32 left = 0;
    uint32 right = W25N04_TOTAL_PAGES / image_storage_page;  // 总帧数
    uint32 last_valid_frame = 0;
    uint8 temp_buffer[W25N04_DATA_SIZE];
    bool is_all_ff;

    if (storage_config.state != STORAGE_IDLE) return;
    
    storage_config.state = STORAGE_READING;
    
    // 使用二分查找找到最后一个非全0xFF的帧
    while (left <= right)
    {
        uint32 mid = left + (right - left) / 2;
        uint32 frame_start_page = mid * image_storage_page;  // 当前帧的起始页
        
        // 读取该帧的第一页
        if (!w25n04_read_page(frame_start_page, 0, temp_buffer, W25N04_DATA_SIZE))
        {
            storage_config.error = STORAGE_READ_FAILED;
            storage_config.state = STORAGE_ERROR;
            return;
        }
        
        // 检查该页是否全为0xFF
        is_all_ff = true;
        for (uint16 i = 0; i < W25N04_DATA_SIZE; i++)
        {
            if (temp_buffer[i] != 0xFF)
            {
                is_all_ff = false;
                break;
            }
        }
        
        if (is_all_ff)
        {
            right = mid - 1;
        }
        else
        {
            last_valid_frame = mid;
            left = mid + 1;
        }
    }
    
    // 处理结果
    if (last_valid_frame == 0 && is_all_ff)
    {
        // 检查第一帧是否为有效帧
        if (!w25n04_read_page(0, 0, temp_buffer, W25N04_DATA_SIZE))
        {
            storage_config.error = STORAGE_READ_FAILED;
            storage_config.state = STORAGE_ERROR;
            return;
        }
        
        is_all_ff = true;
        for (uint16 i = 0; i < W25N04_DATA_SIZE; i++)
        {
            if (temp_buffer[i] != 0xFF)
            {
                is_all_ff = false;
                break;
            }
        }
        
        if (is_all_ff)
        {
            storage_config.frame_count = 0;
        }
        else
        {
            storage_config.frame_count = 1;
        }
    }
    else
    {
        storage_config.frame_count = last_valid_frame - 1;
    }
    
    storage_config.state = STORAGE_IDLE;
}
