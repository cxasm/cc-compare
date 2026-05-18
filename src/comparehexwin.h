#pragma once

#include <QMainWindow>
#include "ui_comparehexwin.h"
#include "progresswin.h"
#include "CmpareMode.h"

#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>

class MediatorDisplay;

class CompareHexWin : public QMainWindow
{
	Q_OBJECT

public:
	CompareHexWin(QWidget *parent = Q_NULLPTR);
	CompareHexWin(QString leftFile, QString rightFile, QWidget * parent);
	~CompareHexWin();

public slots:
	void slot_openLeftFile();
	void slot_openRightFile();

	void slot_doCmp();
	void slot_outMsgCmpMode(int code, QString msg);
	void slot_cmpMoveStep();

protected:
	//void closeEvent(QCloseEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* e) override;



private slots:
	void slot_fileCompareFinish();
	void slot_syncCurScrollValue(int direction);
	void slot_syncCurScrollXValue(int direction);
	void slot_clearBt(bool);
	void slot_cmpInfoBt(bool);
	void slot_cancelCmp();
	void slot_swapBt(bool);
	void slot_rultBt(bool);
	void slot_cmpModeChange(int mode, int highlightBack);
	void slot_reloadBt(bool);
	void on_preBt();
	void on_nextBt();
private:
	void openFile(int type);
	void clearStatus();
	void init();
	void setLineDisplayMode(int mode);
	void doCmp(int workMode);

private:
	BinLcsResult *m_blockCompare;
	BinLcsResult m_lastResult;
	MediatorDisplay * m_mediator;

private:
	Ui::CompareHexWin ui;
	ProgressWin* m_loadFileProcessWin;

	WORK_STATUS m_workStatus;

	int m_leftRightOrder;
	int m_cmpMode;
	int m_isHighLightBackgroud;

	qint64 m_leftStartPos;
	qint64 m_rightStartPos;
	int m_leftCmpLen;
	int m_rightCmpLen;
};

