#pragma once
#include "resource.h"
#include <map>
#include <atlimage.h>
#include "ServerSocket.h"
#include <direct.h>
#include "CTool.h"
#include <stdio.h>
#include <io.h>
#include <list>
#include "LockDialog.h"

#pragma warning(disable:4966) // fopen sprintf strcpy strstr  无视 4996 错误

class CCommand
{
public:
    CCommand();
    ~CCommand();
    int ExcuteCommand(int nCmd, std::list<CPacket>& lisPacket, CPacket inPacket);
    //命令处理函数，
    static void RunCommand(void* arg, int status, std::list<CPacket>& lisPacket, CPacket inPacket) {
        CCommand* thiz = (CCommand*)arg;
        if (status > 0)
        {
            int ret = thiz->ExcuteCommand(status, lisPacket, inPacket);
            if (ret != 0) {
                TRACE("执行命令失败:%d ret = %d\r\n", status, ret);
            }
        }
        else
            MessageBox(NULL, _T("无法正常接入用户，自动重试"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
    }

protected:
    typedef int(CCommand::* CMDFUNC)(std::list<CPacket>&, CPacket inPacket);//成员函数指针
    std::map<int, CMDFUNC> m_mapFunction;//从命令号到功能映射
    CLockDialog dlg;
    unsigned threadid;

protected:

    static unsigned __stdcall threadLockDlg(void* arg)
    {
        CCommand* thiz = (CCommand*)arg;
        thiz->threadLockDlgMain();
        _endthreadex(0);
        return 0;
    }

    void threadLockDlgMain() {
        TRACE("threadLockDlg:%s(%d):%d\r\n", __FUNCTION__, __LINE__, GetCurrentThreadId());
        dlg.Create(IDD_DIALOG_INFO, NULL);
        //if (!dlg.Create(IDD_DIALOG_INFO, NULL)) {  // 确保对话框创建成功
        //    TRACE("Dialog creation failed!\n");
        //    return 0;
        //}
        dlg.ShowWindow(SW_SHOW);//显示对话框，非模态
        //遮蔽后台窗口
        CRect rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = GetSystemMetrics(SM_CXFULLSCREEN);//宽度
        rect.bottom = GetSystemMetrics(SM_CYFULLSCREEN);//高度
        rect.bottom = LONG(rect.bottom * 1.10);//对话框大于屏幕，确保无法看到桌面
        TRACE("right = %d bottom = %d\r\n", rect.right, rect.bottom);
        dlg.MoveWindow(rect);
        CWnd* pText = dlg.GetDlgItem(IDC_STATIC);//获取对话框上的文本控件
        if (pText) {
            CRect rtText;
            //获取文本框的屏幕坐标位置，并存入 CRect变量 rtText
            pText->GetWindowRect(rtText);
            //设置居中显示
            int nWidth = rtText.Width();//文本框宽度
            int x = (rect.right - nWidth) / 2;//对话框-文本框 /2 就是文本框的左上角位置
            int nHeight = rtText.Height();
            int y = (rect.bottom - nHeight) / 2;
            pText->MoveWindow(x, y, rtText.Width(), rtText.Height());
        }

        //确保 CLockInfoDialog 置顶，无法被其他窗口覆盖
        dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        //不显示鼠标
        ShowCursor(false);
        //隐藏任务栏
        ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);

        dlg.GetWindowRect(rect);
        rect.left = 0;
        rect.top = 0;
        rect.right = 1;
        rect.bottom = 1;
        //禁止鼠标移动
        ClipCursor(rect);

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_KEYDOWN) {
                TRACE("msg:%08X wparam:%08x lparam:%08X\r\n", msg.message, msg.wParam, msg.lParam);
                if (msg.wParam == 0x41) {//按下a键 退出  ESC（1B)
                    break;
                }
            }
        }
        //恢复鼠标
        ClipCursor(NULL);
        ShowCursor(true);
        //恢复任务栏
        ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);
        dlg.DestroyWindow();
    }

    int MakeDriverInfo(std::list<CPacket>& lisPacket, CPacket inPacket) {
        std::string result;
        for (int i = 1; i <= 26; i++) {
            if (_chdrive(i) == 0) {// _chdrive(i) 判断磁盘 i 是否存在
                if (result.size() > 0)
                    result += ',';
                result += 'A' + i - 1;
            }
        }
        lisPacket.push_back(CPacket(1, (BYTE*)result.c_str(), result.size()));
        //CPacket pack(1, (BYTE*)result.c_str(), result.size());//将获取的数据封装成对象
        //CServerSocket::getInstance()->Send(pack);
        //CTool::Dump((BYTE*)pack.Data(), pack.Size());
        return 0;
    }

    // 用于读取文件获取文件列表，向客户端发送文件名 函数每找到一个文件，就发送一次，循环发送
    int MakeDirectoryInfo(std::list<CPacket>& lisPacket, CPacket inPacket) {
        std::string strPath = inPacket.strData;
        // 尝试进入目录, _chdir 切换到指定目录，!= 0为切换失败
        if (_chdir(strPath.c_str()) != 0) {
            FILEINFO finfo;
            finfo.HasNext = FALSE;//表示目录无法访问
            //finfo.HasNext = TRUE;//表示目录无法访问
            //封装成数据包 CPacket(2, (BYTE)&finfo, sizeof(finfo)) 并发送，通知客户端访问失败
            lisPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
            OutputDebugString(_T("没有权限访问目录！！"));
            return -2;
        }
        _finddata_t fdata;
        //int hfind = 0;
        intptr_t hfind = 0;//64位操作系统使用 intptr_t
        //_findfirst 查找当前目录下的第一个文件，fdata 存储查找到的文件信息（如文件名、属性等）
        //返回值 hfind 是搜索句柄，用于后续 _findnext() 继续查找下一个文件
        if ((hfind = _findfirst("*", &fdata)) == -1) {
            OutputDebugString(_T("没有找到任何文件！！"));
            FILEINFO finfo;
            finfo.HasNext = FALSE;
            //finfo.HasNext = TRUE;
            lisPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
            return -3;
        }

        //没有出错，开始记录文件信息
        int count = 0;
        do {
            FILEINFO finfo;
            //判断是否为目录（通过 fdata.attrib & _A_SUBDIR ），subdir 就是目录
            finfo.IsDirectory = (fdata.attrib & _A_SUBDIR) != 0;
            memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));//拷贝文件名
            TRACE("MakeDirectoryInfo函数打印文件信息：finfo.szFileName：%s\r\n", finfo.szFileName);
            lisPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
            count++;
        } while (!_findnext(hfind, &fdata));
        //返回值：0：成功找到下一个文件，并将信息存入 fdata , -1 ：没有找到下一个文件（已经遍历完所有文件）或出现错误。

        //注意有两个隐藏文件"."和".."，所以打印出的count比实际文件数量多2
        TRACE("服务端的MakeDirectoryInfo函数 count = %d\r\n", count);
        //最后这段代码的作用是向客户端明确通知文件列表已经发送完毕
        FILEINFO finfo;
        finfo.HasNext = FALSE;// HasNext 为 false 说明没有下一个文件了
        lisPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
        return 0;
    }

    int RunFile(std::list<CPacket>& lisPacket, CPacket inPacket) {
        std::string strPath = inPacket.strData;
        ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);  // 运行文件
        lisPacket.push_back(CPacket(3, NULL, 0));

        return 0;
    }

    int DownloadFile(std::list<CPacket>& lisPacket, CPacket inPacket) {
        std::string strPath = inPacket.strData;
        long long data = 0;
        FILE* pFile = NULL;
        // 以只读二进制模式打开文件
        // pFile是输出参数，存储打开的文件指针，"rb" 只读二进制
        errno_t err = fopen_s(&pFile, strPath.c_str(), "rb");
        // 表示文件不存在（返回0说明文件打开成功，2（ENOENT）文件不存在，13（EACCES）无权限访问）
        if (err != 0) { // 失败 发送 CPacket(4, 0, 8) 
            lisPacket.push_back(CPacket(4, (BYTE*)&data, 8));
            return -1;
        }

        if (pFile != NULL) {
            fseek(pFile, 0, SEEK_END);// 跳转到文件末尾，用于获取文件大小 (0表示不移动)
            data = _ftelli64(pFile);  // 获取文件大小
            lisPacket.push_back(CPacket(4, (BYTE*)&data, 8));

            fseek(pFile, 0, SEEK_SET);// 将文件指针移回开头，准备读取数据
            char buffer[1024] = "";  // 定义缓冲区
            size_t rlen = 0;
            do {
                rlen = fread(buffer, 1, 1024, pFile);  // 读取 1024 字节
                lisPacket.push_back(CPacket(4, (BYTE*)&buffer, rlen));

            } while (rlen >= 1024);

            fclose(pFile);
        }
        else {
            lisPacket.push_back(CPacket(4, NULL, 0));
        }
        
        return 0;
    }

    int MouseEvent(std::list<CPacket>& lisPacket, CPacket inPacket)
    {
        MOUSEEV mouse;
        memcpy(&mouse, inPacket.strData.c_str(), sizeof(MOUSEEV));
        //if (CServerSocket::getInstance()->GetMouseEvent(mouse)) {

        //}
        //else {
        //    OutputDebugString(_T("获取鼠标操作参数失败！！"));
        //    return -1;
        //}
        DWORD nFlags = 0;

        switch (mouse.nButton) {
        case 0://左键
            nFlags = 1;
            break;
        case 1://右键
            nFlags = 2;
            break;
        case 2://中键
            nFlags = 4;
            break;
        case 4://没有按键
            nFlags = 8;
            break;
        }
        // 这里只处理鼠标按下的情况，鼠标未按下的情况下面处理，这里不能重复处理
        // 如果按下和释放的位置不一致，可能会出现意外效果，比如拖动窗口，拖动文件的时候
        if (nFlags != 8)SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);

        switch (mouse.nAction)
        {
        case 0://单击
            nFlags |= 0x10;
            break;
        case 1://双击
            nFlags |= 0x20;
            break;
        case 2://按下
            nFlags |= 0x40;
            break;
        case 3://放开
            nFlags |= 0x80;
            break;
        default:
            break;
        }

        TRACE("\r\n");
        TRACE("鼠标的坐标为 : %08X x %d y %d\r\n", nFlags, mouse.ptXY.x, mouse.ptXY.y);
        TRACE("\r\n");

        switch (nFlags)
        {
        case 0x21://左键双击
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
        case 0x11://左键单击
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x41://左键按下
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x81://左键放开
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x22://右键双击
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
        case 0x12://右键单击
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x42://右键按下
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x82://右键放开
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x24://中键双击
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
        case 0x14://中键单击
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x44://中键按下
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x84://中键放开
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x08://单纯的鼠标移动
            mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
            break;
        }
        lisPacket.push_back(CPacket(5, NULL, 0));

        return 0;
    }

    int SendScreen(std::list<CPacket>& lisPacket, CPacket inPacket)
    {
        CImage screen;//GDI

        HDC hScreen = ::GetDC(NULL);//整个屏幕的设备上下文( HDC ), HDC 是一个句柄，表示绘图对象，用于绘制图形或截图
        int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);//24   ARGB8888 32bit // RGB888 24bit //RGB565  // RGB444
        int nWidth = GetDeviceCaps(hScreen, HORZRES); // 屏幕宽度
        int nHeight = GetDeviceCaps(hScreen, VERTRES);// 屏幕高度
        screen.Create(nWidth, nHeight, nBitPerPixel); // 创建一个空白 CImage ，大小与屏幕一致
        //复制屏幕像素到 CImage，screen.GetDC()是目标 HDC（绘制到 CImage），nHeight是 源HDC，SRCCOPY表示拷贝
        BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
        ReleaseDC(NULL, hScreen);//释放屏幕 HDC ，避免资源泄露

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);//创建一个可调整的全局内存对象，初始值为 0，用于最终存储 PNG 数据
        if (hMem == NULL)return -1;


        IStream* pStream = NULL;//创建 IStream（内存流，用于存储各种数据），用于操作 hMem 的内存，从而储存 PNG 数据
        // pStream 可以作为流来存储数据，而数据实际上存储在 hMem 中，下面的代码相当于绑定二者
        HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);//TRUE 自动释放 hMem（当 pStream 释放时，自动释放 hMem）

        // 测试生成图片的速度
        //for (int i = 0; i < 10; i++) {
        //    DWORD tick = GetTickCount64();//获取时间戳
        //    screen.Save(_T("test2020.png"), Gdiplus::ImageFormatPNG);
        //    TRACE("png %d\r\n", GetTickCount64() - tick);
        //    tick = GetTickCount64();
        //    screen.Save(_T("test2020.jpg"), Gdiplus::ImageFormatJPEG);
        //    TRACE("jpg %d\r\n", GetTickCount64() - tick) ;
        //}

        if (ret == S_OK) {
            screen.Save(pStream, Gdiplus::ImageFormatPNG);//数据实际上存储在 hMem中 ，用 PNG 耗的带宽更少一些
            LARGE_INTEGER bg = { 0 };// bg 表示偏移量为0，即pStream的头部
            pStream->Seek(bg, STREAM_SEEK_SET, NULL);//将流指针移动到开头，以便后续读取数据(之前保存的时候指针到末尾了)

            //获取全局内存 hMem的指针，使得指针可以直接访问其中的数据。转换为 PBYTE(BYTE*)，方便以字节方式操作数据
            PBYTE pData = (PBYTE)GlobalLock(hMem);
            SIZE_T nSize = GlobalSize(hMem);//数据大小
            lisPacket.push_back(CPacket(6, pData, nSize));
            //CPacket pack(6, pData, nSize);
            //CServerSocket::getInstance()->Send(pack);
            GlobalUnlock(hMem);
        }

        pStream->Release();
        GlobalFree(hMem);
        screen.ReleaseDC();
        return 0;
    }

    int LockMachine(std::list<CPacket>& lisPacket, CPacket inPacket)
    {
        //防止反复创建线程，已创建则不再创建
        if ((dlg.m_hWnd == NULL) || (dlg.m_hWnd == INVALID_HANDLE_VALUE)) {
            //_beginthread(threadLockDlg, 0, NULL);
            _beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);//使用这个函数才能获取线程号
            //Sleep(5000);  // 等待线程初始化对话框
            TRACE("threadid=%d\r\n", threadid);
        }
        lisPacket.push_back(CPacket(7, NULL, 0));
        return 0;

        //测试窗口
        //dlg.Create(IDD_DIALOG_INFO, NULL);//初始化 dlg（锁定对话框 CLockDialog）
        //dlg.ShowWindow(SW_SHOW);//显示对话框，非模态
        ////wndTopMost：让窗口始终保持置顶，SWP_NOSIZE | SWP_NOMOVE：不改变窗口大小，不改变窗口位置。
        //dlg.SetWindowPos(&dlg.wndTopMost, 0,0,0,0, SWP_NOSIZE | SWP_NOMOVE);
        ////ShowCursor(false);//隐藏鼠标
        //CRect rect;
        //dlg.GetWindowRect(rect);
        //rect.right = rect.left + 1;
        //rect.bottom = rect.top + 1;
        //ClipCursor(rect);  // 限制鼠标
        ////消息循环，没有消息循环对话框就会立即结束
        //MSG msg;
        //while (GetMessage(&msg, NULL, 0, 0)){
        //    TranslateMessage(&msg);//将 WM_KEYDOWN 这样的消息转换为字符消息（便于后续 DispatchMessage() 处理）
        //    DispatchMessage(&msg);//把消息传递给窗口的消息处理函数（例如 OnKeyDown()）
        //    if (msg.message == WM_KEYDOWN){//监听键盘按键
        //        //只有按下ESC键才推出
        //        TRACE("msg:%08X wparam:%08x lparam:%08x\r\n",msg.message,msg.wParam,msg.lParam);
        //        //break;
        //        if (msg.wParam == 0x1B)
        //            break;
        //    }
        //}
        //dlg.DestroyWindow();
        //ShowCursor(true);//隐藏鼠标
        //return 0;
    }

    int UnlockMachine(std::list<CPacket>& lisPacket, CPacket inPacket)
    {
        //dlg.SendMessage(WM_KEYDOWN, 0x41, 0x01E0001);
        //::SendMessage(dlg.m_hWnd, WM_KEYDOWN, 0x41, 0x01E0001);
        PostThreadMessage(threadid, WM_KEYDOWN, 0x41, 0);
        lisPacket.push_back(CPacket(8, NULL, 0));
        return 0;
    }

    int TestConnect(std::list<CPacket>& lisPacket, CPacket inPacket) {
        lisPacket.push_back(CPacket(1981, NULL, 0));
        return 0;
    }

    int DeleteLocalFile(std::list<CPacket>& lisPacket, CPacket inPacket)
    {
        std::string strPath = inPacket.strData;
        TCHAR sPath[MAX_PATH] = _T("");
        //mbstowcs(sPath, strPath.c_str(), strPath.size()); //中文容易乱码,多字节转宽字节
        MultiByteToWideChar(
            CP_ACP, 0, strPath.c_str(), strPath.size(), sPath,
            sizeof(sPath) / sizeof(TCHAR));
        DeleteFileA(strPath.c_str());
        lisPacket.push_back(CPacket(9, NULL, 0));
        //CPacket pack(9, NULL, 0);
        //bool ret = CServerSocket::getInstance()->Send(pack);
        //TRACE("Send ret = %d\r\n", ret);
        return 0;
    }

};

