#pragma once

#include <QMainWindow>
#include <QThread>
#include <QCloseEvent>
#include <QEvent>
#include <QObject>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>

#include "ui_comparewin.h"
#include "CmpareMode.h"
#include "StrategyCompare.h"
#include "rcglobal.h"
#include "blockuserdata.h"
#include "command.h"




#ifdef OPEN_UNDO_REDO

class ModifyOperRecords;
class OperatorRecord;
class UserDataOperRecords;
class ReplaceOperRecords;
#endif

class QToolBar;
class MediatorDisplay;
class StatusWidget;
class CompareWorker;

typedef struct lineDataInfo_ {

	//0没有 1 \n 2 :\r\n 左右的行结尾
	int8_t endType;


	//目前一块就是一行，块号就是行号。但是可能会有空块，而空块是没有行号的。
	//当lineNum为-1时，表示是空块
	//int blockNum; //块号
	int lineNum; //行号
	QString linedata;
	lineDataInfo_():lineNum(-1)
	{

	}

}LineDataInfo;

enum OperStatus {
	RC_NONE = 0,//空闲中
	RC_SYNC,//同步中
	RC_EDIT,//编辑中
};



/* 考虑再三，将每块的信息单独给列出来，放到这里收集起来 */
typedef struct UserBlockData_ {
	int blockNum;
	int lineEndType;
	int blockType; // 1:相等行 2 不等行 3 对齐行 （目前一行就是一块，暂时不严格区分块和行）

	int length; //长度 这两个用于生成对齐块中使用。只有对齐行才使用
	bool isFirst; //是否第一行
	bool isDel;
	UserBlockData_()
	{
		isFirst = false;
		isDel = false;
	}
	UserBlockData_(int blockType_, bool isFirst_)
	{
		blockType = blockType_;
		isFirst = isFirst_;
	}
	explicit UserBlockData_(int blockType_)
	{
		blockType = blockType_;
		isFirst = false;
	}

}UserBlockData;


class DiffStatusWin;

class CompareWin : public QMainWindow
{
	Q_OBJECT

public:
	CompareWin(QWidget *parent = Q_NULLPTR);
	CompareWin(QString leftFile, QString rightFile, QWidget * parent);
	virtual ~CompareWin();


	void openFile(int type);
	void setAtEditStatus(bool isInEdit);
	int loadFileToDisplay();


	//使用次数给统计起来
	static int s_useTimes;
	static int s_defaultFontSize;

	//virtual bool eventFilter(QObject* watched, QEvent* event);
#ifdef OPEN_UNDO_REDO
	void undoReplace(ReplaceOperRecords* record);
	void undoEdit(ModifyOperRecords* record);
	void undoSync(OperatorRecord* record);
	void undoUserData(UserDataOperRecords* userData);
	void autoAdjustLineUserData(RC_DIRECTION dir);
	void autoAdjustMarker(RC_DIRECTION dir);
	void autoAdjustMargins();
#endif
	static void createNonEqualBlock(QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInfo, QList<NoEqualBlock>& leftNoEqualBlocks, QList<NoEqualBlock>& rightNoEqualBlocks);
	static void megreUnEqualBlock(QList<NoEqualBlock>& leftNoEqualBlocks, QList<NoEqualBlock>& rightNoEqualBlocks);
	static bool isTextModeFile(QString filePath);
signals:
	void sendModify(int, ModifyRecords*);
	void sendCmpResultAtClose(int status,QString leftFilePath, QString rightFilePath);
	void signCmpFileClose();
	void receneFilePath(QString leftFilePath, QString rightFilePath);

protected:
	void closeEvent(QCloseEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* e) override;


public slots:
	void slot_openLeftFile();
	void slot_openRightFile();

	void slot_saveLeftFile();
	void slot_saveRightFile();
	void slot_doCmp();
	void slot_syncCurScrollValue(int value);
	void slot_syncCurScrollXValue(int direction);
	void slot_undoBt(bool);
	void slot_redoBt(bool);

#ifdef _DEBUG
	void check();
#endif

private slots:

	void slot_preBt(bool);

	void slot_nextBt(bool);
	void slot_swapBt(bool);
	void slot_fileCompareFinish();

	void slot_left_SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int);

	void slot_right_SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int);

	void slot_leftMarginClicked(int margin, int line, Qt::KeyboardModifiers state);

	void slot_rightMarginClicked(int margin, int line, Qt::KeyboardModifiers state);

	void slot_doCmpAfterEdit(int direction);

	void slot_whiteCharClick(bool checked);

	void slot_reloadBt();
	void slot_diffViewBt();
	void slot_findFile(bool);
	void slot_saveAsFile();
	void slot_searchDireChange(RC_DIRECTION dir);

	void slot_gotoLine();
	void slot_gotoEditLine(int dire, int lineNum); 
	void slot_rultBt();
	void slot_pullopenBt();
	void on_pullOpenLeft();
	void on_pullOpenRight();
	void slot_alignWork(int type, int start, int end);
	void slot_breakBt(bool check);
	void slot_strictBt(bool isStrict);
	void slot_tolerantBt(bool isTolerant);
	void slot_cmpModeChange(int mode,bool blankMatch, int equalRata);
	void slot_clearBt(bool);
	void slot_zoomin();
	void slot_zoomout();
	void slot_leftCodeChange(int code);
	void slot_rightCodeChange(int code);
	void slot_textChanged();
	void slot_doMemoryCmp();
	void slot_saveFile();
	void slot_outCmpMsg(int code, QString msg);
	void on_updateDiffView(int line, int pos);

	void on_leftPathReturnPressed();
	void on_rightPathReturnPressed();

	void on_leftSaveCodeChange(int index);
	void on_rightSaveCodeChange(int index);

private:
	void init();
	void clearInterStatus();
	void clearStatus();
	void enableEdit(bool enable);
	void saveFile(RC_DIRECTION direction);

	void setLastBlockFormat(QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInf);
	//int outputEditBlock(int direction, QVector<BlockNode>& blocks, QMap<int, UserBlockData>& blockStatus);
	void createNonEqualBlock(QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInfo);
	void clearMarker(RC_DIRECTION dir);
	void setMarker(RC_DIRECTION dir, QList<BlockUserData*>* externBlockInfo);

	void megreUnEqualBlock();
	void createMargins(QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInfo);
	void cleanMargins(RC_DIRECTION dir);
	void createBlocksInfoFromStringList(QStringList & strList, BlocksInfo & blocksInfo, QVector<LineFileInfo>& linesInfo);
	void delUserBlockData(QList<UserBlockData>& blockData, int start, int len);
	void enableTextChangeSignal();
	void disableTextChangeSignal();
	int getLineCompareStatus(BlockUserData* leftBlockData, BlockUserData* rightBlockData);
#ifdef OPEN_UNDO_REDO
	void cmpLineOneByOne(int lineNum, int leftStartLineNum, int rightStartLineNum, QsciDisplayWindow* leftSrc, QsciDisplayWindow* rightSrc, QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInfo, QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks);
#endif
	void outputMultiLineCompareResult(int leftStartLineNum, int leftLineLength, int externLeftLineLength, int rightStartLineNum, int rightLineLength, QsciDisplayWindow* leftSrc, QsciDisplayWindow* rightSrc, QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks);
	void getCompareUnqealPos(BlockNode& blocks, QVector<UnequalCharsPosInfo>& m_unqealPosList, int& blockType);
	void getCompareUnqealPos(QVector<BlockNode>& blocks, QStringList& content, QList<BlockUserData*>& externBlockInfo, QVector<UnequalCharsPosInfo>& m_unqealPosList, QVector<int>& blockTypes);
	void deleteLinesMarker(int dire, int startLine, int delNum);
	void filterCompareResult(QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks);
	void doCmpAfterEdit(RC_DIRECTION dir);
	void setUserDataMarkerFlag(int dire, int line, int status);
	void doMarginClicked(RC_DIRECTION dir, int margin, int line, Qt::KeyboardModifiers state);
	void goNoEqualPos(int index);

	RC_DIRECTION getUiRealDirection(RC_DIRECTION type);

	void asyncVerticalScrollValue(RC_DIRECTION dir);
	void setLineMarkers(QsciDisplayWindow * uiSrc, int line, int markMask);
    void replaceText(RC_DIRECTION dir, int start, int end, int textLen, const char* text);
	void signLineNoEqualText(int lineStart, int lineNums);
	void gotoViewLine(int lineNum);
	void showTips(QString text, int sec = 1500);
	void initFindWindow(RC_DIRECTION dir);
	bool enableSignTextChange(bool isOn);

	bool isNeedMemoryCmp();

	void loadFileToDisplay(RC_DIRECTION dir);
	void syncNeedSaveIcon();

	void removePadLines();
	void initDiffStatusDockWin();
	void updateDiffStatusWin(int maxLength, QList<NoEqualBlock>& diffLines);
	void updateDiffWinCurView();
	//void deleteLineDiffWin(int delLineNum);
	void doPullOpen(int dire);
private:
	Ui::CompareWin ui;

	QList<NoEqualBlock> m_leftNoEqualBlocks;
	QList<NoEqualBlock> m_rightNoEqualBlocks;

	int m_curShowBlockNums;

	bool m_isLeftDirty;
	bool m_isRightDirty;

	AbstractCompare *blockCompare;

	MediatorDisplay * m_mediator;

	//获取文件的文字编码
	CODE_ID m_leftCode;
	CODE_ID m_rightCode;

	//QList<LineFileInfo> m_leftLineIndex;
	//QList<LineFileInfo> m_rightLineIndex;

	OperStatus m_leftOperStatus;
	OperStatus m_rightOperStatus;

	CmpareMode* m_pCmpMode;

	QList<BlockUserData*>* m_leftExternBlockInfo;
	QList<BlockUserData*>* m_rightExternBlockInfo;


	//启动一个工作线程，用来做修改后的对比
	QThread m_workerThread;

	CompareWorker* m_compareWorker;

	QList<ModifyRecords*> m_leftModifyRecord;
	QList<ModifyRecords*> m_rightModifyRecord;

#ifdef OPEN_UNDO_REDO
	//记录操作历史，方便做undo与redo
	QList<Command*> m_undoList;

	int m_commandIndex;

#endif

	int m_leftRightOrder; //0 正序 1 反序

	QPixmap* m_leftPix;
	QPixmap* m_rightPix;

	//文件模式下的改变信号
	bool m_enableTextChangeSignal;

	//内存模式下的改变信号
	bool m_isTextChangeSignalOnMemoryMode;

	//空行是否参与行匹配
	bool m_isBlankLineDoMatch; //默认 true


	//对应radioButtonDefault控件 1 对应严格 2 对应忽略模式
	int m_cmpMode; // 0 默认忽略空白，行前行尾全部忽略 1:只忽略行尾 2:忽略行中的所有空白
	int m_srcCmpMode;

	//0 文件模式 1 内存模式（即直接拷贝文本到输入框进行对比）
	int m_workMode;


	int m_lineMatchEqualRata; //行认定匹配的相似率。90为超过90%则认定匹配，后续类似

	WORK_STATUS m_workStatus;

	bool m_isCmpFinished;
	volatile bool m_isCancel;
	QPointer<QMainWindow> m_pFindWin;
	QList<QString> m_findHistroy;

	QToolButton* m_strictBt;
	QToolButton* m_tolerantBt;

	QPointer<QDockWidget> m_dockDiffStatusWin;
	QPointer<DiffStatusWin> m_diffStatusWin;

	int m_zoomValue;
	int m_splitterLeftPos;
	int m_splitterRightPos;

	bool m_leftSaveCodeChange;
	bool m_rightSaveCodeChange;
};
