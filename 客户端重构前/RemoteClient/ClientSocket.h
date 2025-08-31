#pragma once

#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>

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

    //解析接收到的数据包，下面DealCommand()中用到了，pData 是 buffer，nSize 是 buffer 中数据的大小
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
        //包未完全接收到，就返回，解析失败，如果是 nLength + i > 4096 , 说明客户端未处理的数据过多
        if (nLength + i > nSize) {
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
        TRACE("校验失败，解析数据包失败\r\n\n");
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
        *(WORD*)pData = sHead; pData += 2;//这里的 *(WORD*)pData = sHead 是将 sHead 的值存入 pData 指向的地
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

void Dump(BYTE* pData, size_t nSize);
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

std::string GetErrInfo(int wsaErrCode);
class CClientSocket
{
public:
    //类中的静态函数在类外可以直接通过类名去访问
    static CClientSocket* getInstance() {
        if (m_instance == NULL) {//静态函数没有this指针，所以无法直接访问成员变量
            m_instance = new CClientSocket();
        }
        return m_instance;
    }

    bool InitSocket(int nIP,int nPort) {
        if (m_sock != INVALID_SOCKET)CloseSocket();
        m_sock = socket(PF_INET, SOCK_STREAM, 0);//这样每次初始化（点击按钮）就可以重新创建套接字
        TRACE("m_sock = %d\r\n", m_sock);
        //校验
        sockaddr_in serv_addr, cli_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        //TRACE("addr %08X nIP %08X\r\n", inet_addr("127.0.0.1"), nIP);
        //serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        serv_addr.sin_addr.s_addr = htonl(nIP);
        serv_addr.sin_port = htons(nPort);

        if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
            AfxMessageBox("指定的IP地址不存在！");
            return false;
        }
        int ret = connect(m_sock,(sockaddr*)&serv_addr,sizeof(serv_addr));
        if (ret == -1) {
            AfxMessageBox("连接失败!");
            TRACE("连接失败：%d %s\r\n", WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
            return false;
        }

        return true;
    }

#define BUFFER_SIZE 4096000
    //接收服务器发送的数据(是完整的数据，需要CPacket(const BYTE* pData, size_t& nSize)函数处理 )，初始化 m_packet 对象
    int DealCommand() {
        if (m_sock == -1)return -1;
        //char buffer[1024] = "";

        //处理命令
        char* buffer = m_buffer.data();
        if (buffer == NULL) {
            TRACE("内存不足！\r\n");
            return -2;
        }
        //memset(buffer, 0, BUFFER_SIZE);//不在这里清零，在构造函数中
        static size_t index = 0; // 记录 buffer 中已存储的数据大小
        //size_t index = 0;
        while (true) {
            // 读取数据，并存入 buffer + index 位置，BUFFER_SIZE - index 是剩余容量，len是接收到的数据的大小
            // len 记录每次读取到的数据的大小，index记录当前 buffer 中的总大小
            // 如果第一次发送的数据不完整，recv函数读取到了一部分数据，那么第二次进入while循环的时候，就要从buffer + index位置开始存
            size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
            if ((len <= 0&& index <= 0 ) ) {//
                return -1;
            }
            //TRACE("客户端DealCommand()函数收到的数据长度： %d\r\n", len);
            index += len; // 更新 index，指向 buffer 当前存储的总数据大小

            size_t templen = index;// 让 len 记录 buffer 中的总数据大小（即接收的累计数据量）
            //解析数据包，使用 CPacket 解析数据包，len 代表当前数据大小（len传入CPacket之后会修改后传出）
            m_packet = CPacket((BYTE*)buffer, templen);
            //TRACE("CPacket解析的数据长度： %d\r\n", templen);

            //如果 templen==0 ，就说明数据有问题或者不完整，需要通过while循环重新读取
            if (templen > 0) {
                //buffer：目标地址（拷贝后的起始位置），buffer + templen：源地址（未处理的数据的起始位置），BUFFER_SIZE - templen：需要拷贝的字节数
                memmove(buffer, buffer + templen, BUFFER_SIZE - templen);
                index -= templen;//已处理大小为 templen 的数据，将这部分减掉，表示记录剩余未解析数据的大小，index刚好指向刚才移动数据之后的位置，下次就从index开始写入
                //delete[]buffer;
                return m_packet.sCmd;//返回解析出的命令
            }
        }
        //delete[]buffer;
        return -1;
    }

    bool Send(const char* pData, int nSize) {//直接发送 pData
        if (m_sock == -1)return false;
        return send(m_sock, pData, nSize, 0) > 0;
    }
    bool Send(CPacket& pack) {//封装数据包并发送给服务器（通过Data()函数）
        //TRACE("m_sock = %d \r\n", m_sock);
        if (m_sock == -1)return false;
        //Dump((BYTE*)pack.Data(), pack.Size());
        return send(m_sock, pack.Data(), pack.Size(), 0) > 0;
    }

    bool GetFilePath(std::string& strPath) {
        if (((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) ||
            (m_packet.sCmd == 9))
        {
            strPath = m_packet.strData;// 从数据包中提取内容部分，即需要的路径
            return true;
        }
        return false;
    }

    bool GetMouseEvent(MOUSEEV& mouse) {
        if (m_packet.sCmd == 5) {
            memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
            return true;
        }
        return false;
    }
    CPacket& GetPacket() {
        return m_packet;
    }
    void CloseSocket() {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
private:
    //静态成员变量类内声明，类外初始化，静态成员函数只能访问静态成员变量
    static CClientSocket* m_instance;

    //CHepler类
    class CHelper {
    public:
        CHelper() {
            CClientSocket::getInstance();
        }
        ~CHelper() {
            CClientSocket::releaseInstance();
        }
    };
    static void releaseInstance() {
        if (m_instance != NULL) {
            CClientSocket* tmp = m_instance;
            m_instance = NULL;
            delete tmp;
        }
    }
    static CHelper m_helper;//要写在声明下面

    std::vector<char> m_buffer;
    CPacket m_packet;
    //SOCKET m_client;
    SOCKET m_sock;
    CClientSocket() {
        if (InitSockEnv() == FALSE) // 初始化套接字环境
        {
            MessageBox(NULL, _T("无法初始化套接字环境！"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
            exit(0); // 初始化失败时直接退出程序
        }
        m_buffer.resize(BUFFER_SIZE);
        memset(m_buffer.data(), 0, BUFFER_SIZE);
    }

    ~CClientSocket() {
        closesocket(m_sock);
        WSACleanup(); // 清理套接字环境
    }

    BOOL InitSockEnv() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
            return FALSE; // 如果初始化失败返回 FALSE
        }
        return TRUE; // 初始化成功返回 TRUE
    }

    //私有化拷贝构造函数
    CClientSocket& operator=(const CClientSocket& ss) {}
    CClientSocket(const CClientSocket& ss) {
        m_sock = ss.m_sock;
    }
};

//extern CClientSocket server; // 声明一个外部变量 server
