
// RemoteClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "RemoteClient.h"
#include "RemoteClientDlg.h"
#include "afxdialogex.h"
#include "ClientController.h"

#ifndef WM_SEND_PACK_ACK
#define WM_SEND_PACK_ACK (WM_USER+2)//发送包数据应答
#endif
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

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
	//绑定 M_SEND_PACKET 消息 和 OnSendPackAck 函数
	//ON_MESSAGE(WM_SEND_PACKET,&CRemoteClientDlg::OnSendPackAck)
	ON_BN_CLICKED(IDC_BTN_START_WATCH, &CRemoteClientDlg::OnBnClickedBtnStartWatch)
	ON_WM_TIMER()
	ON_NOTIFY(IPN_FIELDCHANGED, IDC_IPADDRESS_SERV, &CRemoteClientDlg::OnIpnFieldchangedIpaddressServ)
	ON_MESSAGE(WM_SEND_PACK_ACK, &CRemoteClientDlg::OnSendPackAck)
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

	InitUIData();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CRemoteClientDlg::InitUIData()
{
	UpdateData();
	m_server_address = 0x7F000001;//127.0.0.1
	//m_server_address = 0xC0A83488;//192.168.52.136
	m_nPort = _T("9527");

	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));

	UpdateData(FALSE);

	//将根据对话框创建的类“绑定”为 IDD_DLG_STATUS（下载框）
	m_dlgStatus.Create(IDD_DLG_STATUS, this);
	m_dlgStatus.ShowWindow(SW_HIDE);
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
	//UpdateData();
	//CClientController* pController = CClientController::getInstance();
	//pController->UpdateAddress(m_server_address,atoi((LPCTSTR)m_nPort));


	//////return CClientController::getInstance()->SendCommandPacket(nCmd, bAutoClose, pData, nLength);

	//CClientSocket* pClient = CClientSocket::getInstance();//不能重复创建套接字，不然原来的套接字就没有指针管理了，会造成内存泄漏
	
	//bool ret = pClient->InitSocket();//connect
	//bool ret = pClient->InitSocket(m_server_address, atoi((LPCTSTR)m_nPort));//connect
	//CPacket pack(nCmd, pData, nLength);	

	//MSG msg;
	//memset(&msg, 0, sizeof(MSG));
	//msg.message = WM_SEND_PACK;
	//msg.wParam = (WPARAM)&pack;
	//bool ret = pController->SendMessage(msg);
	//if (!ret) {
	//	MessageBox("发送消息失败，网络初始化失败!");
	//	return -1;
	//}
	return 0;
}

void CRemoteClientDlg::OnBnClickedBtnTest()
{
	CClientController::getInstance()->CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 1981);
}

//显示磁盘信息
void CRemoteClientDlg::OnBnClickedBtnFileinfo()
{
	//std::list<CPacket> lstPackets;
	//发送命令并且获取响应数据
	bool ret = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 1, true, NULL, 0 );
	if (ret == 0) {
		AfxMessageBox(_T("命令处理失败!!!"));
		return;
	}
}

//远程监控屏幕
void CRemoteClientDlg::OnBnClickedBtnStartWatch()
{
	CClientController::getInstance()->StartWatchScreen();
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
	TRACE("hTreeSelected %08X\r\n", hTreeSelected);
	//直接返回就是不修改显示的内容，保持上次的样式
	if (hTreeSelected == NULL)return;//未选中任何目录，直接返回
	//没有子节点(子文件夹)则返回,但是这样会导致没有子文件夹的文件夹只有第一次点击有效
	//if (m_Tree.GetChildItem(hTreeSelected) == NULL)return;
	
	DeleteTreeChildrenItem(hTreeSelected);//防止在同一个节点点击多次重复添加子节点
	m_List.DeleteAllItems();
	//获取选中目录的完整路径（就是加上上级目录，这样得到文件来源）
	CString strPath = GetPath(hTreeSelected);
	TRACE("hTreeSelected %08X\r\n", hTreeSelected);

	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 2, false,
		(BYTE*)(LPCTSTR)strPath, strPath.GetLength(),(WPARAM)hTreeSelected);
}


void CRemoteClientDlg::LoadFileCurrent()
{
	HTREEITEM hTree = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hTree);//获取选中目标的完整路径
	m_List.DeleteAllItems();
	// 发送命令请求服务器获取该目录下的文件列表，服务端解析数据，调用MackeDriverInfo()函数发送数据到客户端，客户端从而得到数据存于对象中
	// SendCommandPacket(2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength());
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength());
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

	//CClientController::getInstance()->CloseSocket(); // 关闭连接
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
	//获取文件
	int nListSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nListSelected, 0);//0表示获取文件名，文件名在第一个
	//获取路径
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	strFile = GetPath(hSelected) + strFile;
	int ret = CClientController::getInstance()->DownFile(strFile);

	if (ret != 0) {
		MessageBox(_T("下载失败！"));
		TRACE("下载失败ret=%d \r\n", ret);
	}
	//BeginWaitCursor(); // 光标变为等待状态

}

void CRemoteClientDlg::OnDeleteFile()
{
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hSelected);
	int nSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nSelected, 0);
	strFile = strPath + strFile;
	int ret = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 9, true, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
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
	int ret = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 3, true, (BYTE*)(LPCSTR)strFile, strFile.GetLength());
	if (ret < 0) {
		AfxMessageBox("打开文件命令执行失败！！！");
	}
}

//IP发生变化就会调用函数
void CRemoteClientDlg::OnIpnFieldchangedIpaddressServ(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMIPADDRESS pIPAddr = reinterpret_cast<LPNMIPADDRESS>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
	UpdateData();
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));
}


void CRemoteClientDlg::OnEnChangeEditPort()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不发送此通知，除非重写 CDialogEx::OnInitDialog()函数并调用 CRichEditCtrl().SetEventMask()，同时将 ENM_CHANGE 标志“或”运算到掩码中。
	// TODO:  在此添加控件通知处理程序代码
	UpdateData();
	CClientController* pController = CClientController::getInstance();
	pController->UpdateAddress(m_server_address, atoi((LPCTSTR)m_nPort));
}

//显示磁盘信息
void CRemoteClientDlg::StrToTree(const std::string& drivers, CTreeCtrl& tree)
{
	std::string dr;
	tree.DeleteAllItems();//清空
	for (size_t i = 0; i < drivers.size(); i++)
	{
		if (drivers[i] == ',') {
			dr += ":";//相当于把，换成 ：，即A:
			HTREEITEM hTemp = tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);//根目录，追加
			tree.InsertItem("", hTemp, TVI_LAST);
			//tree.InsertItem(NULL, hTemp, TVI_LAST);
			dr.clear();
			continue;
		}
		dr += drivers[i];
	}
	if (dr.size() > 0) {
		dr += ":";//相当于把，换成 ：，即A:
		HTREEITEM hTemp = tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);//根目录，追加
		tree.InsertItem("", hTemp, TVI_LAST);
	}
}

//更新文件信息
void CRemoteClientDlg::UpdateFileInfo(const FILEINFO& finfo, HTREEITEM hParent)
{
	TRACE("hasnext: %d     isdirectory: %d %s\r\n", finfo.HasNext, finfo.IsDirectory, finfo.szFileName);
	if (finfo.HasNext == false)return;
	if (finfo.IsDirectory) {
		//处理特殊情况(即忽略这两种情况)
		if ((CString(finfo.szFileName) == ".") || (CString(finfo.szFileName) == ".."))return;

		//将当前目录 插入 hTreeSelected 下面，追加形式插入
		HTREEITEM hTem = m_Tree.InsertItem(finfo.szFileName, (HTREEITEM)hParent, TVI_LAST);
		m_Tree.InsertItem("", hTem, TVI_LAST);
		m_Tree.Expand((HTREEITEM)hParent, TVE_EXPAND);
	}
	else {//是文件的话
		m_List.InsertItem(0, finfo.szFileName);
	}
}

void CRemoteClientDlg::UpdateDownloadFile(const std::string& strData, FILE* pFile)
{
	static LONGLONG length = 0, index = 0;
	//第一次读取，之后直到下载完成，length都不是0
	if (length == 0) {
		length = *(long long*)strData.c_str();//获取文件长度
		if (length == 0) {
			AfxMessageBox("文件长度为零或者无法读取文件！！！");
			CClientController::getInstance()->DownloadEnd();
		}
	}
	else {// >0 ,可以写数据了
		// 1 表示写入方式为 一次写一个字节的内容，防止出问题
		fwrite(strData.c_str(), 1, strData.size(), pFile);
		index += strData.size();//index为文件中当前数据量
		TRACE("index %d\r\n", index);
		//最后一次，没有这个，下载界面不会结束，文件不会关闭（我们也就无法打开）
		if (index >= length) {
			fclose(pFile);
			length = 0;
			index = 0;
			CClientController::getInstance()->DownloadEnd();
		}
	}
}

void CRemoteClientDlg::DealCommand(WORD nCmd, const std::string& strData, LPARAM lParam) 
{
	switch (nCmd) {
	case 1://获取驱动信息
	{
		StrToTree(strData, m_Tree);
	}
	break;
	case 2:
	{
		// pInfo 指向目前获取的数据 , PFILEINFO 就是一个指针类型
		PFILEINFO pInfo = (PFILEINFO)strData.c_str();
		UpdateFileInfo(*pInfo, (HTREEITEM)lParam);//这里为什么要解引用
	}
	break;
	case 3: //打开文件
		TRACE("run file done!\r\n");
		break;
	case 4://下载
		UpdateDownloadFile(strData, (FILE*)lParam);
		break;
	case 9:
		MessageBox("删除完成！", "删除文件成功", MB_ICONINFORMATION);
		break;
	case 1981:
		MessageBox("连接测试成功！","连接成功", MB_ICONINFORMATION);
		break;
	default:
		TRACE("输入命令无效 %d\r\n", nCmd);
		break;
	}

}

LRESULT CRemoteClientDlg::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{
	if (lParam == -1 || lParam == -2)
		TRACE("连接错误\r\n");
	else if (lParam == 1)
		TRACE("对方关闭套接字 \r\n");
	else 
	{
		CPacket* pPacket = (CPacket*)wParam;//发送回来的数据都会存入 wParam	
		if (pPacket != NULL) {
			CPacket head = *pPacket;
			delete pPacket;
			DealCommand(head.sCmd,head.strData, lParam);
		}
	}
	return 0;
}