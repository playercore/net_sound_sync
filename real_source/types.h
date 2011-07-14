#ifndef _TYPES_H_
#define _TYPES_H_

#include "streams.h"

struct TMapFileHeader
{
    int totalBufferSize;    //一共可以存放数据的空间大小
    int availBufferSize;    //可以写入数据的空间大小
    int alreadUsedBufferSize;  //已经存放了数据的空间大小
    int consumedBufferSize; //在另外进程已经消费了的空间大小
    int packetNumbers;      //缓存一共的数据包个数
    int consumedPacketNumbers;//在另外进程已经消费了的数据包个数
    int resveredSize;           //保留的空间大小
    int maxSampleSize;          //最大的sample的大小
    bool haveMediaInfo;    //标识媒体类型和其他连接信息已经得到
};

struct TDataPacket
{
    int size;
    REFERENCE_TIME beginTime;
    REFERENCE_TIME endTime;
};

#endif
