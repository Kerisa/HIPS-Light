
#define POLY 0xEDB88320L // CRC32生成多项式

static unsigned int crc_table[256];

static unsigned int get_sum_poly(unsigned char data)
{
    int hi;
    unsigned int sum_poly = data;
    for(int j=0; j<8; j++)
    {
        hi = sum_poly & 0x01; // 取得reg的最高位
        sum_poly >>= 1;
        if (hi) sum_poly = sum_poly ^ POLY;
    }
    return sum_poly;
}

void create_crc_table()
{
    for (int i=0; i<256; i++)
    {
        crc_table[i] = get_sum_poly((unsigned char)(i&0xFF));
    }
}

unsigned int CRC32_4(const unsigned char* data, unsigned int reg, int len)
{
    reg ^= 0xFFFFFFFF;
    for (int i=0; i<len; i++)
    {
        reg = (reg>>8) ^ crc_table[(reg&0xFF) ^ data[i]];
    }
    return reg ^ 0xFFFFFFFF;
}