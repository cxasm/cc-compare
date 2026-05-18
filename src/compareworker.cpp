#include "compareworker.h"
#include <QTimerEvent>
#include <Scintilla.h>

CompareWorker::CompareWorker(QObject*parent): QObject(parent)
{
}

CompareWorker::~CompareWorker()
{
}


//接受来自修改的消息。该函数会在新的工作线程中执行
void CompareWorker::doCompareWork(int direct, ModifyRecords* record)
{
	if (0 == direct)
	{
		//如果没有发生行的加删，而且是纯增加内容，则直接执行，不延时
		//修改类型：
		//1）纯增加 2）纯减少 3）覆盖即先减少在增加
		//可以归并为凡是遇到增加，则直接执行。减少的缓一缓。因为减少可能是替换，先减少后续马上要增加
		if (record->modificationType == SC_MOD_INSERTTEXT)
		{
			emit startCompare(0);
			return;
		}
		//如果是删除，但是在拷贝中，则还要等待，不发送
		if ((record->modificationType == SC_MOD_DELETETEXT) && (record->isInPaste))
		{
			return;
		}
		else if (record->modificationType == SC_MOD_DELETETEXT)
		{
			//如果是删除，纯删除，不在拷贝中，则直接执行
			emit startCompare(0);
		}

	}
	else
	{
		if (record->modificationType == SC_MOD_INSERTTEXT)
		{

			emit startCompare(1);
			return;
		}
		//如果是删除，但是在拷贝中，则还要等待，不发送
		if ((record->modificationType == SC_MOD_DELETETEXT) && (record->isInPaste))
		{
			return;
		}
		else if (record->modificationType == SC_MOD_DELETETEXT)
		{
			//如果是删除，纯删除，不在拷贝中，则直接执行
			emit startCompare(1);
		}
	}
}
