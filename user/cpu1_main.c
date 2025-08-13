/*********************************************************************************************************************
* TC264 Opensourec Library 即（TC264 开源库）是一个基于官方 SDK 接口的第三方开源库
* Copyright (c) 2022 SEEKFREE 逐飞科技
*
* 本文件是 TC264 开源库的一部分
*
* TC264 开源库 是免费软件
* 您可以根据自由软件基金会发布的 GPL（GNU General Public License，即 GNU通用公共许可证）的条款
* 即 GPL 的第3版（即 GPL3.0）或（您选择的）任何后来的版本，重新发布和/或修改它
*
* 本开源库的发布是希望它能发挥作用，但并未对其作任何的保证
* 甚至没有隐含的适销性或适合特定用途的保证
* 更多细节请参见 GPL
*
* 您应该在收到本开源库的同时收到一份 GPL 的副本
* 如果没有，请参阅<https://www.gnu.org/licenses/>
*
* 额外注明：
* 本开源库使用 GPL3.0 开源许可证协议 以上许可申明为译文版本
* 许可申明英文版在 libraries/doc 文件夹下的 GPL3_permission_statement.txt 文件中
* 许可证副本在 libraries 文件夹下 即该文件夹下的 LICENSE 文件
* 欢迎各位使用并传播本程序 但修改内容时必须保留逐飞科技的版权声明（即本声明）
*
* 文件名称          cpu1_main
* 公司名称          成都逐飞科技有限公司
* 版本信息          查看 libraries/doc 文件夹内 version 文件 版本说明
* 开发环境          ADS v1.8.0
* 适用平台          TC264D
* 店铺链接          https://seekfree.taobao.com/
*
* 修改记录
* 日期              作者                备注
* 2022-09-15       pudding            first version
********************************************************************************************************************/

#include "zf_common_headfile.h"
#include "cpu0_main.h"
/*
 * 功能概述
 * - 在 CPU1 初始化并驱动显示屏 IPS200、摄像头 MT9V03X、Flash W25N04。
 * - 支持两种运行方式：
 *   1) 仅显示模式（is_storage = false）：采集摄像头图像并实时显示。
 *   2) 存储/回放模式（is_storage = true）：
 *      初始化存储并擦除 → 采集前 100 帧并压缩写入 → 顺序回放显示 → 再次擦除。
 *
 * 关键变量
 * - is_storage: 是否启用存储与回放流程。
 * - frame_count: 当前存储帧计数（由图像存储模块维护与返回）。
 * - is_compress: 存储模块中的压缩写入标志（在写入阶段置位，用于回放条件判断）。
 *
 * 使用建议
 * - 根据需求设置 is_storage（默认 false）。为 true 时会执行擦除操作，请注意数据安全。
 */
#pragma section all "cpu1_dsram"
// 将本语句与#pragma section all restore语句之间的全局变量都放在CPU1的RAM中


// 工程导入到软件之后，应该选中工程然后点击refresh刷新一下之后再编译
// 工程默认设置为关闭优化，可以自己右击工程选择properties->C/C++ Build->Setting
// 然后在右侧的窗口中找到C/C++ Compiler->Optimization->Optimization level处设置优化等级
// 一般默认新建立的工程都会默认开2级优化，因此大家也可以设置为2级优化

// 对于TC系列默认是不支持中断嵌套的，希望支持中断嵌套需要在中断内使用 enableInterrupts(); 来开启中断嵌套
// 简单点说实际上进入中断后TC系列的硬件自动调用了 disableInterrupts(); 来拒绝响应任何的中断，因此需要我们自己手动调用 enableInterrupts(); 来开启中断的响应。

// ****************************代码区域 ****************************
// 是否启用存储与回放流程（true: 存储+回放；false: 仅显示）
bool is_storage = false;
// 当前已写入/可读取的帧数（通过 get_frame_count() 获取）
uint32 frame_count = 0;
/**
 * @brief CPU1 主函数（初始化外设并执行图像显示/存储/回放流程）
 * 流程：
 * 1. 关闭看门狗并开启全局中断
 * 2. 初始化 IPS200、MT9V03X、W25N04
 * 3. 若 is_storage 为 true，则初始化图像存储并执行一次擦除
 * 4. 主循环：
 *    - 若相机完成一帧且 frame_count < 100：显示图像；如启用存储则压缩写入并更新计数
 *    - 若已达 100 帧且未处于压缩写入阶段：依次读取回放 100 帧并显示，然后清空存储
 */
void core1_main(void)
{
    disable_Watchdog();                     // 关闭看门狗
    interrupt_global_enable(0);             // 打开全局中断
    // 此处编写用户代码 例如外设初始化代码等
    ips200_init(IPS200_TYPE_PARALLEL8);
    mt9v03x_init();
    w25n04_init();
    if(is_storage)
    {
        image_storage_init();
        erase_storage_block();
    }
    // 此处编写用户代码 例如外设初始化代码等
    cpu_wait_event_ready();                 // 等待所有核心初始化完毕
    while (TRUE)
    {
        // 采集阶段：相机完成一帧且未达到 100 帧
        if(mt9v03x_finish_flag && frame_count < 100)
        {
            ips200_displayimage03x(mt9v03x_image[0], MT9V03X_W, MT9V03X_H);
            // 如启用存储，则将当前帧压缩写入到内部缓冲区
            if(is_storage)
            {
                image_compress(mt9v03x_image);
                is_compress = true; // 标记处于压缩写入阶段
            }
            frame_count = get_frame_count(); // 更新当前帧计数
        }
        // 回放阶段：达到 100 帧且不在压缩写入中
        else if(frame_count == 100 && !is_compress)
        {
            for(uint8 i = 0; i < 100; i++)
            {
                read_compressed_image(go);       // 读取下一帧到内部缓冲区
                image_data_analysis();           // 将压缩图像解码到 mt9v03x_image
                ips200_displayimage03x(mt9v03x_image[0], MT9V03X_W, MT9V03X_H);
            }
            frame_count = 0;                     // 重置计数，准备下一轮
            erase_storage_block();               // 擦除存储，释放空间
        }
    }
}
#pragma section all restore
