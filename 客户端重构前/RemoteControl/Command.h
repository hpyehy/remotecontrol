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

#pragma warning(disable:4966) // fopen sprintf strcpy strstr  ���� 4996 ����

class CCommand
{
public:
    CCommand();
    ~CCommand();
    int ExcuteCommand(int nCmd, std::list<CPacket>& lisPacket, CPacket inPacket);
    //���������
    static void RunCommand(void* arg, int status, std::list<CPacket>& lisPacket, CPacket inPacket) {
        CCommand* thiz = (CCommand*)arg;
        if (status > 0)
        {
            int ret = thiz->ExcuteCommand(status, lisPacket, inPacket);
            if (ret != 0) {
                TRACE("ִ������ʧ��:%d ret = %d\r\n", status, ret);
            }
        }
        else
            MessageBox(NULL, _T("�޷����������û����Զ�����"), _T("�����û�ʧ�ܣ�"), MB_OK | MB_ICONERROR);
    }

protected:
    typedef int(CCommand::* CMDFUNC)(std::list<CPacket>&, CPacket inPacket);//��Ա����ָ��
    std::map<int, CMDFUNC> m_mapFunction;//������ŵ�����ӳ��
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
        //if (!dlg.Create(IDD_DIALOG_INFO, NULL)) {  // ȷ���Ի��򴴽��ɹ�
        //    TRACE("Dialog creation failed!\n");
        //    return 0;
        //}
        dlg.ShowWindow(SW_SHOW);//��ʾ�Ի��򣬷�ģ̬
        //�ڱκ�̨����
        CRect rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = GetSystemMetrics(SM_CXFULLSCREEN);//���
        rect.bottom = GetSystemMetrics(SM_CYFULLSCREEN);//�߶�
        rect.bottom = LONG(rect.bottom * 1.10);//�Ի��������Ļ��ȷ���޷���������
        TRACE("right = %d bottom = %d\r\n", rect.right, rect.bottom);
        dlg.MoveWindow(rect);
        CWnd* pText = dlg.GetDlgItem(IDC_STATIC);//��ȡ�Ի����ϵ��ı��ؼ�
        if (pText) {
            CRect rtText;
            //��ȡ�ı������Ļ����λ�ã������� CRect���� rtText
            pText->GetWindowRect(rtText);
            //���þ�����ʾ
            int nWidth = rtText.Width();//�ı�����
            int x = (rect.right - nWidth) / 2;//�Ի���-�ı��� /2 �����ı�������Ͻ�λ��
            int nHeight = rtText.Height();
            int y = (rect.bottom - nHeight) / 2;
            pText->MoveWindow(x, y, rtText.Width(), rtText.Height());
        }

        //ȷ�� CLockInfoDialog �ö����޷����������ڸ���
        dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        //����ʾ���
        ShowCursor(false);
        //����������
        ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);

        dlg.GetWindowRect(rect);
        rect.left = 0;
        rect.top = 0;
        rect.right = 1;
        rect.bottom = 1;
        //��ֹ����ƶ�
        ClipCursor(rect);

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_KEYDOWN) {
                TRACE("msg:%08X wparam:%08x lparam:%08X\r\n", msg.message, msg.wParam, msg.lParam);
                if (msg.wParam == 0x41) {//����a�� �˳�  ESC��1B)
                    break;
                }
            }
        }
        //�ָ����
        ClipCursor(NULL);
        ShowCursor(true);
        //�ָ�������
        ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);
        dlg.DestroyWindow();
    }

    int MakeDriverInfo(std::list<CPacket>& lisPacket, CPacket inPacket) {
        std::string result;
        for (int i = 1; i <= 26; i++) {
            if (_chdrive(i) == 0) {// _chdrive(i) �жϴ��� i �Ƿ����
                if (result.size() > 0)
                    result += ',';
                result += 'A' + i - 1;
            }
        }
        lisPacket.push_back(CPacket(1, (BYTE*)result.c_str(), result.size()));
        //CPacket pack(1, (BYTE*)result.c_str(), result.size());//����ȡ�����ݷ�װ�ɶ���
        //CServerSocket::getInstance()->Send(pack);
        //CTool::Dump((BYTE*)pack.Data(), pack.Size());
        return 0;
    }

    // ���ڶ�ȡ�ļ���ȡ�ļ��б���ͻ��˷����ļ��� ����ÿ�ҵ�һ���ļ����ͷ���һ�Σ�ѭ������
    int MakeDirectoryInfo(std::list<CPacket>& lisPacket, CPacket inPacket) {
        std::string strPath = inPacket.strData;
        // ���Խ���Ŀ¼, _chdir �л���ָ��Ŀ¼��!= 0Ϊ�л�ʧ��
        if (_chdir(strPath.c_str()) != 0) {
            FILEINFO finfo;
            finfo.HasNext = FALSE;//��ʾĿ¼�޷�����
            //finfo.HasNext = TRUE;//��ʾĿ¼�޷�����
            //��װ�����ݰ� CPacket(2, (BYTE)&finfo, sizeof(finfo)) �����ͣ�֪ͨ�ͻ��˷���ʧ��
            lisPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
            OutputDebugString(_T("û��Ȩ�޷���Ŀ¼����"));
            return -2;
        }
        _finddata_t fdata;
        //int hfind = 0;
        intptr_t hfind = 0;//64λ����ϵͳʹ�� intptr_t
        //_findfirst ���ҵ�ǰĿ¼�µĵ�һ���ļ���fdata �洢���ҵ����ļ���Ϣ�����ļ��������Եȣ�
        //����ֵ hfind ��������������ں��� _findnext() ����������һ���ļ�
        if ((hfind = _findfirst("*", &fdata)) == -1) {
            OutputDebugString(_T("û���ҵ��κ��ļ�����"));
            FILEINFO finfo;
            finfo.HasNext = FALSE;
            //finfo.HasNext = TRUE;
            lisPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
            return -3;
        }

        //û�г�����ʼ��¼�ļ���Ϣ
        int count = 0;
        do {
            FILEINFO finfo;
            //�ж��Ƿ�ΪĿ¼��ͨ�� fdata.attrib & _A_SUBDIR ����subdir ����Ŀ¼
            finfo.IsDirectory = (fdata.attrib & _A_SUBDIR) != 0;
            memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));//�����ļ���
            TRACE("MakeDirectoryInfo������ӡ�ļ���Ϣ��finfo.szFileName��%s\r\n", finfo.szFileName);
            lisPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
            count++;
        } while (!_findnext(hfind, &fdata));
        //����ֵ��0���ɹ��ҵ���һ���ļ���������Ϣ���� fdata , -1 ��û���ҵ���һ���ļ����Ѿ������������ļ�������ִ���

        //ע�������������ļ�"."��".."�����Դ�ӡ����count��ʵ���ļ�������2
        TRACE("����˵�MakeDirectoryInfo���� count = %d\r\n", count);
        //�����δ������������ͻ�����ȷ֪ͨ�ļ��б��Ѿ��������
        FILEINFO finfo;
        finfo.HasNext = FALSE;// HasNext Ϊ false ˵��û����һ���ļ���
        lisPacket.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
        return 0;
    }

    int RunFile(std::list<CPacket>& lisPacket, CPacket inPacket) {
        std::string strPath = inPacket.strData;
        ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);  // �����ļ�
        lisPacket.push_back(CPacket(3, NULL, 0));

        return 0;
    }

    int DownloadFile(std::list<CPacket>& lisPacket, CPacket inPacket) {
        std::string strPath = inPacket.strData;
        long long data = 0;
        FILE* pFile = NULL;
        // ��ֻ��������ģʽ���ļ�
        // pFile������������洢�򿪵��ļ�ָ�룬"rb" ֻ��������
        errno_t err = fopen_s(&pFile, strPath.c_str(), "rb");
        // ��ʾ�ļ������ڣ�����0˵���ļ��򿪳ɹ���2��ENOENT���ļ������ڣ�13��EACCES����Ȩ�޷��ʣ�
        if (err != 0) { // ʧ�� ���� CPacket(4, 0, 8) 
            lisPacket.push_back(CPacket(4, (BYTE*)&data, 8));
            return -1;
        }

        if (pFile != NULL) {
            fseek(pFile, 0, SEEK_END);// ��ת���ļ�ĩβ�����ڻ�ȡ�ļ���С (0��ʾ���ƶ�)
            data = _ftelli64(pFile);  // ��ȡ�ļ���С
            lisPacket.push_back(CPacket(4, (BYTE*)&data, 8));

            fseek(pFile, 0, SEEK_SET);// ���ļ�ָ���ƻؿ�ͷ��׼����ȡ����
            char buffer[1024] = "";  // ���建����
            size_t rlen = 0;
            do {
                rlen = fread(buffer, 1, 1024, pFile);  // ��ȡ 1024 �ֽ�
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
        //    OutputDebugString(_T("��ȡ����������ʧ�ܣ���"));
        //    return -1;
        //}
        DWORD nFlags = 0;

        switch (mouse.nButton) {
        case 0://���
            nFlags = 1;
            break;
        case 1://�Ҽ�
            nFlags = 2;
            break;
        case 2://�м�
            nFlags = 4;
            break;
        case 4://û�а���
            nFlags = 8;
            break;
        }
        // ����ֻ������갴�µ���������δ���µ�������洦�����ﲻ���ظ�����
        // ������º��ͷŵ�λ�ò�һ�£����ܻ��������Ч���������϶����ڣ��϶��ļ���ʱ��
        if (nFlags != 8)SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);

        switch (mouse.nAction)
        {
        case 0://����
            nFlags |= 0x10;
            break;
        case 1://˫��
            nFlags |= 0x20;
            break;
        case 2://����
            nFlags |= 0x40;
            break;
        case 3://�ſ�
            nFlags |= 0x80;
            break;
        default:
            break;
        }

        TRACE("\r\n");
        TRACE("��������Ϊ : %08X x %d y %d\r\n", nFlags, mouse.ptXY.x, mouse.ptXY.y);
        TRACE("\r\n");

        switch (nFlags)
        {
        case 0x21://���˫��
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
        case 0x11://�������
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x41://�������
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x81://����ſ�
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x22://�Ҽ�˫��
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
        case 0x12://�Ҽ�����
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x42://�Ҽ�����
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x82://�Ҽ��ſ�
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x24://�м�˫��
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
        case 0x14://�м�����
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x44://�м�����
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x84://�м��ſ�
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case 0x08://����������ƶ�
            mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
            break;
        }
        lisPacket.push_back(CPacket(5, NULL, 0));

        return 0;
    }

    int SendScreen(std::list<CPacket>& lisPacket, CPacket inPacket)
    {
        CImage screen;//GDI

        HDC hScreen = ::GetDC(NULL);//������Ļ���豸������( HDC ), HDC ��һ���������ʾ��ͼ�������ڻ���ͼ�λ��ͼ
        int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);//24   ARGB8888 32bit // RGB888 24bit //RGB565  // RGB444
        int nWidth = GetDeviceCaps(hScreen, HORZRES); // ��Ļ���
        int nHeight = GetDeviceCaps(hScreen, VERTRES);// ��Ļ�߶�
        screen.Create(nWidth, nHeight, nBitPerPixel); // ����һ���հ� CImage ����С����Ļһ��
        //������Ļ���ص� CImage��screen.GetDC()��Ŀ�� HDC�����Ƶ� CImage����nHeight�� ԴHDC��SRCCOPY��ʾ����
        BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
        ReleaseDC(NULL, hScreen);//�ͷ���Ļ HDC ��������Դй¶

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);//����һ���ɵ�����ȫ���ڴ���󣬳�ʼֵΪ 0���������մ洢 PNG ����
        if (hMem == NULL)return -1;


        IStream* pStream = NULL;//���� IStream���ڴ��������ڴ洢�������ݣ������ڲ��� hMem ���ڴ棬�Ӷ����� PNG ����
        // pStream ������Ϊ�����洢���ݣ�������ʵ���ϴ洢�� hMem �У�����Ĵ����൱�ڰ󶨶���
        HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);//TRUE �Զ��ͷ� hMem���� pStream �ͷ�ʱ���Զ��ͷ� hMem��

        // ��������ͼƬ���ٶ�
        //for (int i = 0; i < 10; i++) {
        //    DWORD tick = GetTickCount64();//��ȡʱ���
        //    screen.Save(_T("test2020.png"), Gdiplus::ImageFormatPNG);
        //    TRACE("png %d\r\n", GetTickCount64() - tick);
        //    tick = GetTickCount64();
        //    screen.Save(_T("test2020.jpg"), Gdiplus::ImageFormatJPEG);
        //    TRACE("jpg %d\r\n", GetTickCount64() - tick) ;
        //}

        if (ret == S_OK) {
            screen.Save(pStream, Gdiplus::ImageFormatPNG);//����ʵ���ϴ洢�� hMem�� ���� PNG �ĵĴ������һЩ
            LARGE_INTEGER bg = { 0 };// bg ��ʾƫ����Ϊ0����pStream��ͷ��
            pStream->Seek(bg, STREAM_SEEK_SET, NULL);//����ָ���ƶ�����ͷ���Ա������ȡ����(֮ǰ�����ʱ��ָ�뵽ĩβ��)

            //��ȡȫ���ڴ� hMem��ָ�룬ʹ��ָ�����ֱ�ӷ������е����ݡ�ת��Ϊ PBYTE(BYTE*)���������ֽڷ�ʽ��������
            PBYTE pData = (PBYTE)GlobalLock(hMem);
            SIZE_T nSize = GlobalSize(hMem);//���ݴ�С
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
        //��ֹ���������̣߳��Ѵ������ٴ���
        if ((dlg.m_hWnd == NULL) || (dlg.m_hWnd == INVALID_HANDLE_VALUE)) {
            //_beginthread(threadLockDlg, 0, NULL);
            _beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);//ʹ������������ܻ�ȡ�̺߳�
            //Sleep(5000);  // �ȴ��̳߳�ʼ���Ի���
            TRACE("threadid=%d\r\n", threadid);
        }
        lisPacket.push_back(CPacket(7, NULL, 0));
        return 0;

        //���Դ���
        //dlg.Create(IDD_DIALOG_INFO, NULL);//��ʼ�� dlg�������Ի��� CLockDialog��
        //dlg.ShowWindow(SW_SHOW);//��ʾ�Ի��򣬷�ģ̬
        ////wndTopMost���ô���ʼ�ձ����ö���SWP_NOSIZE | SWP_NOMOVE�����ı䴰�ڴ�С�����ı䴰��λ�á�
        //dlg.SetWindowPos(&dlg.wndTopMost, 0,0,0,0, SWP_NOSIZE | SWP_NOMOVE);
        ////ShowCursor(false);//�������
        //CRect rect;
        //dlg.GetWindowRect(rect);
        //rect.right = rect.left + 1;
        //rect.bottom = rect.top + 1;
        //ClipCursor(rect);  // �������
        ////��Ϣѭ����û����Ϣѭ���Ի���ͻ���������
        //MSG msg;
        //while (GetMessage(&msg, NULL, 0, 0)){
        //    TranslateMessage(&msg);//�� WM_KEYDOWN ��������Ϣת��Ϊ�ַ���Ϣ�����ں��� DispatchMessage() ����
        //    DispatchMessage(&msg);//����Ϣ���ݸ����ڵ���Ϣ������������ OnKeyDown()��
        //    if (msg.message == WM_KEYDOWN){//�������̰���
        //        //ֻ�а���ESC�����Ƴ�
        //        TRACE("msg:%08X wparam:%08x lparam:%08x\r\n",msg.message,msg.wParam,msg.lParam);
        //        //break;
        //        if (msg.wParam == 0x1B)
        //            break;
        //    }
        //}
        //dlg.DestroyWindow();
        //ShowCursor(true);//�������
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
        //mbstowcs(sPath, strPath.c_str(), strPath.size()); //������������,���ֽ�ת���ֽ�
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

