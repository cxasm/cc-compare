#pragma once

#include <QWidget>
#include <QFuture>
#include <functional>
#include <QMap>

#include "ui_encodeconvert.h"
#include "rcglobal.h"


typedef struct EncodeThreadParameter_ {
	QString filepath;
	CODE_ID code;
	int error;// -1 文件过大。
	QTreeWidgetItem* item;

	EncodeThreadParameter_(QString filePath_):error(0)
	{
		filepath = filePath_;
		code = CODE_ID::UNKOWN;
	}

}EncodeThreadParameter;

class EncodeConvert : public QWidget
{
	Q_OBJECT

public:
	EncodeConvert(QWidget *parent = Q_NULLPTR);
	~EncodeConvert();

private:
	int allfile(QTreeWidgetItem* root_, QString path_);
	int loadDir(QString rootDirPath);
	bool isSupportExt(int index, QString ext);

	QFuture<EncodeThreadParameter*> commitTask(std::function<EncodeThreadParameter* (EncodeThreadParameter*)> fun, EncodeThreadParameter* parameter);
	QFuture<EncodeThreadParameter_*> checkFileCode(QString filePath, QTreeWidgetItem* item);
	static CODE_ID convertFileToCode(QString& filePath, CODE_ID srcCode, CODE_ID dstDode, int& error);
	static CODE_ID getComboBoxCode(int index);
	QFuture<EncodeThreadParameter_*> convertFileCode(QString filePath, QTreeWidgetItem* item);
	void scanFileCode();

protected:
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* e) override;

public slots:
	void slot_scanFileCode();
	void slot_userDefineExt();
	void on_changeExt(int index);
	void on_showOnlyConvertibleItem(int state);
	void on_delSelLine();

private slots:
	void slot_selectFile();
	void slot_startConvert();

	void slot_convertFileFinish();
	void slot_itemClicked(QTreeWidgetItem* item, int column);

private:
	//void setItemIntervalBackground();
	void setItemBackground(QTreeWidgetItem* item, const QColor& color);

private:
	Ui::EncodeConvert ui;

	QList<fileAttriNode> m_fileAttris;
	QString m_fileDirPath;

	int m_commitCmpFileNums;
	int m_finishCmpFileNums;
	int m_transFailNums;
	int m_transFailLargeNums;
	int m_extComBoxNum;

	int m_workStatus;

	QList< QMap<QString, bool>* > m_supportFileExt;

	QMenu* m_menu;
};
