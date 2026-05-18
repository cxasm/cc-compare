#pragma once

#include <QWidget>
#include <QMainWindow>
#include <QFileInfoList>
#include <QContextMenuEvent>
#include <functional>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>

#include "ui_CompareDirs.h"
#include "comparedirmode.h"
#include "rcglobal.h"

class MediatorFileTree;
class QTreeWidgetItem;
class DirFindFile;
class ProgressWin;
class QLabel;
struct CmpWalkFileInfo;
class CompareDirsData;

//不要被名称迷惑，其实文件夹也是用下面来标记的。
enum ITEM_FILE_STATUS {
	NO_USER = -1,//默认没有设置，就是这个
	EQUAL_FILE = 0,//相等
	NO_EQUAL_FILE,
	ONLY_ONE_SIDE_FILE, //独有
	LOST_SIDE_FILE, //匹配独有，是缺失的文件
	DELETED_FILE //已经删除掉

};

class CompareDirs :public QMainWindow
{
	Q_OBJECT

public:
	CompareDirs(QWidget *parent = Q_NULLPTR);
	virtual ~CompareDirs();



	void contextUserDefineItemMenuEvent(int dire, QMenu* menu, QTreeWidgetItem* curItem);

	static QString getFileSizeFormat(qint64 size);
	static QString fileSuffix(const QString& filePath);

	void setCmpFile(QString leftFile, QString rightFile);
	static ITEM_FILE_STATUS getItemEqualStatus(QTreeWidgetItem* item);

signals:
	void delayWork(int dir, QString path);

	//发送执行目录遍历的信号
	int s_walkFile(int direction_, QString path_, bool isCompareHideDir, bool isSkipDir, bool isSkipFile, bool isSkipNamePrefix, int compareAllFiles, QString skipDirs, QString skipFileExt, QString skipNamePrefix);

public slots:
	void slot_openLeftDir();
	void slot_openRightDir();
	void slot_itemLeftDoubleClick(QTreeWidgetItem * item, int column);
	void slot_cmpResultStatusChange(int status, QString leftFilePath, QString rightFilePath);
	void slot_itemRightDoubleClick(QTreeWidgetItem * item, int column);

	void slot_syncCurScrollValue(int direction);

	void slot_syncExpandStatus(QString name, int direction, int status);
	void slot_fileCompareFinish();
	void slot_copyFileToOther(int src, QString fileName, QTreeWidgetItem* curItem, bool multiSelete = false);
	void slot_coverDiffFilesToOther(int src, QString fileName, QTreeWidgetItem* curItem);
	void slot_coverDiffFilesWalkDirToOther(int src, QString fileName, QTreeWidgetItem* curItem);
	void slot_copyUniqueFilesToOther(int src, QString fileName, QTreeWidgetItem * curItem);
	void slot_copyUniqueFilesWalkDirToOther(int src, QString fileName, QTreeWidgetItem* curItem);
	void slot_deleteOnlyFileInThisSide(int src, QString fileName, QTreeWidgetItem * curItem);
	void slot_findFile(int src = 0);
	void slot_deleteFile(int src, QString fileName, QTreeWidgetItem * curItem, bool multiSelete = false);
	void slot_FindFileWithPara(int dire, int prevOrNext, QString fileName, bool caseSenstive, bool wholeWord);
	void slot_itemLeftClick(QTreeWidgetItem * item, int column);
	void slot_itemRightClick(QTreeWidgetItem * item, int column);

	void slot_swapBt(bool);
	void slot_reloadBt();
	void slot_rultBt(bool);
	void slot_clearBt(bool);
	void slot_showAllBt(bool);
	void slot_showDiffBt(bool);
	void slot_showOnlyDiffBt(bool);
	void slot_showUniqueBt(bool);
	void slot_reloadLeftDir();
	void slot_reloadRightDir();

private slots:
	void slot_quitProgress();
	void slot_leftPathReturnPressed();
	void slot_rightPathReturnPressed();
	void slot_delayWork(int dir, QString path);
	void slot_MarkAsEqual(int dir, QString fileName, QTreeWidgetItem * curItem);
	void slot_onLeftSort(int logicalIndex, Qt::SortOrder order);
	void slot_onRightSort(int logicalIndex, Qt::SortOrder order);
	void slot_syncOrder(int dire, Qt::SortOrder);

	void slot_expandBt(bool);
	void slot_foldBt(bool);

	//显示子线程中的消息。
	void on_childThreadMsg(int type, QString msg, qint64 value);
	void on_addNode(CmpWalkFileInfo* nodeInfo);

signals:
	void signCmpDirClose();
	void receneDirPath(QString left, QString right);
	void synvOrder(int dire, Qt::SortOrder);

protected:

	void cmpFileTree();

	int loadDir(int type, QString rootDirPath);

	QString getRelativePathFromDirList(QStringList & dirList, int endPos);

	QString getRelativePath(int dire, QString filePath);

	QTreeWidgetItem * findItemParentByDirName(QString dirPath, int direction);

	QTreeWidgetItem* findItemByRelativePath(int direction, QString relativePath);

	void createDirOnFileTree(QString dirPath, int direction);

	void displayCmpFileList();

	QString filePathToRelativePath(QString filePath, int direction);

	void QFileInfoToAttriNode(QFileInfo & fileInfo, fileAttriNode & fileAttri, QTreeWidgetItem* parentItem, int direction);

	void alignPathToAttriNode(fileAttriNode & fileAttri, QTreeWidgetItem * parentItem, int direction);

	void appendAttriNode(fileAttriNode &fileAttri, int direction);

	QList<fileAttriNode>& getFileAttriNodeList(int direction);

	void createFileAttriMapFromList();

	QMap<QString, fileAttriNode>* getFileAttriNodeMap(int direction);

	int walkLoadFile(QString path, int direction);

	//void setItemIntervalBackground();

	static void setItemBackground(QTreeWidgetItem* item, const QColor& color);

	static void setItemForeground(QTreeWidgetItem* item, const QColor& color);

	static void setItemEqualStatus(QTreeWidgetItem * item, ITEM_FILE_STATUS status);


	void setAllParentDirForegroundNoEqual(QTreeWidgetItem* item, const QColor& color);

	void dragEnterEvent(QDragEnterEvent* event) override;

	void dropEvent(QDropEvent* e) override;

	void closeEvent(QCloseEvent* e) override;
	void setWorkStatusInfo(QString info);
	void cmpSelectFile();

	void createWorkThread();
private:
	Ui::CompareDirs ui;

	//左右进行补齐的文件列表，方便对比
	QList<fileAttriNode> m_leftFileAttris;
	QList<fileAttriNode> m_rightFileAttris;

	//用一个map加快上面List的索引查找
	QMap<QString,fileAttriNode> m_leftFileAttrisMap;
	QMap<QString,fileAttriNode> m_rightFileAttrisMap;

	//未完成的对比任务列表，防止有大文件时，可以直接显示出来
	//编号、相对短路径
	QMap<int, QString> m_unFinishedCmpFileMap;
	MediatorFileTree * m_mediator;

	int m_curItemIndex;

	CompareDirMode * m_dataMode;

	std::function<void(int dire, QMenu*, QTreeWidgetItem*)> m_menuCallbackFun;

	int m_currentLeftFindIndex;
	int m_currentRightFindIndex;

	DirFindFile* m_findFileWin;

	QString m_leftFindText;
	QString m_rightFindText;

	RC_DIRECTION m_curFindDire;
	bool m_findCaseSenstive;
	bool m_findWholeWord;

	QList<QTreeWidgetItem *> m_leftFindResult;
	QList<QTreeWidgetItem *> m_rightFindResult;

	int m_leftFileNums;
	int m_rightFileNums;

	int m_leftRightOrder;
	int m_commitCmpFileNums;
	int m_finishCmpFileNums;

	ProgressWin* m_loadFileProcessWin;

	bool m_isFirstDirCmp;
	bool m_isCompareCancel;
	//做之前是否询问
	bool m_doWithAsk;
	bool m_loadDirCancel;
	bool m_loadDirFinished;

	int m_workStatus;

	//显示工作状态
	QLabel* m_sbarWorkStatusLable;

	QString m_selectLeftCmpFile;
	QString m_selectRightCmpFile;

	QThread* m_workThread;
	CompareDirsData* m_cmpDataThread;

	QTreeWidgetItem* m_curRootDirNode; //当前处理的目录根节点。
	QList<QTreeWidgetItem*> m_dirNodeList; //记录所有目录的根节点，每次都是去头部第一个做当前根节点

public:

	//对应DirCmpExtWin中的参数
	static bool s_compareHideDir;
	static bool s_compareChildDir;
	static int s_compareAllFiles;
	static int s_compareMode;
	static int s_isSkipDir;
	static QString s_skipDirs;
	static int s_isSkipFile;
	static QString s_skipFileExt;
	static int s_isSkipNamePrefix;
	static QString s_skipNamePrefix;
};
