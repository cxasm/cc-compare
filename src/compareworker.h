#pragma once

#include "rcglobal.h"
#include <QObject>

class CompareWorker : public QObject
{
	Q_OBJECT

public:
	CompareWorker(QObject*parent = Q_NULLPTR);
	virtual ~CompareWorker();

signals:
	void startCompare(int direct);

public slots:
	void doCompareWork(int direct, ModifyRecords* record);
};
