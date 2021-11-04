//
//  h264_utils.cpp
//  H264Demo
//
//  Created by BZF on 2021/11/3.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    NALU_UNKNOWN = 0, //未使用
    NALU_TYPE_SLICE = 1, //不分区、非IDR图像的片（片的头信息和书脊）
    NALU_TYPE_DPA = 2, //片分区A
    NALU_TYPE_DPB = 3, //片分区B
    NALU_TYPE_DPC = 4, //片分区C
    NALU_TYPE_IDR = 5, //IDR图像中的片
    NALU_TYPE_SEI = 6, //补充增强信息单元
    NALU_TYPE_SPS = 7, //序列参数集
    NALU_TYPE_PPS = 8, //图像参数集
    
    NALU_TYPE_AUD = 9, //分界符
    NALU_TYPE_EOSEQ = 10,
    NALU_TYPE_EOSTREAM = 11,
    NALU_TYPE_FILL = 12 //填充（哑元数据，用于填充字节）
    
} NaluType;


typedef enum {
    NALU_PRIORITY_DISPOSABLE = 0,
    NALU_PRIORITY_LOW =1,
    NALU_PRIORITY_HIGH = 2,
    NALU_PRIORITY_HIGHEST = 3
}NaluPriority;

typedef struct{
    int startcodeprefix_len;
    unsigned len;
    unsigned max_size;
    int forbidden_bit;
    int nal_reference_idc;
    int nal_unit_type;
    char *buf;
} NALU_t;

FILE *h264bitstream = NULL;

int info2=0, info3=0;

static int findStartCode2(unsigned char *buf){
    if(buf[0]!=0 || buf[1]!=0 || buf[2]!=1){
        return 0;
    }else{
        return 1;
    }
}

static int findStartCode3(unsigned char *buf){
    if(buf[0]!=0 || buf[1]!=0 || buf[2]!=0 || buf[3]!=1){
        return 0;
    }else{
        return 1;
    }
}


int getAnnexbNALU(NALU_t *nalu){
    int pos = 0;
    int startCodeFound, rewind;
    unsigned char *buf;
    if((buf = (unsigned char *)calloc(nalu->max_size, sizeof(char))) == NULL){
        printf("不能为buf分配内存");
    }
    
    nalu->startcodeprefix_len = 3;
    
    if(3 != fread(buf, 1, 3, h264bitstream)){
        free(buf);
        return 0;
    }
    info2 = findStartCode2(buf);
    if(info2 != 1){
        if(1 != fread(buf + 3, 1, 1, h264bitstream)){
            free(buf);
            return 0;
        }
        info3 = findStartCode3(buf);
        if(info3 != 1){
            free(buf);
            return -1;
        }else{
            pos = 4;
            nalu->startcodeprefix_len = 4;
        }
    }else{
        nalu->startcodeprefix_len = 3;
        pos = 3;
    }
    startCodeFound = 0;
    info2 = 0;
    info3 = 0;
    
    while(!startCodeFound){
        if(feof(h264bitstream)){
            nalu->len = (pos-1)-nalu->startcodeprefix_len;
            memcpy(nalu->buf, &buf[nalu->startcodeprefix_len], nalu->len);
            nalu->forbidden_bit = nalu->buf[0] & 0x80; //1bit
            nalu->nal_reference_idc = nalu->buf[0] & 0x60; //2bit
            nalu->nal_unit_type = (nalu->buf[0]) & 0x1f; //5bit
            free(buf);
            return pos-1;
        }
        buf[pos++] = fgetc(h264bitstream);
        info3 = findStartCode3(&buf[pos-4]);
        if(info3 != 1){
            info2 = findStartCode2(&buf[pos-3]);
        }
        startCodeFound = (info2 == 1 || info3 == 1);
    }
    
    rewind = (info3 == 1)? -4 : -3;
    if(0 != fseek(h264bitstream, rewind, SEEK_CUR)){
        free(buf);
        printf("Cannot fseek in the bit stream file");
    }
    
    nalu->len = (pos+rewind) - nalu->startcodeprefix_len;
    memcpy(nalu->buf, &buf[nalu->startcodeprefix_len], nalu->len);
    nalu->forbidden_bit = nalu->buf[0] & 0x80;
    nalu->nal_reference_idc = nalu->buf[0] & 0x60;
    nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;
    free(buf);
    return (pos + rewind);
}


/**
 * h.264码流解析步骤：
 *  1、搜索0x000001和0x00000001,分离出NALU;
 *  2、再分析NALU的各个字段;
 */
int h264Parser(char *url){
    NALU_t *n;
    int buffersize = 100000;
    
    FILE *myOut = stdout;
    
    h264bitstream = fopen(url, "rb+");
    if(h264bitstream == NULL){
        printf("Fail to open file!\n");
        return -1;
    }
    
    n = (NALU_t*)calloc (1, sizeof (NALU_t));
    if (n == NULL){
        printf("Alloc NALU Error\n");
        return 0;
    }

    
    n->max_size = buffersize;
    n->buf = (char *)calloc(buffersize, sizeof(char));
    if(n->buf == NULL){
        free(n);
        return -1;
    }
    
    int data_offset=0;
    int nal_num=0;
    
    printf("------+-------- NALU Table ----------+------+\n");
    printf(" NUM  |  POS    |  IDC     |  TYPE   | LEN  |\n");
    printf("------+-------- NALU Table ----------+------+\n");
    
    
    while (!feof(h264bitstream)) {
        int dataLength;
        dataLength = getAnnexbNALU(n);
        
        char typeStr[20] = {0};
        switch (n->nal_unit_type) {
            case NALU_TYPE_SLICE:
                sprintf(typeStr, "SLICE");
                break;
            case NALU_TYPE_DPA:
                sprintf(typeStr, "DPA");
                break;
            case NALU_TYPE_DPB:
                sprintf(typeStr, "DPB");
                break;
            case NALU_TYPE_DPC:
                sprintf(typeStr, "DPC");
                break;
            case NALU_TYPE_IDR:
                sprintf(typeStr, "IDR");
                break;
            case NALU_TYPE_SEI:
                sprintf(typeStr, "SEI");
                break;
            case NALU_TYPE_SPS:
                sprintf(typeStr, "SPS");
                break;
            case NALU_TYPE_PPS:
                sprintf(typeStr, "PPS");
                break;
            case NALU_TYPE_AUD:
                sprintf(typeStr, "AUD");
                break;
            case NALU_TYPE_EOSEQ:
                sprintf(typeStr, "EQSEQ");
                break;
            case NALU_TYPE_EOSTREAM:
                sprintf(typeStr, "EOSTREAM");
                break;
            case NALU_TYPE_FILL:
                sprintf(typeStr, "FILL");
                break;
            default:
                break;
        }
        
        char idc_str[20] = {0};
        switch (n->nal_reference_idc >> 5) {
            case NALU_PRIORITY_DISPOSABLE:
                sprintf(idc_str, "DISPOS");
                break;
            case NALU_PRIORITY_LOW:
                sprintf(idc_str, "LOW");
                break;
            case NALU_PRIORITY_HIGH:
                sprintf(idc_str, "HIGH");
                break;
            case NALU_PRIORITY_HIGHEST:
                sprintf(idc_str, "HIGHEST");
                break;
        }
        
        fprintf(myOut, "%5d| %8d| %7s| %6s| %8d|\n", nal_num, data_offset, idc_str, typeStr,n->len);
        
        data_offset = data_offset + dataLength;
        nal_num++;
    }
    
    if(n){
        if(n->buf){
            free(n->buf);
            n->buf = NULL;
        }
        free(n);
    }
    
    return 0;
}
