#pragma once
class CTool
{
public:
    static void Dump(BYTE* pData, size_t nSize)
    {//以十六进制格式打印数据
        std::string strOut;
        for (size_t i = 0; i < nSize; i++)
        {
            char buf[8] = "";
            if (i > 0 && (i % 16 == 0))strOut += "\n";// 每行显示16个子节
            snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
            strOut += buf;
        }
        strOut += "\n";
        OutputDebugStringA(strOut.c_str());
    }

    static bool IsAdmin() {
        HANDLE hToken = NULL;//获取当前进程的 Token
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            ShowError();
            return false;
        }

        TOKEN_ELEVATION eve;
        DWORD len = 0;
        //GetTokenInformation() 查询 Token 是否提升（是否是管理员权限）
        if (GetTokenInformation(hToken, TokenElevation, &eve, sizeof(eve), &len) == FALSE) {
            ShowError();
            return false;
        }
        CloseHandle(hToken);

        if (len == sizeof(eve)) {
            return eve.TokenIsElevated;//返回 true 表示是管理员，false表示不是管理员
        }
        printf("length of tokeninformation is %d\r\n", len);
        return false;
    }

    //将普通用户提升至管理员
    static bool RunAsAdmin() {

        //HANDLE hToken =NULL;
        ////LOGON32_LOGON_BATCH    //    LOGON32_LOGON_INTERACTIVE
        //BOOL ret= LogonUser(L"Administrator", NULL, NULL, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken);
        //if (!ret) {
        //    ShowError();
        //    MessageBox(NULL, _T("登录错误"), _T("程序错误"), 0);
        //    exit(0);
        //}
        //OutputDebugString(L"Logon administrator success!\r\n");

        //本地安全策略：开启Administrator账户，禁止空密码只能登录本地控制台
        STARTUPINFO si = { 0 };
        PROCESS_INFORMATION pi = { 0 };
        TCHAR sPath[MAX_PATH] = _T("");
        //GetCurrentDirectory(MAX_PATH, sPath);//获取当前程序所在的目录
        //CString strCmd = sPath;strCmd += _T("\\RemoteControl.exe");

        GetModuleFileName(NULL, sPath, MAX_PATH);//获取完整路径(包括当前文件)
        // CreateProcessWithLogonW 以 Administrator 账号启动新的进程
        // LOGON_WITH_PROFILE 加载 Administrator 账户的用户环境变量  //  sPath 要运行的程序路径（当前程序本身）
        // 因为在管理员权限下才能向开始菜单开机启动的地方写入数据
        BOOL ret = CreateProcessWithLogonW(_T("Administrator"), NULL, NULL, LOGON_WITH_PROFILE, NULL, sPath, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi);
        if (!ret)
        {
            ShowError();
            MessageBox(NULL, sPath, _T("程序错误"), 0);
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }

    static void ShowError()
    {
        LPWSTR lpMessageBuf = NULL;// 用于存储错误消息，LPWSTR 是指向宽字符的指针
        //strerror(errno); //标准C语言库
        //获取系统错误信息
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            NULL, GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&lpMessageBuf, 0, NULL);
        OutputDebugString(lpMessageBuf);//输出错误信息
        MessageBox(NULL, lpMessageBuf, _T("发生错误"), 0);
        LocalFree(lpMessageBuf);
    }

    //开机启动的时候，程序的权限是跟随启动用户的，如果两者权限不一致，则会导致程序启动失败
    //开机启动对环境变量有影响，如果依赖d11（动态库），则可能启动失败。解决方法：
    // [复制这些d11到system32下面或者sysW0W64下面，system32下面多是64位程序而syswow64下面多是32位程序
    // [用静态库（编译后的可执行文件不依赖外部库），而动态库需要依赖]

    //将文件复制到系统开机启动目录下面
    static bool WriteStartupDir(const CString& strPath) {

        TCHAR sPath[MAX_PATH] = _T("");
        GetModuleFileName(NULL, sPath, MAX_PATH);//获取当前程序的完整目录存到sPath之中

        //把程序复制到开始菜单的路径里面，strPath是开始菜单的路径（需要管理员权限）
        return CopyFile(sPath, strPath, FALSE);
    }

    //修改注册表实现开机启动
    static bool WriteRegisterTable(const CString& strPath) {
        CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");//注册表的路径(前面HKEY_LOCAL_MACHINE\\删掉)
        //获取当前程序路径和系统目录
        TCHAR sPath[MAX_PATH] = _T("");
        char sSys[MAX_PATH] = "";
        GetModuleFileName(NULL, sPath, MAX_PATH);//获取当前程序的完整目录存到sPath
        //GetCurrentDirectoryA(MAX_PATH, sPath);  //获取当前程序所在目录(不包含当前文件)
        //GetSystemDirectoryA(sSys, sizeof(sSys));//获取系统路径

        BOOL ret = CopyFile(sPath, strPath, FALSE);//把程序复制到系统路径下面，strPath是系统路径的路径
        if (ret == FALSE)
        {
            MessageBox(NULL, _T("复制文件失败，是否权限不足？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }

        HKEY hKey = NULL;
        // 尝试打开注册表，KEY_WRITE 权限用于写入键值对 //必须加上 KEY_WOW64_64KEY，不然注册表写不进去
        ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubKey, 0, KEY_WRITE | KEY_WOW64_64KEY, &hKey);

        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            MessageBox(NULL, _T("设置自动开机启动失败！是否权限不足？\r\n程序启动失败！"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }

        //在注册表中设置开机启动项，注意长度是 strPath.GetLength()*sizeof(TCHAR)
        ret = RegSetValueEx(hKey, _T("RemoteControl"), 0, REG_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength() * sizeof(TCHAR));
        if (ret != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            MessageBox(NULL, _T("设置自动开机启动失败！是否权限不足？\r\n程序启动失败！"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
            return false;
        }

        RegCloseKey(hKey);
        return true;
    }


    //用于带mfc命令行项目初始化（通用）
    static bool Init() {
        HMODULE hModule = ::GetModuleHandle(nullptr);
        if (hModule == nullptr) {
            wprintf(L"错误:GetModuleHandle 失败\n");
            return false;
        }
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: 在此处为应用程序的行为编写代码。
            wprintf(L"错误: MFC 初始化失败\n");
            return false;
        }
        return true;
    }


};