#pragma once
#include "afxdialogex.h"

// CWatchDialog 对话框

class CWatchDialog : public CDialog
{
	DECLARE_DYNAMIC(CWatchDialog)

public:
	CWatchDialog(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CWatchDialog();

	void SetImageStatus(bool isFull = false) {
		m_isFull = isFull;
	}
	bool isFull() const {//const 表示函数中不能做任何成员变量的修改
		return m_isFull;
	}

	CImage m_image;//缓存，用于接收服务器(从被监控的电脑上)发送的图片
	CImage& GetImage() {
		return m_image;
	}
// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_WATCH };
#endif

protected:
	bool m_isFull;//缓存是否有数据 true表示有缓存数据false表示没有缓存数据
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	int m_nObjWidth;
	int m_nObjHeight;

	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	//对话框中显示的信息
	CStatic m_picture;

	afx_msg LRESULT OnSendPackAck(WPARAM wParam, LPARAM lParam);

	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	//这个函数是添加的窗口的控件
	CPoint UserPointToRemoteScreenPoint(CPoint& point,bool isScreen = false);
	//这个函数是窗口添加的事件处理函数
	afx_msg void OnStnClickedWatch();
	virtual void OnOK();
	afx_msg void OnBnClickedBtnLock();
	afx_msg void OnBnClickedBtnUnlock();
};
