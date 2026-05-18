#pragma once

#include <QObject>
#include <functional>

class WorkerThread : public QObject
{
	Q_OBJECT

public:
	WorkerThread(QObject *parent=nullptr);
	~WorkerThread();
	
signals:
	void resultReady(int);

public slots:
	void doWork(std::function<int (void*)> fun, void * parameter);
};
