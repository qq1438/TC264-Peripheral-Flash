/*********************************************************************************************************************
* TC264 Opensourec Library ����TC264 ��Դ�⣩��һ�����ڹٷ� SDK �ӿڵĵ�������Դ��
* Copyright (c) 2022 SEEKFREE ��ɿƼ�
*
* ���ļ��� TC264 ��Դ���һ����
*
* TC264 ��Դ�� ��������
* �����Ը��������������ᷢ���� GPL��GNU General Public License���� GNUͨ�ù������֤��������
* �� GPL �ĵ�3�棨�� GPL3.0������ѡ��ģ��κκ����İ汾�����·�����/���޸���
*
* ����Դ��ķ�����ϣ�����ܷ������ã�����δ�������κεı�֤
* ����û�������������Ի��ʺ��ض���;�ı�֤
* ����ϸ����μ� GPL
*
* ��Ӧ�����յ�����Դ���ͬʱ�յ�һ�� GPL �ĸ���
* ���û�У������<https://www.gnu.org/licenses/>
*
* ����ע����
* ����Դ��ʹ�� GPL3.0 ��Դ���֤Э�� �����������Ϊ���İ汾
* �������Ӣ�İ��� libraries/doc �ļ����µ� GPL3_permission_statement.txt �ļ���
* ���֤������ libraries �ļ����� �����ļ����µ� LICENSE �ļ�
* ��ӭ��λʹ�ò����������� ���޸�����ʱ���뱣����ɿƼ��İ�Ȩ����������������
*
* �ļ�����          cpu1_main
* ��˾����          �ɶ���ɿƼ����޹�˾
* �汾��Ϣ          �鿴 libraries/doc �ļ����� version �ļ� �汾˵��
* ��������          ADS v1.8.0
* ����ƽ̨          TC264D
* ��������          https://seekfree.taobao.com/
*
* �޸ļ�¼
* ����              ����                ��ע
* 2022-09-15       pudding            first version
********************************************************************************************************************/

#include "zf_common_headfile.h"
#include "cpu0_main.h"
/*
 * ���ܸ���
 * - �� CPU1 ��ʼ����������ʾ�� IPS200������ͷ MT9V03X��Flash W25N04��
 * - ֧���������з�ʽ��
 *   1) ����ʾģʽ��is_storage = false�����ɼ�����ͷͼ��ʵʱ��ʾ��
 *   2) �洢/�ط�ģʽ��is_storage = true����
 *      ��ʼ���洢������ �� �ɼ�ǰ 100 ֡��ѹ��д�� �� ˳��ط���ʾ �� �ٴβ�����
 *
 * �ؼ�����
 * - is_storage: �Ƿ����ô洢��ط����̡�
 * - frame_count: ��ǰ�洢֡��������ͼ��洢ģ��ά���뷵�أ���
 * - is_compress: �洢ģ���е�ѹ��д���־����д��׶���λ�����ڻط������жϣ���
 *
 * ʹ�ý���
 * - ������������ is_storage��Ĭ�� false����Ϊ true ʱ��ִ�в�����������ע�����ݰ�ȫ��
 */
#pragma section all "cpu1_dsram"
// ���������#pragma section all restore���֮���ȫ�ֱ���������CPU1��RAM��


// ���̵��뵽���֮��Ӧ��ѡ�й���Ȼ����refreshˢ��һ��֮���ٱ���
// ����Ĭ������Ϊ�ر��Ż��������Լ��һ�����ѡ��properties->C/C++ Build->Setting
// Ȼ�����Ҳ�Ĵ������ҵ�C/C++ Compiler->Optimization->Optimization level�������Ż��ȼ�
// һ��Ĭ���½����Ĺ��̶���Ĭ�Ͽ�2���Ż�����˴��Ҳ��������Ϊ2���Ż�

// ����TCϵ��Ĭ���ǲ�֧���ж�Ƕ�׵ģ�ϣ��֧���ж�Ƕ����Ҫ���ж���ʹ�� enableInterrupts(); �������ж�Ƕ��
// �򵥵�˵ʵ���Ͻ����жϺ�TCϵ�е�Ӳ���Զ������� disableInterrupts(); ���ܾ���Ӧ�κε��жϣ������Ҫ�����Լ��ֶ����� enableInterrupts(); �������жϵ���Ӧ��

// ****************************�������� ****************************
// �Ƿ����ô洢��ط����̣�true: �洢+�طţ�false: ����ʾ��
bool is_storage = false;
// ��ǰ��д��/�ɶ�ȡ��֡����ͨ�� get_frame_count() ��ȡ��
uint32 frame_count = 0;
/**
 * @brief CPU1 ����������ʼ�����貢ִ��ͼ����ʾ/�洢/�ط����̣�
 * ���̣�
 * 1. �رտ��Ź�������ȫ���ж�
 * 2. ��ʼ�� IPS200��MT9V03X��W25N04
 * 3. �� is_storage Ϊ true�����ʼ��ͼ��洢��ִ��һ�β���
 * 4. ��ѭ����
 *    - ��������һ֡�� frame_count < 100����ʾͼ�������ô洢��ѹ��д�벢���¼���
 *    - ���Ѵ� 100 ֡��δ����ѹ��д��׶Σ����ζ�ȡ�ط� 100 ֡����ʾ��Ȼ����մ洢
 */
void core1_main(void)
{
    disable_Watchdog();                     // �رտ��Ź�
    interrupt_global_enable(0);             // ��ȫ���ж�
    // �˴���д�û����� ���������ʼ�������
    ips200_init(IPS200_TYPE_PARALLEL8);
    mt9v03x_init();
    w25n04_init();
    if(is_storage)
    {
        image_storage_init();
        erase_storage_block();
    }
    // �˴���д�û����� ���������ʼ�������
    cpu_wait_event_ready();                 // �ȴ����к��ĳ�ʼ�����
    while (TRUE)
    {
        // �ɼ��׶Σ�������һ֡��δ�ﵽ 100 ֡
        if(mt9v03x_finish_flag && frame_count < 100)
        {
            ips200_displayimage03x(mt9v03x_image[0], MT9V03X_W, MT9V03X_H);
            // �����ô洢���򽫵�ǰ֡ѹ��д�뵽�ڲ�������
            if(is_storage)
            {
                image_compress(mt9v03x_image);
                is_compress = true; // ��Ǵ���ѹ��д��׶�
            }
            frame_count = get_frame_count(); // ���µ�ǰ֡����
        }
        // �طŽ׶Σ��ﵽ 100 ֡�Ҳ���ѹ��д����
        else if(frame_count == 100 && !is_compress)
        {
            for(uint8 i = 0; i < 100; i++)
            {
                read_compressed_image(go);       // ��ȡ��һ֡���ڲ�������
                image_data_analysis();           // ��ѹ��ͼ����뵽 mt9v03x_image
                ips200_displayimage03x(mt9v03x_image[0], MT9V03X_W, MT9V03X_H);
            }
            frame_count = 0;                     // ���ü�����׼����һ��
            erase_storage_block();               // �����洢���ͷſռ�
        }
    }
}
#pragma section all restore
