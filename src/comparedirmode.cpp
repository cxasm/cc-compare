#include "comparedirmode.h"
#include "workthreadctl.h"
#include "comparedirmode.h"
#include "BlockCompare.h"
#include "comparewin.h"

#include <QFile>
#include <QCryptographicHash>
#include <QtConcurrent>


//bool CompareDirMode::s_isCompareCancel = false;

//最大对比1M的文件。超过1M优先对比时间
const int MAX_CMP_FILE_SIZE = 1024 * 1024 * 1;

CompareDirMode::CompareDirMode(QObject *parent): QObject(parent), s_isCompareCancel(false)
{
	threadCtl = new WorkThreadCtl;
}

QFuture<ThreadParameter*> CompareDirMode::commitTask(std::function<ThreadParameter* (ThreadParameter*)> fun, ThreadParameter * parameter)
{
	/* 这里最开始准备使用信号提交多线程，但是发现std:;function无法使用槽函数机制，需要自己是实现元对象 
	* 直接使用QtConcurrent::run机制，不仅简单许多，而且在网上看了资料
	*/
	//threadCtl->commitTask(fun, parameter);
	return QtConcurrent::run(fun, parameter);
}

CompareDirMode::~CompareDirMode()
{
	if (threadCtl != nullptr)
	{
		delete threadCtl;
		threadCtl = nullptr;
	}
}

bool CompareDirMode::quickCmpFile(QString& leftPath, QString& rightPath)
{
	bool ret = true;

	QFile leftFile(leftPath);
	QFile rightFile(rightPath);

	bool ok1 = leftFile.open(QIODevice::ReadOnly);
	bool ok2 = rightFile.open(QIODevice::ReadOnly);

	if (!ok1 || !ok2)
	{
		return false;
	}

	if (leftFile.size() != rightFile.size())
	{
		ret = false;
	}
	else if (leftFile.size() > 5*MAX_CMP_FILE_SIZE) //5M以上文件，只比较时间。
	{
		ret = false;

		//文件太大会影响多文件的对比效率。不进行详细对比
		QDateTime leftChangeDate = leftFile.fileTime(QFileDevice::FileMetadataChangeTime);
		QDateTime rightChangeDate = rightFile.fileTime(QFileDevice::FileMetadataChangeTime);

		if (leftChangeDate == rightChangeDate)
		{
			////如果修改时间相等,而且创建时间相等，则认定相等。20230509 暂时不对比创建时间。
			//QDateTime leftBirthDate = leftFile.fileTime(QFileDevice::FileBirthTime);
			//QDateTime rightBirthDate = rightFile.fileTime(QFileDevice::FileBirthTime);

			//if (leftBirthDate == rightBirthDate)
			//{
				ret = true;
			/*}*/
			}
		}
	else
	{
		//大小相等，比较md4值。实验发现md4比md5快6倍。5M以下文件才比较MD5
		QCryptographicHash leftHash(QCryptographicHash::Md4);
		QCryptographicHash rightHash(QCryptographicHash::Md4);

		leftHash.addData(&leftFile);
		rightHash.addData(&rightFile);

		if (leftHash.result() != rightHash.result())
		{
			ret = false;
		}

	}

	leftFile.close();
	rightFile.close();

	return ret;
}

//慢速模式，则要走文件的lcs对比
bool CompareDirMode::deepCmpFile(QString& leftPath, QString& rightPath)
{
	if (s_isCompareCancel)
	{
		return false;
	}

	bool ret = true;

	CmpareMode * pCmpMode = new CmpareMode(leftPath, rightPath);
	pCmpMode->setCmpMode(0);
	BlockCompare* pBlockCompare = new BlockCompare();
	pBlockCompare->m_isCancel = &s_isCompareCancel;

	ret = pCmpMode->isFileTextEqual(pBlockCompare);
	delete pBlockCompare;
	delete pCmpMode;

	return ret;
}

//对比左右文件的大小，sha1值来判断文件是否相等
//mode 比较模式  0 慢速模式 1 快速模式
QFuture<ThreadParameter*> CompareDirMode::compareFile(QString leftPath, QString rightPath, int mode, int fileIndex)
{
	ThreadParameter *p = new ThreadParameter(leftPath, rightPath);
	p->fileIndex = fileIndex;

	//int 0相等 1 不等。注意下面函数体是在多线程中执行的。
	return commitTask([this,mode](ThreadParameter *parameter)->ThreadParameter *
	{
		if (s_isCompareCancel)
		{
			parameter->isEqual = false;
			return parameter;
		}

		bool ret = true;
		int exeMode = mode;

		ThreadParameter *para = (ThreadParameter *)parameter;

		QFileInfo lfi(para->leftPath);
		QFileInfo rfi(para->rightPath);


		if ((lfi.size() == 0)  && (rfi.size() == 0))
		{
			//都是0长，直接相等。
			ret = true;
			exeMode = 2;
		}
		//如果文件大小差别过大，明显是不同，直接认定为不等。
		else if (qAbs(lfi.size() - rfi.size()) > 100)
		{
			ret = false;
			exeMode = 2;
		}
		else if (lfi.size() == rfi.size())
		{
			//文件大小相等，而且修改时间也相等，也认定为相等拉倒。
			QDateTime leftChangeDate = lfi.fileTime(QFileDevice::FileMetadataChangeTime);
			QDateTime rightChangeDate = rfi.fileTime(QFileDevice::FileMetadataChangeTime);
			if (leftChangeDate == rightChangeDate)
			{
				ret = true;
				exeMode = 2;
			}
		}

		//没有得到结果，继续比较
		if (exeMode != 2)
		{
			if (lfi.size() > MAX_CMP_FILE_SIZE || rfi.size() > MAX_CMP_FILE_SIZE) //大文件一律走快速模式
			{
			exeMode = 1;
		}
		else if (!CompareWin::isTextModeFile(para->leftPath) || !CompareWin::isTextModeFile(para->rightPath))
		{
			//非文本模式一律走快速模式
			exeMode = 1;
		}
			else
		{

				if (lfi.size() == rfi.size())
				{
					//既然要对比md5值，肯定是文件一样大。否则如果文件都不一样大，md5肯定不相等。
				//既是文本，又是小文件。对比md5值
				QFile leftFile(para->leftPath);
				QFile rightFile(para->rightPath);

				bool ok1 =leftFile.open(QIODevice::ReadOnly);
				bool ok2 = rightFile.open(QIODevice::ReadOnly);

				//打开出错，认定不等
				if (!ok1 || !ok2)
				{
					ret = false;
					exeMode = 2;
				}

				//大小相等，比较md4值。实验发现md4比md5快6倍。5M以下文件才比较MD5
				QCryptographicHash leftHash(QCryptographicHash::Md4);
				QCryptographicHash rightHash(QCryptographicHash::Md4);

				leftHash.addData(&leftFile);
				rightHash.addData(&rightFile);

				//md5值相同，认定为相等拉倒。
				if (leftHash.result() == rightHash.result())
				{
					ret = true;
					exeMode = 2;
				}
					leftFile.close();
					rightFile.close();
				}
				else if (exeMode == 1)
				{
					//如果文件大小不等，而且是走快速模式，直接认定为不等。
					//如果是慢速，则还需要深入到文本中，继续对比是否相等
			ret = false;
					exeMode = 2;
		}
			}
		}


		//默认快速模式1
		if (exeMode == 0)
		{
			ret = deepCmpFile(para->leftPath, para->rightPath);
		}
		else if (exeMode == 1)
		{
			ret = quickCmpFile(para->leftPath, para->rightPath);
		}

		parameter->isEqual = ret;
		return parameter;
	}
	,p);

}

void CompareDirMode::setCompareCancel(bool b)
{
	s_isCompareCancel = b;
}
