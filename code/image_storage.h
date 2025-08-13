#ifndef _IMAGE_STORAGE_H_
#define _IMAGE_STORAGE_H_

#include "zf_common_headfile.h"
#include "w25n04.h"

/*
 * ģ�����
 * - �ṩ���� W25N04 �ġ�֡ʽ�����ݴ洢������֧�ֽ�ͼ��������������ԡ����ͱ�ʶ + ���ء��ķ�ʽ
 *   д���ڲ�������������֡д�� Flash����ȡʱ�ٰ����ͽ��н����뻹ԭ��
 * - ͼ��Ĭ������ MT9V03X��ѹ���� IPCH��IPCW �ߴ�󱣴棬���ٴ洢ռ�á�
 *
 * ��������ֲ˵��
 * 1) ��Ҫ���õ� W25N04 �������� w25n04.h/.c�������ṩ���к���/������
 *    - ������W25N04_DATA_SIZE��W25N04_PAGES_PER_BLOCK��W25N04_TOTAL_PAGES ��
 *    - ������w25n04_reset��w25n04_disable_write_protection��w25n04_write_enable��
 *            w25n04_program_data_load��w25n04_program_execute��w25n04_wait_busy��
 *            w25n04_read_page��w25n04_block_erase
 * 2) ��Ҫ�ڹ��̹���ͷ�ļ��ж��� MT9V03X_W��MT9V03X_H��ԭʼͼ���ߣ���
 * 3) �ɰ���Ҫ���� IPCH��IPCW��ѹ����ߴ磩�� image_storage_page��ÿ֡ҳ������
 *
 * ����ʹ������
 * - ��ʼ����image_storage_init()
 * - дһ֡���ȵ��� image_compress/parameter_compress_* ����д�롰���� + ���ء���
 *           �ٵ��� store_compressed_image() ˢд�� Flash
 * - ��һ֡��read_compressed_image(video_process) ��һ֡���˵������������ִ��
 *           image_data_analysis()/data_analysis() ���н���
 * - ������erase_storage_block()
 *
 * ��չ��������
 * - Ϊ�����ݷ���Ψһ����ֵ��д����ԡ������ֽ� + ���ء�˳��д�룻
 * - �ڽ��������� switch ����� case��ȷ����ȷ�ƽ� address������Խ�硣
 */

// ѹ�����ͼ��ߴ�
#define IPCH 60
#define IPCW 80
#define image_storage_page 4
#define STORAGE_BUFFER_SIZE  image_storage_page * W25N04_DATA_SIZE

/*
 * ���ͱ�ʶ�븺�ظ�ʽ˵��
 *
 * ͨ�ù���
 * - ÿ�μ�¼�� 1 �ֽڡ����ͱ�ʶ����ͷ�������������͵ĸ������ݣ�
 * - �������̣��ȶ�ȡ���� �� address += 1 �� �����ͽ��븺�ز��ƽ� address��
 * - д�����̣����������ֽ� + �������ݡ���˳������д���ڲ���������
 * - ��ȫ�ԣ�����ǰӦȷ������Խ����� STORAGE_BUFFER_SIZE��
 *
 * 1) image_type (0x01)
 *    - ���壺ѹ����� 8-bit �Ҷ�ͼ�񣬳ߴ�Ϊ IPCH��IPCW����������˳��洢��
 *    - ���أ�COMPRESSED_IMAGE_SIZE �ֽڣ����� IPCH*IPCW����
 *    - д�룺���� image_compress(src) �Զ���ɣ��ڲ�����д�� image_type ��д�������ݣ���
 *    - ������
 *        address += 1;
 *        image_decompress(storage_buffer + address, mt9v03x_image);
 *        address += COMPRESSED_IMAGE_SIZE;
 *
 * 2) left_boundary_type (0x02)
 *    - ���壺��߽�����ɸ�����㣬����洢˳��Ϊ (y, x)��
 *    - ���أ�1B step������ N�� + 2N �ֽ��������У�ÿ�� 2 �ֽڣ�y �� x����Ϊ uint8����
 *    - д�룺image_compress_boundary(boundary, step, left_boundary_type)��
 *    - ������
 *        address += 1;
 *        address = image_decompress_boundary(boundary, address);
 *
 * 3) right_boundary_type (0x03)
 *    - ���壺�ұ߽�����ɸ�����㣬��ʽͬ left_boundary_type��
 *    - ���أ�1B step + 2N �ֽ��������У�
 *    - д�룺image_compress_boundary(boundary, step, right_boundary_type)��
 *    - ������
 *        address += 1;
 *        address = image_decompress_boundary(boundary, address);
 *
 * ��չ���Ͷ���ָ��
 * - Ϊ�����ͷ���Ψһֵ����������� 0x01~0x03 ��ͻ�����Զ����ڸ��ߵı�ŷ�Χ����
 * - ��ȷ���ز��֣����ȡ��ֶ�˳�������ͣ�������ע���м�¼��
 * - д��ˣ��������� + ���ء�˳��׷�ӵ���������
 * - �����ˣ��� data_analysis()/image_data_analysis() �� switch ����� case������ȷ�ƽ� address��
 * - ����Ϊ�Զ���ṹ��д image_decompress_xxx()/parameter_compress_xxx() ��ͳһ��д�߼���
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

// �洢ϵͳ״̬��
typedef enum {
    STORAGE_IDLE = 0,        // ����״̬
    STORAGE_WRITING = 1,     // ����д��
    STORAGE_READING = 2,     // ���ڶ�ȡ
    STORAGE_ERROR = 3        // ����״̬
} storage_state_t;

// �����붨��
typedef enum {
    STORAGE_OK = 0,          // �����ɹ�
    STORAGE_INIT_FAILED,     // ��ʼ��ʧ��
    STORAGE_WRITE_FAILED,    // д��ʧ��
    STORAGE_READ_FAILED,     // ��ȡʧ��
    STORAGE_END,             // ��ȡ����
    STORAGE_BUSY,           // �豸æ
    STORAGE_INVALID_PARAM   // ��Ч����
} storage_error_t;

// �洢ϵͳ���ýṹ��
typedef struct {
    uint32 frame_count;    // ֡��
    uint32 current_num;   // ��ǰ��д��ַ
    storage_state_t state;   // ��ǰ״̬
    storage_error_t error;   // ������
} storage_config_t;

extern bool is_compress;

/**
 * @brief ��ʼ��ͼ��洢ϵͳ
 * @return storage_error_t ������
 * ˵������� W25N04 ��λ����д�������ɹ���״̬��Ϊ STORAGE_IDLE��
 */
storage_error_t image_storage_init(void);

/**
 * @brief ͼ��ѹ�����������/��ֵ������ IPCH��IPCW����д�뻺����
 * @param src Դͼ������ָ�루MT9V03X_H �� MT9V03X_W��
 * ˵������д���ڲ����������� image_type ��Ϊ��ͷ����ֱ��д�� Flash��
 */
void image_compress(uint8 src[MT9V03X_H][MT9V03X_W]);

/**
 * @brief ѹ��д��߽����ݶ�
 * @param boundary �߽����飬��ʽΪ step��2 ������ԣ�y, x��
 * @param step ���������
 * @param type �����ͣ��� left_boundary_type/right_boundary_type��
 * ˵�����ԡ�type + step + �������С��ĸ�ʽ׷�ӵ��ڲ���������
 */ 
void image_compress_boundary(uint8 boundary[][2],uint8 step,uint8 type);

/**
 * @brief ����ѹ����float��
 * @param parameter ����ֵ��float��
 * @param parameter_type ������
 * ˵����д���ʽΪ��parameter_type + 4B IEEE754������򣩡���
 */
void parameter_compress_float(float parameter,uint8 parameter_type);

/**
 * @brief ����ѹ����int��
 * @param parameter ����ֵ��int��
 * @param parameter_type ������
 * ˵����д���ʽΪ��parameter_type + 4B �������������
 */
void parameter_compress_int(int parameter,uint8 parameter_type);

/**
 * @brief ����ѹ����uint8��
 * @param parameter ����ֵ��uint8��
 * @param parameter_type ������
 */
void parameter_compress_uint8(uint8 parameter,uint8 parameter_type);

/**
 * @brief ���ڲ�������������Ϊ��һ֡��д�� Flash
 * @return storage_error_t ������
 * ǰ�ã����Ѱ���д�����ɡ����� + ���ء��Σ��ɹ��� frame_count++��
 */
storage_error_t store_compressed_image(void);

/**
 * @brief ��ȡһ֡ѹ�����ݵ��ڲ�������
 * @param video_process ֡���ƶ����ԣ�go/back/go_go/back_back��
 * @return storage_error_t ������
 * ˵������ȡ��ɵ��� image_data_analysis()/data_analysis() ���н��롣
 */
storage_error_t read_compressed_image(video_process_t video_process);

/**
 * @brief ��ȡһ֡ѹ�����ݣ������ˣ���������
 * ���� STORAGE_END ��ʾ���޸���֡��
 */
storage_error_t read_compressed_data(void);

/**
 * @brief ���ݷ����������������ڸ���Σ�
 * ˵�������������� + ���ء���¼����ҵ����Ҫ���н����뻹ԭ������չ�������͡�
 */
void data_analysis(void);

/**
 * @brief ͼ�����ݷ�����������ͼ��Σ�
 * ˵������ͼ��ν��벢��ԭ��ͼ�񻺳�����
 */
void image_data_analysis(void);


/**
 * @brief ��ȡ��ǰ֡��
 * @return uint32_t ��ǰ֡��
 */
uint32 get_frame_count(void);

/**
 * @brief �����洢�飨��������֡��
 * @return storage_error_t ������
 * ע�⣺���� STORAGE_IDLE ״̬�µ��ã�������ʱ�ϳ���
 */
storage_error_t erase_storage_block(void);

/**
 * @brief ̽�� flash ����д���֡��
 * ˵����ͨ������������λ���һ֡����ʼҳ�������ɻָ� frame_count��
 */
void image_read_frame_count(void);


#endif /* _IMAGE_STORAGE_H_ */
