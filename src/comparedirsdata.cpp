#include "comparedirsdata.h"
#include "rcglobal.h"
#include "doctypelistview.h"

#include <QFileInfo>
#include <QDir>
#include <QDateTime>


CompareDirsData::CompareDirsData(QObject *parent)
	: QObject(parent), m_isCancel(false), m_isDone(false)
{
}

CompareDirsData::~CompareDirsData()
{
}

//这个接口，得在外面主线程中访问。严格来说，主线程中调用了子线程中的函数。有一定的风险。
void CompareDirsData::setCancel(bool value)
{
	m_isCancel = value;
}

void CompareDirsData::setIsDone(bool value)
{
	m_isDone = value;
}

//这个接口，得在外面主线程中访问。严格来说，主线程中调用了子线程中的函数。有一定的风险。
bool CompareDirsData::isDone()
{
	return m_isDone;
}

int CompareDirsData::getFileNums()
{
	return m_fileNums;
}

//非递归版本。遍历目录，该函数是在子线程中执行，注意不能操作UI类
void CompareDirsData::on_walkFile(int direction, QString path_, bool isCompareHideDir, bool isSkipDir, bool isSkipFile, bool isSkipNamePrefix, int compareAllFiles, QString skipDirs, QString skipFileExt, QString skipNamePrefix)
{
	QList<QString> dirsList;
	//WalkFileInfo oneDir(direction, path_);
	dirsList.append(path_);

	int fileNums = 0;

	//再获取文件夹到列表
	QDir::Filters dirfilter;

	if (isCompareHideDir)
	{
		dirfilter = QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::NoSymLinks;
	}
	else
	{
		dirfilter = QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks;
	}

	//过滤文件
	auto fileFilter = [](QFileInfo& fileInfo)->bool {
		QString suffix = fileInfo.suffix();
		if (!suffix.isEmpty())
		{
			return DocTypeListView::isSupportExt(suffix);
		}
		return true;
	};

	QStringList skipDirList = skipDirs.split(":");

	auto dirFilter = [&skipDirList](QString& dirName)->bool {

		//如果在过滤列表中，则过滤返回false;
		if (-1 != skipDirList.indexOf(dirName))
		{
			return false;
		}
		return true;
	};

	QStringList skipFileExtList = skipFileExt.split(":");

	auto fileExtFilter = [&skipFileExtList](QString fileExt)->bool {

		//如果在过滤列表中，则过滤返回false;
		if (-1 != skipFileExtList.indexOf(QString(".%1").arg(fileExt)))
		{
			return false;
		}
		return true;
	};

	QStringList skipFileNamePrefix = skipNamePrefix.split(":");

	auto fileNamePrefixFilter = [&skipFileNamePrefix](QString name)->bool {

		//如果在过滤列表中，则过滤返回false;
		for (int i = 0; i < skipFileNamePrefix.size(); ++i)
		{
			if (name.startsWith(skipFileNamePrefix.at(i)))
			{
				return false;
			}
		}
		return true;
	};


	int dirNums = 0;

	bool firstChildDirs = true;
	int totalStep = 0;
	bool isFirstDir = true;

	while (!dirsList.isEmpty())
	{

		QString path = dirsList.takeFirst();

		//切换当前的根节点。第一个不用切换，因为第一个在外面已经挂载在目录的根节点树上了。
		//后面每取一个子目录节点，则外面要同步更新一个目录节点。
		if (!isFirstDir)
		{
			CmpWalkFileInfo* dirNode = new CmpWalkFileInfo(direction, "", CHANGE_CURRENT_DIR_NODE);
			emit addNode(dirNode);
		}
		else
		{
			isFirstDir = false;
		}


		/*添加path路径文件*/
		QDir dir(path);          //遍历各级子目录

		QFileInfoList folder_list = dir.entryInfoList(dirfilter);   //获取当前所有目录

		for (int i = 0; i != folder_list.size(); ++i)         //自动递归添加各目录到上一级目录
		{
			QString namepath = folder_list.at(i).absoluteFilePath();    //获取路径
			QFileInfo folderinfo = folder_list.at(i);

			if (!isCompareHideDir && folderinfo.baseName().isEmpty())
			{
				emit outMsg(PROGRESS_INFO, tr("skip dir %1").arg(namepath));
				continue;
			}

			QString name = folderinfo.fileName();      //获取目录名

			if (isSkipDir && !dirFilter(name))
			{
				emit outMsg(PROGRESS_INFO, tr("skip dir %1").arg(namepath));
				continue;
			}

			//增加dir
			//emit outMsg(2,name,)

			//QObject* childroot = new QObject(root);
			//childroot->setObjectName(name);
			//childroot->setIcon(0, QIcon(":/Resources/img/dir.png"));
			//root->addChild(childroot);              //将当前目录添加成path的子项

			//fileAttriNode node;
			//node.type = RC_DIR;//是目录
			//node.selfItem = childroot;
			//QFileInfoToAttriNode(folderinfo, node, root, direction);

			//把路径名称保存到tips中，后续需要这个来排序，下同
			//childroot->setData(0, Item_RelativePath, node.relativePath);
			//appendAttriNode(node, direction);
			CmpWalkFileInfo* dirNode = new CmpWalkFileInfo(direction,name, RC_DIR);
			dirNode->fileinfo = folderinfo;
			emit addNode(dirNode);

			//WalkFileInfo oneDir(direction, namepath);
			//注意这里是添加在头部，外面也要保存同样的顺序
			dirsList.push_front(namepath);

			dirNums++;

			//m_loadFileProcessWin->info(tr("load %1 dir %2").arg(dirNums).arg(namepath));
			emit outMsg(PROGRESS_INFO, tr("load %1 dir %2").arg(dirNums).arg(namepath));

			//文件太少，会导致提前出去，外部可能先执行导致崩溃
			//if (folder_list.size() > 10)
			//{
			//	QCoreApplication::processEvents(/*QEventLoop::ExcludeUserInputEvents*/);
			//}
		}

		if (firstChildDirs)
		{
			totalStep = dirNums;
			//m_loadFileProcessWin->setTotalSteps(dirNums);
			firstChildDirs = false;
			emit outMsg(SET_PROGRESS_TOTAL_STEPS, "", dirNums);
		}

		if (dirsList.size() < totalStep)
		{
			totalStep = dirsList.size();
			emit outMsg(PROGRESS_STEP, "");
		}


		qint64 maxFileSize = 0;

		QDir dir_file(path);
		dir_file.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);//获取当前所有文件
		QFileInfoList list_file = dir_file.entryInfoList();
		for (int i = 0; i < list_file.size(); ++i)
		{  //将当前目录中所有文件添加到treewidget中
			QFileInfo fileInfo = list_file.at(i);

			//不支持所有文件，仅仅支持支持的类型
			if ((compareAllFiles == 2) && !fileFilter(fileInfo))
			{
				continue;
			}

			QString name2 = fileInfo.fileName();

			if (isSkipFile && !fileExtFilter(fileInfo.suffix()))
			{
				emit outMsg(PROGRESS_INFO, tr("skip file ext %1").arg(name2));
				continue;
			}

			if (isSkipNamePrefix && !fileNamePrefixFilter(name2))
			{
				emit outMsg(PROGRESS_INFO, tr("skip file prefix %1").arg(name2));
				continue;
			}

			++fileNums;

			CmpWalkFileInfo* fileNode = new CmpWalkFileInfo(direction, name2, RC_FILE);
			fileNode->fileSize = fileInfo.size();
			fileNode->modifyTime = fileInfo.lastModified().toString("yy/MM/dd hh:mm:ss");
			fileNode->fileinfo = fileInfo;

			emit addNode(fileNode);
			//QObject* child = new QObject(root);
			//child->setObjectName(name2);

			//qint64 fileSize = fileInfo.size();
			//child->setText(1, getFileSizeFormat(fileSize));
			//QString lastModifyTime = fileInfo.lastModified().toString("yy/MM/dd hh:mm:ss");
			//修改时间。上次修改的实际。
			//child->setText(2, lastModifyTime);
			//root->addChild(child);

			//记录当前目录下的最大值
			if (maxFileSize < fileNode->fileSize)
			{
				maxFileSize = fileNode->fileSize;
			}

			//fileAttriNode node;
			//node.type = RC_FILE;//是文件
			//node.selfItem = child;
			//QFileInfoToAttriNode(fileInfo, node, root, direction);

			//child->setData(0, Item_RelativePath, node.relativePath);
			//appendAttriNode(node, direction);

			if (m_isCancel)
			{
				emit outMsg(PROGRESS_INFO, tr("compare canceled ..."));
				break;
			}
		}

		//记录最大文件到目录的DIR_ITEM_MAXSIZE_FILE中，后续排序时，文件夹大小按照此来排序
		//root->setData(0, DIR_ITEM_MAXSIZE_FILE, maxFileSize);
		//emit outMsg(DIR_MAX_FILE_SIZE, "", maxFileSize);

		//文件的最大值，也发送出去。
		CmpWalkFileInfo* dirNode = new CmpWalkFileInfo(direction, "", DIR_MAX_FILE_SIZE);
		dirNode->fileSize = maxFileSize;
		emit addNode(dirNode);

		//fileNums += list_file.size();

		if (m_isCancel)
		{
			break;
		}
	}

	m_isDone = true;

	m_fileNums = fileNums;

	//全部文件加载完毕。要等到这个槽函数执行后，主界面中才能算所有文件节点创建完毕。
	CmpWalkFileInfo* dirNode = new CmpWalkFileInfo(direction, "", DIR_FILE_LOAD_FINISHED);
	emit addNode(dirNode);

}
