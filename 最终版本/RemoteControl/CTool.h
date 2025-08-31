#pragma once
class CTool
{
public:
    static void Dump(BYTE* pData, size_t nSize)
    {//��ʮ�����Ƹ�ʽ��ӡ����
        std::string strOut;
        for (size_t i = 0; i < nSize; i++)
        {
            char buf[8] = "";
            if (i > 0 && (i % 16 == 0))strOut += "\n";// ÿ����ʾ16���ӽ�
            snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
            strOut += buf;
        }
        strOut += "\n";
        OutputDebugStringA(strOut.c_str());
    }

    static bool IsAdmin() {
        HANDLE hToken = NULL;//��ȡ��ǰ���̵� Token
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            ShowError();
            return false;
        }

        TOKEN_ELEVATION eve;
        DWORD len = 0;
        //GetTokenInformation() ��ѯ Token �Ƿ��������Ƿ��ǹ���ԱȨ�ޣ�
        if (GetTokenInformation(hToken, TokenElevation, &eve, sizeof(eve), &len) == FALSE) {
            ShowError();
            return false;
        }
        CloseHandle(hToken);

        if (len == sizeof(eve)) {
            return eve.TokenIsElevated;//���� true ��ʾ�ǹ���Ա��false��ʾ���ǹ���Ա
        }
        printf("length of tokeninformation is %d\r\n", len);
        return false;
    }

    //����ͨ�û�����������Ա
    static bool RunAsAdmin() {

        //HANDLE hToken =NULL;
        ////LOGON32_LOGON_BATCH    //    LOGON32_LOGON_INTERACTIVE
        //BOOL ret= LogonUser(L"Administrator", NULL, NULL, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken);
        //if (!ret) {
        //    ShowError();
        //    MessageBox(NULL, _T("��¼����"), _T("�������"), 0);
        //    exit(0);
        //}
        //OutputDebugString(L"Logon administrator success!\r\n");

        //���ذ�ȫ���ԣ�����Administrator�˻�����ֹ������ֻ�ܵ�¼���ؿ���̨
        STARTUPINFO si = { 0 };
        PROCESS_INFORMATION pi = { 0 };
        TCHAR sPath[MAX_PATH] = _T("");
        //GetCurrentDirectory(MAX_PATH, sPath);//��ȡ��ǰ�������ڵ�Ŀ¼
        //CString strCmd = sPath;strCmd += _T("\\RemoteControl.exe");

        GetModuleFileName(NULL, sPath, MAX_PATH);//��ȡ����·��(������ǰ�ļ�)
        // CreateProcessWithLogonW �� Administrator �˺������µĽ���
        // LOGON_WITH_PROFILE ���� Administrator �˻����û���������  //  sPath Ҫ���еĳ���·������ǰ������
        // ��Ϊ�ڹ���ԱȨ���²�����ʼ�˵����������ĵط�д������
        BOOL ret = CreateProcessWithLogonW(_T("Administrator"), NULL, NULL, LOGON_WITH_PROFILE, NULL, sPath, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
        if (!ret)
        {
            ShowError();
            MessageBox(NULL, sPath, _T("�������"), 0);
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }

    static void ShowError()
    {
        LPWSTR lpMessageBuf = NULL;// ���ڴ洢������Ϣ��LPWSTR ��ָ����ַ���ָ��
        //strerror(errno); //��׼C���Կ�
        //��ȡϵͳ������Ϣ
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            NULL, GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&lpMessageBuf, 0, NULL);
        OutputDebugString(lpMessageBuf);//���������Ϣ
        MessageBox(NULL, lpMessageBuf, _T("��������"), 0);
        LocalFree(lpMessageBuf);
    }

    //����������ʱ�򣬳����Ȩ���Ǹ��������û��ģ��������Ȩ�޲�һ�£���ᵼ�³�������ʧ��
    //���������Ի���������Ӱ�죬�������d11����̬�⣩�����������ʧ�ܡ����������
    // [������Щd11��system32�������sysW0W64���棬system32�������64λ�����syswow64�������32λ����
    // [�þ�̬�⣨�����Ŀ�ִ���ļ��������ⲿ�⣩������̬����Ҫ����]

    //���ļ����Ƶ�ϵͳ��������Ŀ¼����
    static bool WriteStartupDir(const CString& strPath) {

        TCHAR sPath[MAX_PATH] = _T("");
        GetModuleFileName(NULL, sPath, MAX_PATH);//��ȡ��ǰ���������Ŀ¼�浽sPath֮��

        //�ѳ����Ƶ���ʼ�˵���·�����棬strPath�ǿ�ʼ�˵���·������Ҫ����ԱȨ�ޣ�
        return CopyFile(sPath, strPath, FALSE);
    }

    //�޸�ע���ʵ�ֿ�������
    static bool WriteRegisterTable(const CString& strPath) {
        CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");//ע����·��(ǰ��HKEY_LOCAL_MACHINE\\ɾ��)
        //��ȡ��ǰ����·����ϵͳĿ¼
        TCHAR sPath[MAX_PATH] = _T("");
        char sSys[MAX_PATH] = "";
        GetModuleFileName(NULL, sPath, MAX_PATH);//��ȡ��ǰ���������Ŀ¼�浽sPath
        //GetCurrentDirectoryA(MAX_PATH, sPath);  //��ȡ��ǰ��������Ŀ¼(��������ǰ�ļ�)
        //GetSystemDirectoryA(sSys, sizeof(sSys));//��ȡϵͳ·��

        BOOL ret = CopyFile(sPath, strPath, FALSE);//�ѳ����Ƶ�ϵͳ·�����棬strPath��ϵͳ·����·��
        if (ret == FALSE)
        {
            MessageBox(NULL, _T("�����ļ�ʧ�ܣ��Ƿ�Ȩ�޲��㣿\r\n"), _T("����"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }

        HKEY hKey = NULL;
        // ���Դ�ע���KEY_WRITE Ȩ������д���ֵ�� //������� KEY_WOW64_64KEY����Ȼע���д����ȥ
        ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubKey, 0, KEY_WRITE | KEY_WOW64_64KEY, &hKey);

        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            MessageBox(NULL, _T("�����Զ���������ʧ�ܣ��Ƿ�Ȩ�޲��㣿\r\n��������ʧ�ܣ�"), _T("����"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }

        //��ע��������ÿ��������ע�ⳤ���� strPath.GetLength()*sizeof(TCHAR)
        ret = RegSetValueEx(hKey, _T("RemoteControl"), 0, REG_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength() * sizeof(TCHAR));
        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            MessageBox(NULL, _T("�����Զ���������ʧ�ܣ��Ƿ�Ȩ�޲��㣿\r\n��������ʧ�ܣ�"), _T("����"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }

        RegCloseKey(hKey);
        return true;
    }


    //���ڴ�mfc��������Ŀ��ʼ����ͨ�ã�
    static bool Init() {
        HMODULE hModule = ::GetModuleHandle(nullptr);
        if (hModule == nullptr) {
            wprintf(L"����:GetModuleHandle ʧ��\n");
            return false;
        }
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: �ڴ˴�ΪӦ�ó������Ϊ��д���롣
            wprintf(L"����: MFC ��ʼ��ʧ��\n");
            return false;
        }
        return true;
    }


};