
// RemoteClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "RemoteClient.h"
#include "RemoteClientDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// CRemoteClientDlg 对话框
CRemoteClientDlg::CRemoteClientDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_REMOTECLIENT_DIALOG, pParent)
	, m_server_address(0)
	, m_nPort(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRemoteClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_IPAddress(pDX, IDC_IPADDRESS_SERV, m_server_address);
	DDX_Text(pDX, IDC_EDIT_PORT, m_nPort);
	DDX_Control(pDX, IDC_TREE_DIR, m_Tree);
	DDX_Control(pDX, IDC_LIST_FILE, m_List);
}

BEGIN_MESSAGE_MAP(CRemoteClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_TEST, &CRemoteClientDlg::OnBnClickedBtnTest)
	ON_EN_CHANGE(IDC_EDIT_PORT, &CRemoteClientDlg::OnEnChangeEditPort)
	ON_BN_CLICKED(IDC_BTN_FILEINFO, &CRemoteClientDlg::OnBnClickedBtnFileinfo)
	ON_NOTIFY(NM_DBLCLK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMDblclkTreeDir)
	ON_NOTIFY(NM_CLICK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMClickTreeDir)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_FILE, &CRemoteClientDlg::OnNMRClickListFile)
	ON_COMMAND(ID_DOWNLOAD_FILE, &CRemoteClientDlg::OnDownloadFile)
	ON_COMMAND(ID_DELETE_FILE, &CRemoteClientDlg::OnDeleteFile)
	ON_COMMAND(ID_RUN_FILE, &CRemoteClientDlg::OnRunFile)
	//绑定 M_SEND_PACKET 消息 和 OnSendPacket 函数
	ON_MESSAGE(WM_SEND_PACKET,&CRemoteClientDlg::OnSendPacket)
	ON_BN_CLICKED(IDC_BTN_START_WATCH, &CRemoteClientDlg::OnBnClickedBtnStartWatch)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CRemoteClientDlg 消息处理程序

BOOL CRemoteClientDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	UpdateData();
	m_server_address = 0x7F000001;//127.0.0.1
	//m_server_address = 0xC0A83488;//192.168.52.136
	m_nPort = _T("9527");
	UpdateData(FALSE);

	m_isFull = false;
	
	//将根据对话框创建的类“绑定”为 IDD_DLG_STATUS（下载框）
	m_dlgStatus.Create(IDD_DLG_STATUS, this);
	m_dlgStatus.ShowWindow(SW_HIDE);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CRemoteClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

//如果向对话框添加最小化按钮，则需要下面的代码来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，这将由框架自动完成。
void CRemoteClientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标显示。
HCURSOR CRemoteClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

int CRemoteClientDlg::SendCommandPacket(int nCmd, bool bAutoClose, BYTE* pData, size_t nLength)
{
	UpdateData();
	CClientSocket* pClient = CClientSocket::getInstance();//不能重复创建套接字，不然原来的套接字就没有指针管理了，会造成内存泄漏
	bool ret = pClient->InitSocket(m_server_address, atoi((LPCTSTR)m_nPort));//connect
	if (!ret) {
		AfxMessageBox("网络初始化失败!");
		return -1;
	}
	CPacket pack(nCmd, pData, nLength);
	//封装数据包并发送给服务器，服务器通过DealCommand函数接收和处理，然后根据命令调用相关的函数，函数处理完后发送数据包返回客户端
	ret = pClient->Send(pack);
	TRACE("send ret: %d \r\n", ret);
	int cmd = pClient->DealCommand();//接收并处理数据，储存在 CPacket对象中
	//pClient->GetPacket();
	TRACE("ack:%d \r\n", cmd);
	if (bAutoClose)
		pClient->CloseSocket();
	return cmd;
}

void CRemoteClientDlg::OnBnClickedBtnTest()
{
	SendCommandPacket(1981);
}


void CRemoteClientDlg::OnEnChangeEditPort()
{
// TODO:  如果该控件是 RICHEDIT 控件，它将不发送此通知，除非重写 CDialogEx::OnInitDialog()函数并调用 CRichEditCtrl().SetEventMask()，同时将 ENM_CHANGE 标志“或”运算到掩码中。
// TODO:  在此添加控件通知处理程序代码
}

//显示磁盘信息
void CRemoteClientDlg::OnBnClickedBtnFileinfo()
{
	int ret = SendCommandPacket(1);//通知服务器调用1对应的函数
	if (ret == -1) {
		AfxMessageBox(_T("命令处理失败!!!"));
		return;
	}
	//发送信息的时候已经调用了磁盘相关的函数在对象中保存了信息，现在只需获取对象中的信息就可以了
	CClientSocket* pClient = CClientSocket::getInstance();
	std::string drivers = pClient->GetPacket().strData;
	std::string dr;
	m_Tree.DeleteAllItems();//清空
	for (size_t i = 0; i < drivers.size(); i++)
	{
		if (drivers[i] == ',') {
			dr += ":";//相当于把，换成 ：，即A:
			HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);//根目录，追加
			m_Tree.InsertItem("", hTemp, TVI_LAST);

			//m_Tree.InsertItem(NULL, hTemp, TVI_LAST);
			dr.clear();
			continue;
		}
		dr += drivers[i];
	}
	if (dr.size() > 0) {
		dr += ":";//相当于把，换成 ：，即A:
		HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);//根目录，追加
		m_Tree.InsertItem("", hTemp, TVI_LAST);
	}
}

void CRemoteClientDlg::OnBnClickedBtnStartWatch()
{
	m_isClosed = false;
	CWatchDialog dlg(this);//设置 parent 为 this
	HANDLE hThread = (HANDLE)_beginthread(CRemoteClientDlg::threadEntryForWatchData, 0, this);
	//GetDlgItem(IDC_BTN_START_WATCH)->EnableWindow(FALSE);//防止反复点
	dlg.DoModal();//设置监视窗口模态
	m_isClosed = true;
	WaitForSingleObject(hThread, 500);
}

void CRemoteClientDlg::threadEntryForWatchData(void* arg)
{
	CRemoteClientDlg* thiz = (CRemoteClientDlg*)arg;//获取 CRemoteClientDlg 对象
	//_beginthread(CRemoteClientDlg::threadEntryForDownFile, 0, this);//调用方式，在 OnDownloadFile 中
	thiz->threadWatchData();//通过对象直接调用成员函数
	_endthread();
}

void CRemoteClientDlg::threadWatchData()
{//可能存在异步问题，导致程序崩溃

	Sleep(50);//防止线程刚启动时系统资源未完全初始化
	CClientSocket* pClient = NULL;
	do {
		pClient = CClientSocket::getInstance();
	} while (pClient == NULL);//等待直到 getInstance() 返回有效指针

	while (m_isClosed==false)//窗口未关闭状态
	{
		// true ：表示当前已存有一帧图像，等待 UI 处理，不应继续请求数据 
		if (m_isFull == false) 
		{
			int ret = SendMessage(WM_SEND_PACKET, 6 << 1 | 1);//调用 OnSendPacket 函数
			if (ret == 6)
			{
				//解析返回的屏幕数据
				BYTE* pData = (BYTE*)pClient->GetPacket().strData.c_str();
				HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
				if (hMem == NULL) {
					TRACE("内存不足了！");
					Sleep(1);
					continue;
				}
				IStream* pStream = NULL;
				HRESULT hRet = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
				if (hRet == S_OK) {
					ULONG length = 0;
					pStream->Write(pData, pClient->GetPacket().strData.size(), &length);
					LARGE_INTEGER bg = { 0 };
					pStream->Seek(bg, STREAM_SEEK_SET, NULL);

					if ((HBITMAP)m_image != NULL)m_image.Destroy();

					m_image.Load(pStream);
					m_isFull = true;
				}
			}
			else {
				Sleep(1);//如果服务器未返回数据，Sleep(1)避免 CPU 100% 占用
			}
		}
		else Sleep(1);
	}
}
 
CString CRemoteClientDlg::GetPath(HTREEITEM hTree)
{
	CString strRet, strTmp;
	do {
		strTmp = m_Tree.GetItemText(hTree); // 获取上级目录（第一次是获取当前目录，然后向上获取），例如 file1
		strRet = strTmp + '\\' + strRet;    // 将上级目录加到当前目录上面,上一级的放在前面: FIle1;\\ now
		hTree = m_Tree.GetParentItem(hTree);// 获取上一级目录,更新 hTree
	} while (hTree != NULL);
	return strRet; // 返回完整路径
	return "";
}

void CRemoteClientDlg::DeleteTreeChildrenItem(HTREEITEM hTree)
{
	HTREEITEM hSub = NULL;
	do {
		hSub = m_Tree.GetChildItem(hTree);//获取子节点，直到子节点全部删除
		if (hSub != NULL)m_Tree.DeleteItem(hSub);
	} while (hSub != NULL);
}

void CRemoteClientDlg::LoadFileInfo()
{
	CPoint ptMouse;
	GetCursorPos(&ptMouse); // 获取鼠标当前位置
	m_Tree.ScreenToClient(&ptMouse);//屏幕坐标转化为客户端上面的坐标
	HTREEITEM hTreeSelected = m_Tree.HitTest(ptMouse, 0); // 检测鼠标点击的树控件项
	//直接返回就是不修改显示的内容，保持上次的样式
	if (hTreeSelected == NULL)return;//未选中任何目录，直接返回
	if (m_Tree.GetChildItem(hTreeSelected) == NULL)return;//没有子节点，说明是文件
	DeleteTreeChildrenItem(hTreeSelected);//防止在同一个节点点击多次重复添加子节点
	m_List.DeleteAllItems();
	//获取选中目录的完整路径（就是加上上级目录，这样得到文件来源）
	CString strPath = GetPath(hTreeSelected);
	// 发送命令请求服务器获取该目录下的文件列表，服务端解析数据，调用MackeDriverInfo()函数发送数据到客户端，客户端从而得到数据存于对象中
	SendCommandPacket(2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength());
	// 解析服务器返回的文件列表数据    // pInfo 指向目前获取的数据
	PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	CClientSocket* pClient = CClientSocket::getInstance();
	int count = 0;
	while (pInfo->HasNext) {//while 放在前面是因为可能第一个文件夹就是空的，过滤掉空文件的情况
		if (pInfo->IsDirectory) {
			//处理特殊情况
			if (CString(pInfo->szFileName) == "." || (CString(pInfo->szFileName) == ".."))
			{
				int cmd = pClient->DealCommand();//继续处理下一个事件
				TRACE("ack:%d\r\n", cmd);
				if (cmd < 0)break;
				pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
				continue;
			}
			//将当前目录 插入 hTreeSelected 下面，追加形式插入
			HTREEITEM hTemp = m_Tree.InsertItem(pInfo->szFileName, hTreeSelected, TVI_LAST);
			m_Tree.InsertItem("", hTemp, TVI_LAST);
		}
		else {//是文件的话
			m_List.InsertItem(0, pInfo->szFileName);
		}
		count++;
		//处理服务器的下一个数据包（因为数据可能一次发不完，要分成几次发送）
		int cmd = pClient->DealCommand(); // 处理服务器返回的命令
		TRACE("ack:%d\r\n", cmd);
		if (cmd < 0)break;
		//继续获取下一个 PFILEINFO 数据
		pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();//因为数据可能一次发不完，所以需要多次获取
	}
	TRACE("LoadFileInfo函数中的 count = %d \r\n", count);
	pClient->CloseSocket(); // 关闭连接
}


void CRemoteClientDlg::LoadFileCurrent()
{
	HTREEITEM hTree = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hTree);//获取选中目标的完整路径
	m_List.DeleteAllItems();
	// 发送命令请求服务器获取该目录下的文件列表，服务端解析数据，调用MackeDriverInfo()函数发送数据到客户端，客户端从而得到数据存于对象中
	SendCommandPacket(2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength());
	// 解析服务器返回的文件列表数据    // pInfo 指向目前获取的数据
	PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	CClientSocket* pClient = CClientSocket::getInstance();

	while (pInfo->HasNext) {//while 放在前面是因为可能第一个文件夹就是空的，过滤掉空文件的情况
		//这里只用更新文件列表，不用更新左边的文件目录
		if (!pInfo->IsDirectory)
			m_List.InsertItem(0, pInfo->szFileName);

		//处理服务器的下一个数据包（因为数据可能一次发不完，要分成几次发送）
		int cmd = pClient->DealCommand(); // 处理服务器返回的命令
		//TRACE("ack:%d\r\n", cmd);
		if (cmd < 0)break;
		//继续获取下一个 PFILEINFO 数据
		pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();//因为数据可能一次发不完，所以需要多次获取
	}

	pClient->CloseSocket(); // 关闭连接
}

void CRemoteClientDlg::OnNMDblclkTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	//LoadFileInfo();
}


void CRemoteClientDlg::OnNMClickTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
	LoadFileInfo();
}


void CRemoteClientDlg::OnNMRClickListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	*pResult = 0;
	CPoint ptMouse, ptList;
	GetCursorPos(&ptMouse);
	ptList = ptMouse;
	m_List.ScreenToClient(&ptList);
	int ListSelected = m_List.HitTest(ptList);
	if (ListSelected < 0)return;
	CMenu menu;
	menu.LoadMenu(IDR_MENU_RCLICK);
	CMenu* pPupup = menu.GetSubMenu(0);//
	if (pPupup != NULL) {
		pPupup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptMouse.x, ptMouse.y, this);
	}
}

void CRemoteClientDlg::OnDownloadFile()
{
	_beginthread(CRemoteClientDlg::threadEntryForDownFile, 0, this); // 开启线程
	BeginWaitCursor(); // 光标变为等待状态
	m_dlgStatus.m_info.SetWindowTextA(_T("命令正在执行中！"));
	m_dlgStatus.ShowWindow(SW_SHOW); // 显示状态对话框
	m_dlgStatus.CenterWindow(this);
	m_dlgStatus.SetActiveWindow(); // 使窗口获得焦点
}

void CRemoteClientDlg::threadEntryForDownFile(void* arg)//传入的参数就是 this
{
	CRemoteClientDlg* thiz = (CRemoteClientDlg*)arg;//获取 CRemoteClientDlg 对象
	//_beginthread(CRemoteClientDlg::threadEntryForDownFile, 0, this);//调用方式，在 OnDownloadFile 中
	thiz->threadDownFile();//通过对象直接调用成员函数
	_endthread();
}

void CRemoteClientDlg::threadDownFile()
{
	int nListSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nListSelected, 0);//0表示获取文件名，文件名在第一个

	//dig作用是什么
	CFileDialog dlg(FALSE, NULL, strFile,
		OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,//隐藏只读，
		NULL, this);

	if (dlg.DoModal() == IDOK) {
		FILE* pFile = fopen(dlg.GetPathName(), "wb+");//路径和权限
		if (pFile == NULL) {
			AfxMessageBox(_T("本地没有权限保存该文件，或者文件无法创建！！！"));
			m_dlgStatus.ShowWindow(SW_HIDE);
			EndWaitCursor();
			return;
		}
		HTREEITEM hSelected = m_Tree.GetSelectedItem();
		strFile = GetPath(hSelected) + strFile;
		TRACE("%s\r\n", LPCSTR(strFile));
		CClientSocket* pClient = CClientSocket::getInstance();

		//int ret = SendCommandPacket(4, false, (BYTE*)(LPCSTR)strFile, strFile.GetLength());

		//主线程收到消息后，调用 OnSendPacket() 处理。
		int ret = SendMessage(WM_SEND_PACKET, 4 << 1 | 0, (LPARAM)(LPCSTR)strFile);
		if (ret < 0)
		{
			AfxMessageBox(_T("执行下载命令失败！！"));
			TRACE(_T("执行下载失败：ret=%d\r\n"), ret);
			fclose(pFile);
			pClient->CloseSocket();
			return;
		}
		//nLength是文件大小 , C风格字符可以直接转化成数字吗,
		long long nLength = *(long long*)pClient->GetPacket().strData.c_str();
		if (nLength == 0)
		{
			AfxMessageBox(_T("文件长度为零或者无法读取文件！！！"));
			fclose(pFile);
			pClient->CloseSocket();
			return;
		}
		long long nCount = 0;
		while (nCount < nLength) {
			ret = pClient->DealCommand();
			if (ret < 0) {
				AfxMessageBox(_T("传输失败！！"));
				TRACE(_T("传输失败：ret = %d\r\n"), ret);
				break;
			}
			//分次写入文件
			fwrite(pClient->GetPacket().strData.c_str(), 1, pClient->GetPacket().strData.size(), pFile);
			nCount += pClient->GetPacket().strData.size();//文件要分次接收
		}

		fclose(pFile);
		pClient->CloseSocket();
	}
	m_dlgStatus.ShowWindow(SW_HIDE);
	EndWaitCursor();
	MessageBox(_T("下载完成！！"), _T("完成"));
}

void CRemoteClientDlg::OnDeleteFile()
{
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hSelected);
	int nSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nSelected, 0);
	strFile = strPath + strFile;
	int ret = SendCommandPacket(9, true, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
	if (ret < 0) {
		AfxMessageBox("删除文件命令执行失败！！！");
	}
	LoadFileCurrent();
}

void CRemoteClientDlg::OnRunFile()
{
	// TODO: 在此添加命令处理程序代码
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hSelected);
	int nSelected = m_List.GetSelectionMark();//获取当前选择的文件
	CString strFile = m_List.GetItemText(nSelected, 0);//获取文件名

	strFile = strPath + strFile;//完整路径
	int ret = SendCommandPacket(3, true, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
	if (ret < 0) {
		AfxMessageBox("打开文件命令执行失败！！！");
	}
}

// WM_SEND_PACKET 消息会调用这个函数
LRESULT CRemoteClientDlg::OnSendPacket(WPARAM wParam, LPARAM lParam)
{
	int ret = 0;
	int cmd = wParam >> 1;
	switch (cmd) {
		case 4:
		{
			CString strFile = (LPCSTR)lParam;
			ret = SendCommandPacket(cmd, wParam & 1, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
		}
		break;
		case 5: {//鼠标操作
			ret = SendCommandPacket(cmd, wParam & 1, (BYTE*)lParam, sizeof(MOUSEEV));
		}
		break;
		case 6:
		{
			ret = SendCommandPacket(cmd, wParam >> 1);
		}
		break;
		case 7:
		{

		}
		case 8:
		{
			ret = SendCommandPacket(cmd, wParam >> 1);
		}
		default:
			ret - 1;
	}
	return ret;
}


