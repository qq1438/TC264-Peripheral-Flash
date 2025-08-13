## TC264-Peripheral-Flash ͼ�������֡ʽ�洢���

���ֿ��ṩ��Ӣ���� AURIX TC264 ƽ̨�ϣ����� W25N04KV NAND Flash �ġ�֡ʽ�洢��ʵ��ʾ����
��ͼ��͵���/�������ݰ��ա����ͱ�ʶ + ���ء��ļ�¼��ʽд��ҳ�����֡����������֧�ֶ�ȡ�ط��밴���ͽ�����

��Ŀ�ֿ����֧��
- ����֧ main ����������Ŀ¼ `code`��������ֲ����
- ��ʷ�������ձ����� `flash-upload` ��֧������ο����ɻ�����
- �ֿ���ҳ�� PR ��ڲμ���Ŀ��ַ��[TC264-Peripheral-Flash](https://github.com/qq1438/TC264-Peripheral-Flash)

### ��������
- ���� W25N04KV ��ҳ/���д�볬ʱ����֧�ֳ���ִ����æ״̬��ѯ
- ֡ʽ�洢��ÿ֡ռ�ù̶�ҳ��������д�룻�ṩ���ַ�̽�������Ч֡����
- ͼ��ѹ���뻹ԭ���� MT9V03X ԭʼͼ�������� `IPCH��IPCW` ��洢����ȡʱ˫���Բ�ֵ��ԭ
- ���ͻ����ݸ�ʽ���� 1 �ֽ�����ͷ + ���ص����ݶ�˳��д�벢������������չ�Զ����
- ���Ͷ��Э����CPU1 ����ɼ�/ѹ����CPU0 ���𴥷�ˢд��ͨ��ȫ�ֱ�־Э����

## Ŀ¼�ṹ
```
code/
  w25n04.h, w25n04.c          // W25N04KV ������ָ���װ
  image_storage.h, .c         // ֡ʽ�洢��ͼ��ѹ��/�����߼�
```

## Ӳ��������
- SPI ʵ�������ʣ��� `code/w25n04.h` ��ͨ�����º�����
  - `W25N04_SPI`��Ĭ�� `SPI_3`��
  - `W25N04_SPI_SPEED`��Ĭ�� 100MHz����ƽ̨�벼��ʵ���µ���
- ���Ŷ��壨Ĭ�ϣ���
  - SCK `P22_3`��MOSI `P22_0`��MISO `P22_1`��CS `P22_2`
  - WP `P23_0`�������������HOLD `P23_1`�����������
- DMA ͨ����`W25N04_DMA_CH = IfxDma_ChannelId_7`���ɰ����̷��������
- ������й��̲�һ�£����� `w25n04.h` ���޸ĺ�������Ӳ������

## ������������
- ƽ̨��Ӣ���� AURIX TC264��Tasking/ADS �������������ɲο���
- ͷ�ļ�����ڣ�`zf_common_headfile.h`�����ڹ����п��ã�
- ��Ҫ�꣺`MT9V03X_W`��`MT9V03X_H`������ͷԭʼͼ���ߣ���`IPCH`��`IPCW`��ѹ����߿�
- OpenMP ָ�`image_storage.c` �а��� `#pragma omp parallel for`������������֧�ֿ����ӻ��Ƴ�

## ����֡��ʽ
- ÿ�μ�¼�� 1 �ֽڡ����ͱ�ʶ����ͷ�������������͵ĸ������ݣ����˳�����й���һ֡
- �Ѷ������ͣ��� `image_storage.h`����
  - `image_type (0x01)`: ����Ϊ `IPCH*IPCW` �ֽڵ� 8 λ�Ҷ�ͼ�������ȣ�
  - `left_boundary_type (0x02)`: ����Ϊ `1B step + 2*step ������(y,x)`
  - `right_boundary_type (0x03)`: ͬ��
- �������̣���ȡ `storage_buffer[address]` �õ����� �� `address += 1` �� �����ͽ������ƽ� `address`
- ��������չ������Ψһ����ֵ��Լ�����ز��֣�д��˰������� + ���ء�׷�ӣ��������� `switch` ������ `case` ����ȷ�ƽ���ַ

## ���ٿ�ʼ
### 1) ��ʼ��
```c
// ��ʼ�� W25N04 ��洢ģ��
w25n04_init();
image_storage_init();
```

### 2) �ɼ���д�루����ʾ����
```c
// ��ԭʼͼ��ѹ��д���ڲ�����������д�� RAM�������̣�
image_compress(mt9v03x_image);
// ����׷�Ӳ����Σ��ɵ��� parameter_compress_* �� image_compress_boundary
// �����֡ˢд�� Flash
store_compressed_image();
```

### 3) ��ȡ��ط�
```c
// ����һ֡���˵��ڲ�������
read_compressed_image(go);
// ������ͼ��β���ԭ����ʾ����
image_data_analysis();
// ��ʱ�ɵ�����Ļ������ʾ mt9v03x_image
```

### 4) ���Э�������鷽����
- CPU1���ɼ�֡ �� `image_compress(...)` �� ��λȫ�� `is_compress = true`
- CPU0����ѯ `is_compress` �� ���� `store_compressed_image()` ˢд �� ���� `is_compress`

## API ���������ģ�
- �洢��ʼ����״̬
  - `storage_error_t image_storage_init(void)`����ʼ�� W25N04 ״̬���������
  - `storage_state_t get_storage_state(void)`��`storage_error_t get_last_error(void)`
  - `uint32 get_frame_count(void)`��`void image_read_frame_count(void)`������̽�������Ч֡��
- ֡д�����ȡ
  - `void image_compress(uint8 src[MT9V03X_H][MT9V03X_W])`��ѹ��ͼ���� `image_type` д�뻺��
  - `void image_compress_boundary(uint8 boundary[][2], uint8 step, uint8 type)`��д��߽��
  - `void parameter_compress_float/int/uint8(...)`��д�������
  - `storage_error_t store_compressed_image(void)`����֡ˢд��`frame_count++`
  - `storage_error_t read_compressed_image(video_process_t vp)` / `read_compressed_data()`�����˵�����
- ����
  - `void image_data_analysis(void)`������ͼ��β���ԭ��ͼ�񻺳�
  - `void data_analysis(void)`�������ͱ���������������չ�����
- W25N04 ������������ѡ����� `w25n04.h`��
  - `w25n04_init`��`w25n04_read_page`��`w25n04_write_page`��`w25n04_block_erase`��`w25n04_wait_busy`

## ��ֲ����չָ��
1) Ӳ����
   - �޸� `w25n04.h` �� SPI ʵ�������������ź�������ʵ��Ӳ��
   - ������ DMA ͨ�����ж�ӳ�䣬��ͬ����������
2) ͼ��ߴ���ѹ��
   - �ڹ��̹���ͷ�ļ�����ȷ���� `MT9V03X_W/H`����������� `IPCH/IPCW`
3) �Զ������ݶ�
   - ����Ψһ���ͺ꣨����������ֵ��ͻ��
   - д��ˣ��������� + ���ء�׷�ӣ������ˣ��� `switch` ������ `case` ���ƽ���ַ
4) ʱ���뽡׳��
   - дǰȷ�� `w25n04_write_enable()` �ɹ�����������ѯ `w25n04_wait_busy()`
   - ���� `storage_error_t` ����������Ҫʱ��������

## �������⣨FAQ��
- ��ȡ���� `STORAGE_END`����ʾ�ѵ����һ֡��������֡�Ż��������д
- д��ʧ�ܣ����д������`write_enable`��æ�ȴ��빩���ȶ��ԣ���Ҫʱ��Ƶ SPI
- ECC ״̬��`w25n04_read_page` �����������޴�/�ɾ���/���ɾ��������¼���������ʽ���

## ������Ȩ
- ��ǰ�ֿ�δ���� License �ļ����������û���ηַ�������������ȷ���������

## �ο�
- ��Ŀ�ֿ��� PR ������ڣ�[TC264-Peripheral-Flash](https://github.com/qq1438/TC264-Peripheral-Flash)


