#pragma once

#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>
#include <list>
#include <map>
#include <mutex>

#define WM_SEND_PACK (WM_USER+1) //CClientSocket::SendPack，发送包数据
#define WM_SEND_PACK_ACK (WM_USER+2) //发送包数据应答

#pragma pack(push)
#pragma pack(1)
class CPacket
{
public:
    CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
    //根据传入的信息创建一个数据包对象，封装发送给客户端的数据，MackeDriverInfo()使用到了
    CPacket(WORD nCmd, const BYTE* pData, size_t nSize){
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
    CPacket(const BYTE* pData, size_t& nSize){
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
        // 包未完全接收到，就返回，解析失败，如果是 nLength + i > 4096 , 说明客户端未处理的数据过多
        // nLength 过大说明
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
            TRACE("%s\r\n", strData.c_str() + 12);
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
            nSize = i;// 解析成功，返回已处理的数据的大小
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
    //后面的const表示不会修改类的成员变量
    const char* Data(std::string & strOut) const{
        strOut.resize(nLength + 6);//6是前面的包头标识，数据长度，nLength是后面的命令标识，数据内容，校验和
        BYTE* pData = (BYTE*)strOut.c_str();//pData 指向 strOut 的数据区域，用于填充数据包,转换为 BYTE* ,确保按字节级别写入数据
        *(WORD*)pData = sHead; pData += 2;//这里的 *(WORD*)pData = sHead 是将 sHead 的值存入 pData 指向的地
        *(DWORD*)(pData) = nLength; pData += 4;
        *(WORD*)pData = sCmd; pData += 2;
        memcpy(pData, strData.c_str(), strData.size());//memcpy() 复制 strData 数据部分
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
    HANDLE hEvent;
    //std::string strOut;//整个包的数据
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

enum {
    CSM_AUTOCLOSE = 1, //Client Socket Mode  自动关闭模式
};

typedef struct PacketData {
    std::string strData;
    UINT nMode;
    WPARAM wParam;
    PacketData(const char* pData, size_t nLen, UINT mode, WPARAM nParam = 0) {
        strData.resize(nLen);
        memcpy((char*)strData.c_str(), pData, nLen);
        nMode = mode;
        wParam = nParam;
    }
    PacketData(const PacketData& data) {
        strData = data.strData;
        nMode = data.nMode;
        wParam = data.wParam;
    }
    PacketData& operator=(const PacketData& data) {
        if (this != &data) {
            strData = data.strData;
            nMode = data.nMode;
            wParam = data.wParam;
        }
        return *this;
    }
}PACKET_DATA;


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

    bool InitSocket() {
        if (m_sock != INVALID_SOCKET)CloseSocket();
        m_sock = socket(PF_INET, SOCK_STREAM, 0);//这样每次初始化（点击按钮）就可以重新创建套接字
        //TRACE("m_sock = %d\r\n", m_sock);
        //校验
        sockaddr_in serv_addr, cli_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        //TRACE("addr %08X nIP %08X\r\n", inet_addr("127.0.0.1"), nIP);
        //serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        serv_addr.sin_addr.s_addr = htonl(m_nIP);
        serv_addr.sin_port = htons(m_nPort);

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

#define BUFFER_SIZE 8192000
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
        while (true) {
            // 读取数据，并存入 buffer + index 位置，BUFFER_SIZE - index 是剩余容量，len是接收到的数据的大小
            // len 记录每次读取到的数据的大小，index记录当前 buffer 中的总大小
            // 如果第一次发送的数据不完整，recv函数读取到了一部分数据，那么第二次进入while循环的时候，就要从buffer + index位置开始存
            size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
            if (((int)len <= 0&& (int)index <= 0 ) ) {//
                return -1;
            }
            TRACE("recv len =  %d (0x%08X)   index = %d   (0x%08X)\r\n", len, len, index, index);
            //TRACE("客户端DealCommand()函数收到的数据长度： %d\r\n", len);
            index += len; // 更新 index，指向 buffer 当前存储的总数据大小
            size_t templen = index;// 让 templen 记录 buffer 中的总数据大小（即接收的累计数据量）
            TRACE("recv len =  %d (0x%08X)   index = %d   (0x%08X)\r\n", len, len, index, index);

            //解析数据包，使用 CPacket 解析数据包，templen 代表当前数据大小（templen传入CPacket之后会修改后传出）
            m_packet = CPacket((BYTE*)buffer, templen);
            TRACE("command %d r\n", m_packet.sCmd);
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

    bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed, WPARAM wParam = 0);

    /*
    bool CClientSocket::SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed = true)
    {
        // m_hThread 负责存储 threadFunc() 运行的线程句柄，这样就能保证threadFunc()只有一个实例
        if (m_sock == INVALID_SOCKET && m_hThread == INVALID_HANDLE_VALUE) {
            //if (InitSocket() == false)return false;
            m_hThread = (HANDLE)_beginthread(&CClientSocket::threadEntry, 0, this);
            TRACE("start thread\r\n");
        }

        
        //映射 HANDLE 到 list<CPacket>，确保 lstPacks 只储存自己的 recv() 数据
        auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>&>(pack.hEvent, lstPacks));
        m_mapAutoClosed.insert(std::pair<HANDLE, bool>(pack.hEvent, isAutoClosed));
        
        m_lock.lock();
        //将数据包加入发送队列 (等待之后发送)，让threadFunc()处理send()
        m_lstSend.push_back(pack);//这里可以多次push不同的请求，因为是线程那边逐个处理的
        //阻塞当前线程，直到CClientSocket::threadFunc()的 recv()返回数据并 SetEvent(hEvent) 触发
        m_lock.unlock();


        TRACE("cmd %d event %08X thread %d\r\n", pack.sCmd, pack.hEvent, GetCurrentThreadId());
        WaitForSingleObject(pack.hEvent, INFINITE);//处理完成之后才能进入下面的操作
        TRACE("cmd %d event %08X thread %d\r\n", pack.sCmd, pack.hEvent, GetCurrentThreadId());

        // 根据 pack.hEvent（这次处理的事件） 寻找对应的事件，将该事件从 m_mapAck 中拿出
        std::map<HANDLE, std::list<CPacket>&>::iterator it;
        it = m_mapAck.find(pack.hEvent);
        if (it != m_mapAck.end()) {
            m_lock.lock();
            m_mapAck.erase(it);
            m_lock.unlock();
            return true;
        }
        return false;
    }
    */

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

    void UpdateAddress(int nIP, int nPort) {
        while ((m_nIP != nIP) || (m_nPort != nPort)) {
            m_nIP = nIP;
            m_nPort = nPort;
        }
    }

private:
    HANDLE m_eventInvoke;//启动事件
    UINT m_nThreadID;
    typedef void(CClientSocket::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);
    std::map<UINT, MSGFUNC> m_mapFunc;
    std::mutex m_lock;
    HANDLE m_hThread;//管理后台线程
    bool m_bAutoClose;//控制 recv() 是否自动关闭
    std::list<CPacket> m_lstSend;//存储待发送的数据包
    //存储 recv() 返回的数据，确保数据不被竞争这里改成取引用
    std::map<HANDLE, std::list<CPacket>&>m_mapAck;
    std::map<HANDLE, bool>m_mapAutoClosed;//通知 recv()是否持续监听数据
    std::vector<char> m_buffer;
    CPacket m_packet;
    //SOCKET m_client;
    SOCKET m_sock;
    int m_nIP;
    int m_nPort;
    //静态成员变量类内声明，类外初始化，静态成员函数只能访问静态成员变量
    static CClientSocket* m_instance;

    static unsigned __stdcall threadEntry(void* arg);
    void threadFunc();
    void threadFunc2();

    CClientSocket() :
        m_nIP(INADDR_ANY), m_nPort(0),m_sock(INVALID_SOCKET),
        m_bAutoClose(true), m_hThread(INVALID_HANDLE_VALUE), m_lstSend(NULL) {
        if (InitSockEnv() == FALSE) // 初始化套接字环境
        {
            MessageBox(NULL, _T("无法初始化套接字环境！"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
            exit(0); // 初始化失败时直接退出程序
        }

        m_eventInvoke = CreateEvent(NULL, TRUE, FALSE, NULL);
        m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientSocket::threadEntry, this, 0, &m_nThreadID);
        if (WaitForSingleObject(m_eventInvoke, 100) == WAIT_TIMEOUT)
            TRACE("网络消息处理线程启动失败了！\r\n");
        CloseHandle(m_eventInvoke);

        m_buffer.resize(BUFFER_SIZE);
        memset(m_buffer.data(), 0, BUFFER_SIZE);

        struct {
            UINT message;
            MSGFUNC func;
        }funcs[] = {
            {WM_SEND_PACK,&CClientSocket::SendPack},
            {0,NULL}
            //{WM_SEND_PACK, },
        };

        for (int i = 0; funcs[i].message != 0; i++) {
            //m_mapFunc.insert(std::pair<UINT, MSGFUNC>(funcs[i].message,funcs[i].func));
            if (m_mapFunc.insert(std::pair<UINT, MSGFUNC>(funcs[i].message, funcs[i].func)).second == false)
            {
                TRACE("插入失败，消息值：%d 函数值：%O8X 序号：%d\r\n", funcs[i].message, funcs[i].func, i);
            }
        }
    }

    bool Send(const char* pData, int nSize) {//直接发送 pData
        if (m_sock == -1)return false;
        return send(m_sock, pData, nSize, 0) > 0;
    }

    bool Send(const CPacket& pack) {//封装数据包并发送给服务器（通过Data()函数）
        //TRACE("m_sock = %d \r\n", m_sock);
        if (m_sock == -1)return false;
        //Dump((BYTE*)pack.Data(), pack.Size());
        std::string strOut;
        pack.Data(strOut);//将数据封装放入 strOut 中

        return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
    }

    ~CClientSocket() {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
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
        m_hThread = INVALID_HANDLE_VALUE;
        m_bAutoClose = ss.m_bAutoClose;
        m_nIP = ss.m_nIP;
        m_nPort = ss.m_nPort;
        m_sock = ss.m_sock;
    }

    void SendPack(UINT nMsg, WPARAM WParam, LPARAM lParam);


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
};
