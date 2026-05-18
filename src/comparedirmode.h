#pragma once

#include <QObject>
#include <QFuture>

#include "workthreadctl.h"

typedef struct ThreadParameter_ {
	QString leftPath;
	QString rightPath;
	bool isEqual;
	int fileIndex;

	ThreadParameter_(QString leftPath_, QString rightPath_)
	{
		leftPath = leftPath_;
		rightPath = rightPath_;
		isEqual = false;
		fileIndex = 0;
	}

}ThreadParameter;

class CompareDirMode : public QObject
{
	Q_OBJECT

public:
	CompareDirMode(QObject *parent=nullptr);
	~CompareDirMode();
	QFuture<ThreadParameter*> compareFile(QString leftPath, QString rightPath,int mode=0, int fileIndex=0);
	void setCompareCancel(bool b);

protected:
	QFuture<ThreadParameter*> commitTask(std::function<ThreadParameter *(ThreadParameter*)> fun, ThreadParameter * parameter);
	bool quickCmpFile(QString & leftPath, QString & rightPath);
    bool deepCmpFile(QString& leftPath, QString& rightPath);
private:
	WorkThreadCtl *threadCtl;
	volatile bool s_isCompareCancel;
};
