#pragma once
#include "pch.h"
#include "framework.h"

#pragma pack(push)
#pragma pack(1)
class CPacket
{
public:
    CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
    //根据传入的信息创建一个数据包对象，封装发送给客户端的数据，MackeDriverInfo()使用到了
    CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
        sHead = 0xFEFF;
        nLength = nSize + 4;
        sCmd = nCmd;
        if (nSize > 0) {
            strData.resize(nSize);
            memcpy((void*)strData.c_str(), pData, nSize);
        }
        else {
            strData.clear();
        }
        sSum = 0;
        for (size_t j = 0; j < strData.size(); j++)
        {
            sSum += BYTE(strData[j]) & 0xFF;
        }
    }

    //括号和等号的拷贝构造函数
    CPacket(const CPacket& pack) {
        sHead = pack.sHead;
        nLength = pack.nLength;
        sCmd = pack.sCmd;
        strData = pack.strData;
        sSum = pack.sSum;
    }
    CPacket& operator=(const CPacket& pack) {
        if (this != &pack) {
            sHead = pack.sHead;
            nLength = pack.nLength;
            sCmd = pack.sCmd;
            strData = pack.strData;
            sSum = pack.sSum;
        }
        return *this;
    }

    //解析接收到的数据包，下面DealCommand()中用到了
    CPacket(const BYTE* pData, size_t& nSize) {
        size_t i = 0;
        for (; i < nSize; i++) {
            if (*(WORD*)(pData + i) == 0xFEFF) {//寻找 0xFEFF 的部分
                sHead = *(WORD*)(pData + i);
                i += 2;//i += 2 使 i 指向 0xFEFF 之后的内容（35.00）
                break;
            }
        }
        //包数据可能不全，或者包头未能全部接收到，因为 i + 4 + 2 + 2 == nSize 的时候，相当于只有有完整的
        //数据包头（sHead）、数据长度（nLength）、命令（sCmd）和校验和（sSum），而没有任何的数据内容
        if (i + 4 + 2 + 2 > nSize) {
            nSize = 0;
            return;
        }
        //包头2，数据长度4，命令标识2，
        nLength = *(DWORD*)(pData + i); i += 4;
        // nLength 是 命令标识 + 数据内容 + 校验和 的总字节数
        if (nLength + i > nSize) {//包未完全接收到，就返回，解析失败
            nSize = 0;
            return;
        }
        sCmd = *(WORD*)(pData + i); i += 2;//i 指向 命令标识 后面

        // nLength 包括 sCmd 和 sSum，因此实际的数据部分大小为 nLength - 4
        if (nLength > 4) {
            strData.resize(nLength - 2 - 2);//重新设置 strData 的大小
            //memcpy((void*)strData.c_str(), pData + i, nLength - 4);//这样不是很好
            memcpy(&strData[0], pData + i, nLength - 4); // 直接修改字符串内容
            i += nLength - 4;//i指向数据部分之后，即校验和 sCmd
        }
        sSum = *(WORD*)(pData + i); i += 2;//i 指向 校验部分的末尾
        //下面检查校验和
        WORD sum = 0;
        for (size_t j = 0; j < strData.size(); j++)
        {
            sum += BYTE(strData[j]) & 0xFF;//不知道为什么要这样，但是也不要太管这个
        }
        if (sum == sSum) {
            nSize = i;// 解析成功，返回解析后的数据包大小
            return;
        }
        nSize = 0;//否则返回0，表示数据有问题
    }

    ~CPacket() {}

    int Size() {//包数据的大小
        return nLength + 6;
    }
    //将 CPacket 类的对象转换为 char* 类型的二进制数据流用于发送，下面的Send函数使用
    const char* Data() {
        strOut.resize(nLength + 6);//6是前面的包头标识，数据长度，nLength是后面的命令标识，数据内容，校验和
        BYTE* pData = (BYTE*)strOut.c_str();//pData 指向 strOut 的数据区域，用于填充数据包,转换为 BYTE* ,确保按字节级别写入数据
        *(WORD*)pData = sHead; pData += 2;//这里的 *(WORD*)pData = sHead 是将将 sHead 的值存入 pData 指向的地
        *(DWORD*)(pData) = nLength; pData += 4;
        *(WORD*)pData = sCmd; pData += 2;
        memcpy(pData, strData.c_str(), strData.size());//emcpy() 复制 strData 数据部分
        pData += strData.size();
        *(WORD*)pData = sSum;
        return strOut.c_str();
    }

public:
    WORD sHead;//固定位 0xFEFF
    DWORD nLength;//包长度（从控制命令开始，到和校验结束）
    WORD sCmd;//控制命令
    std::string strData;//包数据
    WORD sSum;//和校验
    std::string strOut;//整个包的数据
};
#pragma pack(pop)

typedef struct MouseEvent {
    MouseEvent() {
        nAction = 0;   // 0=单击，1=双击，2=按下，3=放开
        nButton = -1;  // 0=左键，1=右键，2=中键，4=无按键
        ptXY.x = 0;    // 鼠标 X 坐标
        ptXY.y = 0;    // 鼠标 Y 坐标
    }
    WORD nAction;  // 事件类型
    WORD nButton;  // 按键类型
    POINT ptXY;    // 鼠标位置
} MOUSEEV, * PMOUSEEV;

//存储目录中文件的信息
typedef struct file_info {
    file_info() {
        IsInvalid = FALSE;
        IsDirectory = -1;
        HasNext = TRUE;
        memset(szFileName, 0, sizeof(szFileName));
    }
    BOOL IsInvalid;//是否有效
    BOOL IsDirectory;//是否为目录 0 否 1 是
    BOOL HasNext;//是否还有后续 0 没有 1 有(文件一个一个发送，需要它判断是否结束)
    char szFileName[256];//文件名
}FILEINFO, * PFILEINFO;