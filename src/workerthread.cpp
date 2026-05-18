#include "workerthread.h"


WorkerThread::WorkerThread(QObject *parent)
	: QObject(parent)
{
}

WorkerThread::~WorkerThread()
{
}

/* 这个函数是运行在多线程里面 */
void WorkerThread::doWork(std::function<int (void *)> fun, void *parameter)
{
	//从外面传递一个函数进来，执行
	int ret = fun(parameter);

	emit resultReady(ret);
}