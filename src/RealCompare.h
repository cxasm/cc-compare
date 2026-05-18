#pragma once

#include <QtWidgets/QMainWindow>
#include <QLabel>
#include "ui_RealCompare.h"

class CompareDirs;
class CompareWin;
class chatSocket;
class DirFindFile;

#ifdef Q_OS_UNIX
const int VERIFY_CODE = 6; //验证注册.windows 5 linux 6
#else
const int VERIFY_CODE = 5; //验证注册.windows 5 linux 6
#endif
static const short version_num = 25;//1.18 是22 1.19是23 1.20是24

class RealCompare : public QMainWindow
{
	Q_OBJECT

public:
	RealCompare(QWidget *parent = Q_NULLPTR);
	virtual ~RealCompare();

	static QString getLocalFirstMac();
	static QStringList getLocalMac();
	static bool isTadayAlreadySgin();
	void compareFile(QString leftPath, QString rightPath);
	void compareDir(QString leftDir, QString rightDir);

#ifdef Q_OS_WIN
	void setLeftCmpFile(QString file);
	void setRightCmpFile(QString file);
	void setLeftCmpDir(QString path);
	void setRightCmpDir(QString path);
#endif

protected:
	void closeEvent(QCloseEvent* e) override;
	void winTopShow();

#ifdef Q_OS_WIN
	bool nativeEvent(const QByteArray & eventType, void * message, long * result);
#endif

public slots:
	void slot_cmpareFileBt();
	void slot_cmpareDirBt();
	void slot_cmpareBinBt();
	void slot_recentCmpDir(QString leftDirPath, QString rightDirPath);
	void slot_recentCmpFile(QString leftFilePath, QString rightFilePath);
	void slot_codeConvert();
	void slot_cmpDirClose();
	void slot_cmpFileClose();

	void slot_changeEnglish();
	void slot_changeChinese();


	void slot_donate();



	void slot1_mainweb();

	void slot_newVersion(const QString & link);

	void on_itemDoubleClicked(QTreeWidgetItem* item, int column);

	void on_itemClicked(QTreeWidgetItem* item, int /*column*/);

	void on_findFile(bool);

	void on_findFileWithPara(int dire, int prevOrNext, QString fileName, bool caseSenstive, bool wholeWord);

private:
	void showNewVersionLabel();

	void openRecentDir(QString text);
	void openRecentFile(QString text);

private:
	Ui::RealCompareClass ui;
	void initMainTab();
	void saveDefaultFontSize();
	void initReceneCmp();

	void saveReceneCmp();
	static void initDefultFontSize();
	void updateCmpTimes();
	void updateServerVersion();
	

	QList<CompareDirs*> m_cmpDirMgr;
	QList<CompareWin*> m_cmpFileMgr;

	QTranslator* m_translator;


	//最近打开的对比文件和目录列表。做一个环形区
	//保存在数据库中
	int m_receneDirStartPos;
	int m_receneFileStartPos;

	//QList<QAction*> m_receneDirList;
	//QList<QAction*> m_receneFileList;

	QStringList m_receneDirList;
	QStringList m_receneFileList;

	QMap<QString, int> m_receneRecrod;

	QLabel* m_codeStatusLabel;

	//服务器上最新的版本号码，即当前发布的最新版本。本地版本小于该值，则显示需要更新
	int m_serverNewVersion;

#ifdef Q_OS_WIN
	QString m_leftCmpFile;
	QString m_rightCmpFile;
	QString m_leftCmpDir;
	QString m_rightCmpDir;
#endif

	QTreeWidgetItem* m_pTopFileItem;
	QTreeWidgetItem* m_pTopDirItem;

	QMenu* m_menu;

	DirFindFile* m_findFileWin;
	QString m_findText;
	QList<QTreeWidgetItem*> m_findResult;
	int m_currentFindIndex;

	bool m_findCaseSenstive;
	bool m_findWholeWord;

};
