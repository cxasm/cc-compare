#pragma once

#include <QObject>


enum cmpDirCmdType
{
	PROGRESS_INFO = 1,//输出进度条信息
	SET_PROGRESS_TOTAL_STEPS,//设置进度条总值
	PROGRESS_STEP,//进度条前进一格
	ADD_DIR_NODE,//增加目录节点
	ADD_FILE_NODE,//增加文件节点
	CHANGE_CURRENT_DIR_NODE,//切换当前操作的节点
	DIR_MAX_FILE_SIZE,//文件夹下面最大的文件大小，后续要使用这个排序文件夹。
	DIR_FILE_LOAD_FINISHED,//文件夹全部加载完毕。加载后是在主线程中执行节点创建，要等最后一个槽函数执行完毕，才能算全部节点加载完毕。
};

struct CmpWalkFileInfo;

class CompareDirsData  : public QObject
{
	Q_OBJECT

public:
	CompareDirsData(QObject *parent);
	~CompareDirsData();

public:
	void setCancel(bool value);
	bool isDone();
	void setIsDone(bool value);
	int getFileNums();

signals:
	//输出进度条消息。
	void outMsg(int type, QString msg, qint64 value=0);

	//增加节点。
	void addNode(CmpWalkFileInfo* nodeInfo);


public slots:

	//在槽函数中，子线程中执行该槽函数。
	void on_walkFile(int direction_, QString path_, bool isCompareHideDir, bool isSkipDir, bool isSkipFile, bool isSkipNamePrefix, int compareAllFiles, \
		QString skipDirs, QString skipFileExt, QString skipNamePrefix);


private:
	volatile bool m_isCancel;
	volatile bool m_isDone;
	volatile int m_fileNums;
};
