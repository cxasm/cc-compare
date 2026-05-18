#include "workthreadctl.h"
#include "workerthread.h"

WorkThreadCtl::WorkThreadCtl(QObject *parent): QObject(parent)
{
	WorkerThread *worker = new WorkerThread;
	worker->moveToThread(&workerThread);

	connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);

	connect(this, &WorkThreadCtl::operate, worker, &WorkerThread::doWork);
	
	//connect(worker, &WorkerThread::resultReady, this, &WorkThreadCtl::handleResults);

	workerThread.start();
}

WorkThreadCtl::~WorkThreadCtl()
{
	workerThread.quit();
	workerThread.wait();
}


//提交信号，执行槽函数在多线程中
void WorkThreadCtl::commitTask(std::function<int(void*)> fun, void * parameter)
{
	operate(fun, parameter);
}
