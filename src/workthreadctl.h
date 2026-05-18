#pragma once

#include <QObject>
#include <QThread>

class WorkThreadCtl : public QObject
{
	Q_OBJECT

public:
	WorkThreadCtl(QObject *parent=nullptr);
	~WorkThreadCtl();

	void commitTask(std::function<int(void*)> fun, void *);

signals:
	void operate(std::function<int(void*)> fun, void *);
private:
	QThread workerThread;
};
