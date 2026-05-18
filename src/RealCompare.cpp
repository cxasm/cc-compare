#include "RealCompare.h"
#include "comparewin.h"
#include "CompareDirs.h"
#include "Lcs.h"
#include "cmpsql.h"
#include "encodeconvert.h"
#include "donate.h"
#include "comparehexwin.h"
#include "doctypelistview.h"
#include "rcglobal.h"

#include <QMessageBox>
#include <QTranslator>
#include <QNetworkInterface>
#include <QDate>
#include <QDir>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QWindow>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <Windows.h>
#endif

#include <dirfindfile.h>

//5 1.3
//6 1.4 20211027日

#ifdef Q_OS_UNIX
#include <QFontDatabase>

QString loadFontFromFile(QString path,int code)
{
    QString font;
    static QMap<int,QString> codelist;

    if(codelist.contains(code))
    {
        return codelist.value(code);
    }

    int loadedFontID = QFontDatabase::addApplicationFont(path);
    QStringList loadedFontFamilies = QFontDatabase::applicationFontFamilies(loadedFontID);
    if(!loadedFontFamilies.empty())
    {
        if ((code == 0) && loadedFontFamilies.size() >=7)
        {
            font = loadedFontFamilies.at(6);
        }
        else
        {
            font = loadedFontFamilies.at(0);
        }
    }

    codelist.insert(code,font);

    return font;
}
#endif

//为了避免路径中\\不一样导致的查找不到问题，进行统一替换
QString getRegularFilePath(QString& path)
{
#ifdef _WIN32
	path = path.replace("/", "\\");
#else
	path = path.replace("\\", "/");
#endif

	return path;
}

RC_LINE_FORM getLineEndType(QString line)
{
    if (line.endsWith("\r\n"))
    {
        return DOS_LINE;
    }
    else if (line.endsWith("\n"))
    {
        return UNIX_LINE;
    }
    else if (line.endsWith("\r"))
    {
        return MAC_LINE;
    }

    return UNKNOWN_LINE;
}
 RC_LINE_FORM getLineEndType(const LineNode& lines)
{
    if (lines.lineText.isEmpty())
    {
        return UNKNOWN_LINE;
    }

    return getLineEndType((lines.lineText.last().text));
}

RealCompare::RealCompare(QWidget *parent): QMainWindow(parent), m_codeStatusLabel(nullptr)
{
	ui.setupUi(this);

#ifdef Q_OS_UNIX
    QString fontName = loadFontFromFile("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",0);
    //QString fontName = loadFontFromFile("/usr/share/fonts/fonts-cesi/CESI_FS_GB2312.TTF");
    QFont font(fontName,9);
    //font.setPixelSize(12);
    QApplication::setFont(font);
#endif


	//初始化数据库
	CmpSql::init();

	//这里在主线程里面调用一下，避免后续因为没有创建，而可能在子线程中初始化里面的值，而且多个子线程引发重入竞争问题
	//20220402在1.11中发现这个问题。所以加上这里的手动调用
	DocTypeListView::initSupportFileTypes();

	QDir::setCurrent(QCoreApplication::applicationDirPath());

	m_translator = new QTranslator(this);

	QLocale local;
	if (local.language() == QLocale::Chinese)
	{
		slot_changeChinese();
	}


	initReceneCmp();

	initDefultFontSize();


	m_serverNewVersion = CmpSql::getKeyValueFromNumSets(QString("version"));

	if (version_num < m_serverNewVersion)
	{
		showNewVersionLabel();
	}
	m_menu = nullptr;
	m_findFileWin = nullptr;

	//设置查找的快捷键
	QAction* findAction = new QAction(this);
	QKeySequence findKey = QKeySequence(Qt::CTRL | Qt::Key_F);
	findAction->setShortcut(findKey);

	connect(findAction, &QAction::triggered, this, &RealCompare::on_findFile);
	this->addAction(findAction);

	m_findCaseSenstive = true;
	m_findWholeWord = false;
	m_currentFindIndex = 0;
}

void RealCompare::on_findFile(bool)
{
	
	if (m_findFileWin == nullptr)
	{
		m_findFileWin = new DirFindFile(0, this);
		m_findFileWin->setWindowFlag(Qt::Window);
		m_findFileWin->setMode(1);
		connect(m_findFileWin, &DirFindFile::signFindFile, this, &RealCompare::on_findFileWithPara);
	}
	m_findFileWin->getForce();
	m_findFileWin->activateWindow();
	m_findFileWin->raise();
	m_findFileWin->showNormal();

}

//接收参数来查询文件
//dir 左0 右1
//prevOrNext 前0 后1
//区分大小写
void RealCompare::on_findFileWithPara(int dire, int prevOrNext, QString fileName, bool caseSenstive, bool wholeWord)
{

	auto work = [this](int dire, QString& fileName, int prevOrNext, bool caseSenstive, bool whole) {

		QList<QTreeWidgetItem*>& FindResult = m_findResult;
		
		const QString noticeMsgPrev = tr("No Find More Prev!");
		const QString noticeMsgNext = tr("No Find More Next!");

		int& currentFindIndex = m_currentFindIndex;

		if ((m_findText != fileName) || (m_findCaseSenstive != caseSenstive) || (m_findWholeWord != whole) || FindResult.isEmpty())
		{
			m_findText = fileName;
			m_findCaseSenstive = caseSenstive;
			m_findWholeWord = whole;

			FindResult.clear();

			Qt::MatchFlags flag = Qt::MatchContains | Qt::MatchRecursive;

			if (caseSenstive)
			{
				flag |= Qt::MatchCaseSensitive;
			}

			FindResult = ui.cmpHistoryTreeWidget->findItems(fileName, flag, 0);

			
			if (FindResult.isEmpty())
			{
				QMessageBox::information(this, tr("Not Find"), tr("can not find %1").arg(fileName));
				return;
			}
			//定位到第0个
			currentFindIndex = 0;

			QTreeWidgetItem* item = FindResult.at(0);
			ui.cmpHistoryTreeWidget->setCurrentItem(item);

			ui.statusBar->showMessage(tr("current find path: %1").arg(item->text(0)));

			return;
		}

		if (prevOrNext == 0)
		{
			//往前
			currentFindIndex--;

			if ((0 <= currentFindIndex) && (currentFindIndex < FindResult.count()))
			{
				QTreeWidgetItem* item = FindResult.at(currentFindIndex);
				ui.cmpHistoryTreeWidget->setCurrentItem(item);
				ui.cmpHistoryTreeWidget->scrollToItem(item);

				ui.statusBar->showMessage(tr("current find path: %1").arg(item->text(0)));
			}
			else
			{
				currentFindIndex = -1;

				QMessageBox::information(this, tr("Not Find"), noticeMsgPrev);
				m_findFileWin->activateWindow();
			}
		}
		else if (prevOrNext == 1)
		{
			//往后
			currentFindIndex++;

			if ((0 <= currentFindIndex) && (currentFindIndex < FindResult.count()))
			{
				QTreeWidgetItem* item = FindResult.at(currentFindIndex);

				ui.cmpHistoryTreeWidget->setCurrentItem(item);

				ui.cmpHistoryTreeWidget->scrollToItem(item);
				ui.statusBar->showMessage(tr("current find path: %1").arg(item->text(0)));

			}
			else
			{
				currentFindIndex = FindResult.count();
				QMessageBox::information(this, tr("Not Find"), noticeMsgNext);
				QApplication::beep();
				m_findFileWin->activateWindow();
			}
		}
	};
	work(dire, fileName, prevOrNext, caseSenstive, wholeWord);
}


//在状态栏上显示新版本的标签
void RealCompare::showNewVersionLabel()
{
	if (m_codeStatusLabel == nullptr)
	{
		m_codeStatusLabel = new QLabel(tr("<a href = \"https://gitee.com/cxasm/cc-compare\"><u>New Version Was Detected</u></a>"), ui.statusBar);

		connect(m_codeStatusLabel, &QLabel::linkActivated, this, &RealCompare::slot_newVersion);

		QPalette pe;
		pe.setColor(QPalette::WindowText, Qt::blue);
		m_codeStatusLabel->setPalette(pe);
		m_codeStatusLabel->setMinimumWidth(120);

		//0在前面，越小越在左边
		ui.statusBar->insertPermanentWidget(0, m_codeStatusLabel);
	}
}

//从命令行输入参数直接进行对比
void RealCompare::compareFile(QString leftPath, QString rightPath)
{
	int isCmpHex = 0;

	if (!leftPath.isEmpty())
	{
		if (!QFile::exists(leftPath))
		{
			QString notice = tr("%1 not exist, please check!").arg(leftPath);
			QMessageBox::warning(this, tr("Notice"), notice);
			return;
		}
		if (leftPath.endsWith(".exe") || leftPath.endsWith(".dll") || leftPath.endsWith(".db") || leftPath.endsWith(".obj"))
		{
			isCmpHex = 1;
		}
		else if (!DocTypeListView::isSupportExt(CompareDirs::fileSuffix(leftPath)))
		{
			this->activateWindow();
			if (QMessageBox::Yes == QMessageBox::question(this, "Notice", tr("file [%1] may be not a text file, cmp in hex mode?").arg(leftPath)))
			{
				isCmpHex = 2;
			}
			else
			{
				isCmpHex = 3;
			}
		}
	}

	if (!rightPath.isEmpty())
	{
		if (!QFile::exists(rightPath))
		{
			QString notice = tr("%1 not exist, please check!").arg(rightPath);
			QMessageBox::warning(this, tr("Notice"), notice);
			return;
		}

		if (rightPath.endsWith(".exe") || rightPath.endsWith(".dll") || rightPath.endsWith(".db") || rightPath.endsWith(".obj"))
		{
			isCmpHex = 1;
		}
		else if (!DocTypeListView::isSupportExt(CompareDirs::fileSuffix(rightPath)))
		{
			if (0 == isCmpHex)
			{
				this->activateWindow();
				if (QMessageBox::Yes == QMessageBox::question(this, "Notice", tr("file [%1] may be not a text file, cmp in hex mode?").arg(rightPath)))
				{
					isCmpHex = 2;
				}
				else
				{
					isCmpHex = 3;
				}
			}
		}
	}

	if ((0 == isCmpHex) || (3 == isCmpHex))
	{
		CompareWin* newWin = new CompareWin(leftPath, rightPath, nullptr);
		newWin->setAttribute(Qt::WA_DeleteOnClose);
		newWin->showMinimized();
		newWin->showNormal();
		newWin->activateWindow();
		newWin->slot_doCmp();

		slot_recentCmpFile(leftPath, rightPath);
	}
	else
	{
		CompareHexWin* newWin = new CompareHexWin(leftPath, rightPath, nullptr);
		newWin->setAttribute(Qt::WA_DeleteOnClose);
		newWin->showMinimized();
		newWin->showNormal();
		newWin->activateWindow();
		newWin->slot_doCmp();

		slot_recentCmpDir(leftPath, rightPath);
	}
}

void RealCompare::compareDir(QString leftDir, QString rightDir)
{
	QDir left(leftDir);
	QDir right(rightDir);

	if (left.exists() && right.exists())
	{
		CompareDirs * newWin = new CompareDirs(nullptr);
		m_cmpDirMgr.append(newWin);
		connect(newWin, &CompareDirs::signCmpDirClose, this, &RealCompare::slot_cmpDirClose);
		connect(newWin, &CompareDirs::receneDirPath, this, &RealCompare::slot_recentCmpDir);

		newWin->setAttribute(Qt::WA_DeleteOnClose);
		newWin->showMinimized();
		newWin->showNormal();
		newWin->activateWindow();
		newWin->setCmpFile(leftDir, rightDir);
	}
}
//初始化第一个主页面
void RealCompare::initMainTab()
{

}

void RealCompare::saveDefaultFontSize()
{
	if (CmpSql::isDbExist())
	{
		if (CompareWin::s_defaultFontSize > 0 && CompareWin::s_defaultFontSize < 20)
		{
			QString key("fontsize");
			CmpSql::updataKeyValueFromNumSets(key, CompareWin::s_defaultFontSize);
		}
	}
}

//保存最近对比到数据库
void RealCompare::saveReceneCmp()
{
	QString rDir("recentdir");
	QString rFile("recentfile");

	const int maxRecord = 200;

	if (CmpSql::isDbExist())
	{
		if (m_receneDirList.size() > maxRecord)
		{
			m_receneDirList = m_receneDirList.mid(0, maxRecord);
		}

		if (!m_receneDirList.isEmpty())
		{
			QString dirSaveText = m_receneDirList.join('|');
			CmpSql::updataKeyValueFromLongSets(rDir, dirSaveText);
		}

		if (m_receneFileList.size() > maxRecord)
		{
			m_receneFileList = m_receneFileList.mid(0, maxRecord);
		}

		if (!m_receneFileList.isEmpty())
		{
			QString fileSaveText = m_receneFileList.join('|');
			CmpSql::updataKeyValueFromLongSets(rFile, fileSaveText);
		}
	}
}

void RealCompare::initDefultFontSize()
{
	if (CmpSql::isDbExist())
	{
		QString key("fontsize");
		int fontSize = CmpSql::getKeyValueFromNumSets(key);
		if (fontSize >= 6 && fontSize <= 20)
		{
			CompareWin::s_defaultFontSize = fontSize;
		}
	}
}

//从数据库读取最近对比的文件列表。
//20260512 还是列表，但是不在菜单上面了，直接在界面的历史记录中记录，这样更直观，而且容量更大。
void RealCompare::initReceneCmp()
{
	QStringList fileItemHeadList;
	fileItemHeadList << tr("History Cmp File Path");

	QStringList dirItemHeadList;
	dirItemHeadList << tr("History Cmp Dir Path");

	QTreeWidgetItem* pFileItem = new QTreeWidgetItem(fileItemHeadList);
	pFileItem->setIcon(0, QIcon(":/Resources/img/file.png"));
	ui.cmpHistoryTreeWidget->addTopLevelItem(pFileItem);

	m_pTopFileItem = pFileItem;

	QTreeWidgetItem* pDirItem = new QTreeWidgetItem(dirItemHeadList);
	pDirItem->setIcon(0, QIcon(":/Resources/img/dir.png"));
	ui.cmpHistoryTreeWidget->addTopLevelItem(pDirItem);

	m_pTopDirItem = pDirItem;

	QHeaderView* header = ui.cmpHistoryTreeWidget->header();
	header->setSectionResizeMode(QHeaderView::ResizeToContents);

	// 开启交替行颜色
	ui.cmpHistoryTreeWidget->setAlternatingRowColors(true);

	QString qssItem = R"(
	QTreeWidget::item:selected:active{
	color:blue;
	}

	/* 窗口失焦时（灰色选中） */
	QTreeWidget::item:selected:!active{
	background-color: #E6F0FF;
	color:blue;
	})";

	ui.cmpHistoryTreeWidget->setStyleSheet(qssItem);

	connect(ui.cmpHistoryTreeWidget, &QTreeWidget::itemDoubleClicked, this, &RealCompare::on_itemDoubleClicked);

	/*ui.cmpHistoryTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(ui.cmpHistoryTreeWidget, &QTreeWidget::customContextMenuRequested,
		this, &RealCompare::on_treeWidget_customContextMenu);*/

	connect(ui.cmpHistoryTreeWidget, &QTreeWidget::itemPressed, this, &RealCompare::on_itemClicked);

	QString rDir("recentdir");
	QString rFile("recentfile");

	if (CmpSql::isDbExist())
	{
		QTreeWidgetItem* childItem = nullptr;

		QString dirStr = CmpSql::getKeyValueFromLongSets(rDir);

		QStringList dirList = dirStr.split('|');

		for (QString var : dirList)
		{
			if (!var.isEmpty() && (!m_receneRecrod.contains(var)))
			{
				dirItemHeadList.clear();
				dirItemHeadList << var;

				m_receneRecrod.insert(var, 1);

				childItem = new QTreeWidgetItem(dirItemHeadList, RC_DIR);

				pDirItem->addChild(childItem);
				m_receneDirList.append(var);
			}
		}

		QString fileStr = CmpSql::getKeyValueFromLongSets(rFile);

		QStringList fileList = fileStr.split('|');

		for (QString var : fileList)
		{
			if (!var.isEmpty() && (!m_receneRecrod.contains(var)))
			{
				fileItemHeadList.clear();
				fileItemHeadList << var;
				m_receneRecrod.insert(var, 1);

				childItem = new QTreeWidgetItem(fileItemHeadList, RC_FILE);

				pFileItem->addChild(childItem);

				m_receneFileList.append(var);
			}
		}
	}

	m_pTopFileItem->setExpanded(true);
}

//右键菜单
void RealCompare::on_itemClicked(QTreeWidgetItem* item, int /*column*/)
{
	if ((item != nullptr) && (Qt::RightButton == QGuiApplication::mouseButtons()))
	{
		if (m_menu == nullptr)
		{
			m_menu = new QMenu(this);

			m_menu->addAction(tr("Delete Path"), this, [&]() {
				QTreeWidgetItem* it = ui.cmpHistoryTreeWidget->currentItem();

				if (it != nullptr)
				{
					if (it == m_pTopDirItem || it == m_pTopFileItem)
					{
						ui.statusBar->showMessage(tr("Dir node can not delete !"));
						return;
					}
					QString text = it->text(0);

					m_receneRecrod.remove(text);

					if (it->type() == RC_FILE)
					{
						m_receneFileList.removeOne(text);
					}
					else  if (it->type() == RC_DIR)
					{
						m_receneDirList.removeOne(text);
					}

					delete it;

					//因为有删除，避免查找结果定位到野对象，缓存的查找结果必须清空。
					if (!m_findResult.isEmpty())
					{
						m_findResult.clear();
					}
				}

				});

			m_menu->addAction(tr("Find Path"), this, [&]() {
				on_findFile(true);
				});
		}
		m_menu->move(QCursor::pos());
		m_menu->show();
	}
	else
	{
		if (item == nullptr)
		{
			return;
		}

		QString path = item->text(0);
		ui.statusBar->showMessage(path);
	}
}

//双击打开。
void RealCompare::on_itemDoubleClicked(QTreeWidgetItem* item, int column)
{
	QString text = item->text(0);

	if (item->type() == RC_FILE)
	{
		openRecentFile(text);
	}
	else if (item->type() == RC_DIR)
	{
		openRecentDir(text);
	}
	
}

//今天是否已经签到过。如果获取数据库失败，则一律认定为没有
bool RealCompare::isTadayAlreadySgin()
{
	QString key("signdate");

	QString signDateStr = CmpSql::getKeyValueFromSets(key);

	//获取失败
	if (signDateStr.isEmpty())
	{
		return false;
	}

	QDate signDate = QDate::fromString(signDateStr, "yyyy/M/d");

	QDate now = QDate::currentDate();

	if (signDate < now)
	{
		//移动到接收到回复消息之后再更新
		return false;
	}

	return true;
}



void RealCompare::slot_cmpareFileBt()
{
	CompareWin * newWin = new CompareWin(nullptr);
	m_cmpFileMgr.append(newWin);
	connect(newWin, &CompareWin::signCmpFileClose, this, &RealCompare::slot_cmpFileClose);
	connect(newWin, &CompareWin::receneFilePath, this, &RealCompare::slot_recentCmpFile);

	newWin->setAttribute(Qt::WA_DeleteOnClose);
	newWin->show();
}

void RealCompare::slot_cmpareBinBt()
{
	CompareHexWin * newHexWin = new CompareHexWin(nullptr);
	newHexWin->setAttribute(Qt::WA_DeleteOnClose);
	newHexWin->show();
}


void RealCompare::slot_cmpareDirBt()
{
	CompareDirs * newWin = new CompareDirs(nullptr);
	m_cmpDirMgr.append(newWin);
	connect(newWin, &CompareDirs::signCmpDirClose, this, &RealCompare::slot_cmpDirClose);
	connect(newWin, &CompareDirs::receneDirPath, this, &RealCompare::slot_recentCmpDir);

	newWin->setAttribute(Qt::WA_DeleteOnClose);
	newWin->show();
}

int findChildItem(QTreeWidgetItem* rootNode, QString text)
{
	QTreeWidgetItemIterator it(rootNode);

	while (*it) {
		if ((*it)->text(0) == text)
		{
			return rootNode->indexOfChild(*it);
		}
		++it;
	}
	return -1;
}

//记录最近打开的目录对比信息
void RealCompare::slot_recentCmpDir(QString leftDirPath, QString rightDirPath)
{
	getRegularFilePath(leftDirPath);
	getRegularFilePath(rightDirPath);

	QString rDir = QString("%1 -> %2").arg(leftDirPath).arg(rightDirPath);

	bool ixExist = false;

	//已经在第一个，啥也不做
	if (!m_receneDirList.isEmpty())
	{
		QString firstText = m_receneDirList.first();
		if (firstText == rDir)
		{
			return;
		}
	}

	//是否已经存在，但是不在列表的第一行而已。
	ixExist = m_receneRecrod.contains(rDir);

	if (m_receneDirList.isEmpty())
	{
		QStringList dirItemList;
		dirItemList << rDir;
		QTreeWidgetItem* newFirst = new QTreeWidgetItem(dirItemList, RC_DIR);

		m_pTopFileItem->addChild(newFirst);
		ui.cmpHistoryTreeWidget->setCurrentItem(newFirst);

	}
	else
	{
		if (ixExist)
		{
			//找到对应的节点，移动到第一个去
			int index = findChildItem(m_pTopDirItem, rDir);

			if (index != -1)
			{
				//找到
				QTreeWidgetItem* pItem = m_pTopDirItem->takeChild(index);

				//放到第一个去。
				m_pTopDirItem->insertChild(0, pItem);
				ui.cmpHistoryTreeWidget->setCurrentItem(pItem);

				m_receneFileList.removeAt(index);
			}
			else
			{
				//这里应该是错误。居然没找到。

				QStringList dirItemList;
				dirItemList << rDir;
				QTreeWidgetItem* newFirst = new QTreeWidgetItem(dirItemList, RC_DIR);

				//放到第一个去。
				m_pTopDirItem->insertChild(0, newFirst);
				ui.cmpHistoryTreeWidget->setCurrentItem(newFirst);
			}
		}
		else
		{
			//这里应该是错误。居然没找到。

			QStringList dirItemList;
			dirItemList << rDir;
			QTreeWidgetItem* newFirst = new QTreeWidgetItem(dirItemList, RC_DIR);

			//放到第一个去。
			m_pTopDirItem->insertChild(0, newFirst);
			ui.cmpHistoryTreeWidget->setCurrentItem(newFirst);
		}
	}

	m_receneDirList.push_front(rDir);

	if (!ixExist)
	{
		m_receneRecrod.insert(rDir, 1);
	}
}

//记录最近打开的文件对比信息
void RealCompare::slot_recentCmpFile(QString leftFilePath, QString rightFilePath)
{
	getRegularFilePath(leftFilePath);
	getRegularFilePath(rightFilePath);

	//如果是svn对比的临时目录文件，则不记录。
	if (leftFilePath.contains("AppData\\Local\\Temp") || leftFilePath.contains("AppData/Local/Temp"))
	{
		return;
	}

	QString rFile = QString("%1 -> %2").arg(leftFilePath).arg(rightFilePath);
	bool ixExist = false;

	//已经在第一个，啥也不做
	if (!m_receneFileList.isEmpty())
	{
		QString firstText = m_receneFileList.first();
		if (firstText == rFile)
		{
			return;
		}
	}

	//是否已经存在，但是不在列表的第一行而已。
	ixExist = m_receneRecrod.contains(rFile);

	if (m_receneFileList.isEmpty())
	{
		QStringList dirItemList;
		dirItemList << rFile;
		QTreeWidgetItem* newFirst = new QTreeWidgetItem(dirItemList, RC_FILE);

		m_pTopFileItem->addChild(newFirst);

		ui.cmpHistoryTreeWidget->setCurrentItem(newFirst);
	}
	else
	{
		if (ixExist)
		{
			//找到对应的节点，移动到第一个去
			int index = findChildItem(m_pTopFileItem, rFile);

			if (index != -1)
			{
				//找到
				QTreeWidgetItem* pItem = m_pTopFileItem->takeChild(index);

				//放到第一个去。
				m_pTopFileItem->insertChild(0, pItem);
				ui.cmpHistoryTreeWidget->setCurrentItem(pItem);

				m_receneFileList.removeAt(index);
			}
			else
			{
				//这里应该是错误。居然没找到。容错逻辑，新增加到第一个。

				QStringList dirItemList;
				dirItemList << rFile;
				QTreeWidgetItem* newFirst = new QTreeWidgetItem(dirItemList, RC_FILE);

				//放到第一个去。
				m_pTopFileItem->insertChild(0, newFirst);
				ui.cmpHistoryTreeWidget->setCurrentItem(newFirst);
			}
		}
		else
		{
			//不存在，新增放到第一个
			QStringList dirItemList;
			dirItemList << rFile;
			QTreeWidgetItem* newFirst = new QTreeWidgetItem(dirItemList, RC_FILE);

			//放到第一个去。
			m_pTopFileItem->insertChild(0, newFirst);
			ui.cmpHistoryTreeWidget->setCurrentItem(newFirst);
		}
	}

	m_receneFileList.push_front(rFile);

	if (!ixExist)
	{
		m_receneRecrod.insert(rFile, 1);
	}
}

//文档编码转换
void RealCompare::slot_codeConvert()
{
	EncodeConvert* newWin = new EncodeConvert(nullptr);
	newWin->setAttribute(Qt::WA_DeleteOnClose);
	newWin->setWindowModality(Qt::ApplicationModal);
	newWin->show();
}

//文件夹对比关闭
void RealCompare::slot_cmpDirClose()
{
	CompareDirs* p = dynamic_cast<CompareDirs*>(sender());

	if (p != nullptr)
	{
		m_cmpDirMgr.removeOne(p);
	}
}

//更新使用计数
void RealCompare::updateCmpTimes()
{
	QString key("utimes");

	if (CmpSql::isDbExist())
	{
		int times = CmpSql::getKeyValueFromNumSets(key);
		times += CompareWin::s_useTimes;

		CmpSql::updataKeyValueFromNumSets(key, times);
	}
}

void RealCompare::updateServerVersion()
{
	if (CmpSql::isDbExist())
	{
		int newVersion = CmpSql::getKeyValueFromNumSets(QString("version"));

		if (m_serverNewVersion > newVersion)
		{
			CmpSql::updataKeyValueFromNumSets(QString("version"), m_serverNewVersion);
		}
	}
}

RealCompare::~RealCompare()
{
	updateServerVersion();

	updateCmpTimes();

	saveReceneCmp();

	saveDefaultFontSize();

	CmpSql::close();

}

void RealCompare::closeEvent(QCloseEvent* e)
{
	if (!m_cmpDirMgr.isEmpty() || !m_cmpFileMgr.isEmpty())
	{
		if (QMessageBox::No == QMessageBox::question(this, tr("Close ?"), tr("already has child window open, close all ?")))
		{
			e->ignore();
			return;
		}

		while (!m_cmpDirMgr.isEmpty())
		{
			CompareDirs* p = m_cmpDirMgr.takeFirst();
			p->close();
		}

		while (!m_cmpFileMgr.isEmpty())
		{
			CompareWin* p = m_cmpFileMgr.takeFirst();
			p->close();
		}
	}
}

#ifdef Q_OS_WIN
static const ULONG_PTR CUSTOM_TYPE = 10000;
const ULONG_PTR OPEN_CC_LEFT_FILE = 10001;
const ULONG_PTR OPEN_CC_RIGHT_FILE = 10002;
const ULONG_PTR OPEN_CC_LEFT_DIR = 10003;
const ULONG_PTR OPEN_CC_RIGHT_DIR = 10004;
const ULONG_PTR OPEN_NOTEPAD_TYPE = 10005;

void RealCompare::setLeftCmpFile(QString file)
{
	m_leftCmpFile = file;
}

void RealCompare::setRightCmpFile(QString file)
{
	m_rightCmpFile = file;
}

void RealCompare::setLeftCmpDir(QString path)
{
	m_leftCmpDir = path;
}

void RealCompare::setRightCmpDir(QString path)
{
	m_rightCmpDir = path;
}

bool RealCompare::nativeEvent(const QByteArray & eventType, void * message, long * result)
{
	MSG *param = static_cast<MSG *>(message);

	switch (param->message)
	{
		case WM_COPYDATA:
		{
			COPYDATASTRUCT *cds = reinterpret_cast<COPYDATASTRUCT*>(param->lParam);
			if (cds->dwData == OPEN_CC_LEFT_FILE)
			{
				m_leftCmpFile = QString::fromUtf8(reinterpret_cast<char*>(cds->lpData), cds->cbData);
				*result = 1;

				if (!m_leftCmpFile.isEmpty() && !m_rightCmpFile.isEmpty())
				{
					compareFile(m_leftCmpFile, m_rightCmpFile);
					m_leftCmpFile.clear();
					m_rightCmpFile.clear();
				}
				return true;
			}
			else if (cds->dwData == OPEN_CC_RIGHT_FILE)
			{
				m_rightCmpFile = QString::fromUtf8(reinterpret_cast<char*>(cds->lpData), cds->cbData);
				*result = 1;

				if (!m_leftCmpFile.isEmpty() && !m_rightCmpFile.isEmpty())
				{
					compareFile(m_leftCmpFile, m_rightCmpFile);
					m_leftCmpFile.clear();
					m_rightCmpFile.clear();
				}
				return true;
			}
			else if (cds->dwData == OPEN_CC_LEFT_DIR)
			{
				m_leftCmpDir = QString::fromUtf8(reinterpret_cast<char*>(cds->lpData), cds->cbData);
				*result = 1;

				if (!m_leftCmpDir.isEmpty() && !m_rightCmpDir.isEmpty())
				{
					compareDir(m_leftCmpDir, m_rightCmpDir);
					m_leftCmpDir.clear();
					m_rightCmpDir.clear();
				}
				return true;
			}
			else if (cds->dwData == OPEN_CC_RIGHT_DIR)
			{
				m_rightCmpDir = QString::fromUtf8(reinterpret_cast<char*>(cds->lpData), cds->cbData);
				*result = 1;

				if (!m_leftCmpDir.isEmpty() && !m_rightCmpDir.isEmpty())
				{
					compareDir(m_leftCmpDir, m_rightCmpDir);
					m_leftCmpDir.clear();
					m_rightCmpDir.clear();
				}
				return true;
			}
			else if (cds->dwData == OPEN_NOTEPAD_TYPE)
			{
				//activateWindow();
				//QApplication::alert(this);
				winTopShow();
				*result = 1;
				return true;
			}
		}
	}

	return QWidget::nativeEvent(eventType, message, result);
}
#endif

void RealCompare::winTopShow()
{
	//窗口如果最小化，则在任务栏下面闪动
	QApplication::alert(this);

	//如果最小化了，先恢复最大化一下，避免看不见。
	if (this->isMinimized())
	{
		if (this->isMaximized())
		{
			this->showMaximized();
		}
		else
		{
			this->showNormal();
		}
	}
	else
	{
		//没有最小化，直接显示
		//这样没法生效，如果被完全遮挡，还是在后面。必须要先置顶，再显示，再取消才行。
		QWindow* pWin = this->windowHandle();
		if (pWin != nullptr)
		{
			pWin->setFlags(this->windowFlags() | Qt::WindowStaysOnTopHint);
			this->showNormal();
			pWin->setFlags(this->windowFlags() & ~Qt::WindowStaysOnTopHint);
		}
	}

	QWindow* pWin = this->windowHandle();
	if (pWin != nullptr)
	{
		pWin->requestActivate();
	}
}

//文件对比关闭
void RealCompare::slot_cmpFileClose()
{
	CompareWin* p = dynamic_cast<CompareWin*>(sender());

	if (p != nullptr)
	{
		m_cmpFileMgr.removeOne(p);
	}
}

void RealCompare::slot_changeEnglish()
{
	m_translator->load("");
	qApp->installTranslator(m_translator);
	ui.retranslateUi(this);
}

void RealCompare::slot_changeChinese()
{
	if (m_translator->load(":/realcompare_zh.qm"))
	{
		qApp->installTranslator(m_translator);
		ui.retranslateUi(this);
	}
}

QStringList RealCompare::getLocalMac()
{
	QStringList mac_list;
	QString strMac;
	QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
	for (int i = 0; i < ifaces.count(); i++)
	{
		QNetworkInterface iface = ifaces.at(i);
		//过滤掉本地回环地址、没有开启的地址
		if (iface.flags().testFlag(QNetworkInterface::IsUp) && \
			iface.flags().testFlag(QNetworkInterface::IsRunning) && \
			iface.flags().testFlag(QNetworkInterface::CanBroadcast) && \
			!iface.flags().testFlag(QNetworkInterface::IsLoopBack))
		{
			//过滤掉虚拟地址
			if (!(iface.humanReadableName().contains("VMware", Qt::CaseInsensitive)))
			{
				strMac = iface.hardwareAddress();
				if (!strMac.isEmpty() && strMac.compare("00:00:00:00:00:00") != 0)
				{
					strMac.replace(":", "");
					mac_list.append(strMac);
					break;
				}
			}
		}
	}
	//实在没有找到合适的，就放宽条件选第一个
	if (mac_list.isEmpty() && !ifaces.isEmpty())
	{
		for (int i = 0; i < ifaces.count(); i++)
		{
			QNetworkInterface iface = ifaces.at(i);
			//过滤掉本地回环地址、没有开启的地址
			if (!iface.flags().testFlag(QNetworkInterface::IsLoopBack))
			{
				//过滤掉虚拟地址
				if (!(iface.humanReadableName().contains("VMware", Qt::CaseInsensitive)))
				{
					strMac = iface.hardwareAddress();
					if (!strMac.isEmpty() && strMac.compare("00:00:00:00:00:00") != 0)
					{
						strMac.replace(":", "");
						mac_list.append(strMac);
						break;
					}
				}
			}
		}
	}
	return mac_list;
}

QString RealCompare::getLocalFirstMac()
{
	QString key("mac");
	QString v = CmpSql::getKeyValueFromSets(key);

	if (v.isEmpty() || v == "0")
	{

	QStringList macList = getLocalMac();
	if (!macList.isEmpty())
	{
			CmpSql::updataKeyValueFromSets(key, macList[0]);
		return macList[0];
	}
	return QString();
}

	return v;
}


void RealCompare::slot_donate()
{
	Donate* pWin = new Donate();
	pWin->setWindowIcon(QIcon(":/Resources/img/main.png"));
	pWin->setAttribute(Qt::WA_DeleteOnClose);
	pWin->show();
}


void RealCompare::openRecentDir(QString text)
{
	QStringList fileList = text.split(" -> ");
	if (fileList.size() == 2)
	{

		QDir left(fileList.at(0));
		QDir right(fileList.at(1));

		if (left.exists() && right.exists())
		{
			CompareDirs * newWin = new CompareDirs(nullptr);
			m_cmpDirMgr.append(newWin);
			connect(newWin, &CompareDirs::signCmpDirClose, this, &RealCompare::slot_cmpDirClose);
			connect(newWin, &CompareDirs::receneDirPath, this, &RealCompare::slot_recentCmpDir);

			newWin->setAttribute(Qt::WA_DeleteOnClose);
			newWin->show();
			newWin->setCmpFile(fileList.at(0), fileList.at(1));
			return;
		}
	}
	else
	{
		QMessageBox::information(this, tr("notice"), tr("file path not exist, remove recent record!"));
	}
	
}

void RealCompare::openRecentFile(QString text)
{
	QStringList fileList = text.split(" -> ");
	if (fileList.size() == 2)
	{
		QString leftPath = fileList.at(0);
		QString rightPath = fileList.at(1);
		bool isCmpHex = false;

		if (QFile::exists(fileList.at(0)) && QFile::exists(fileList.at(1)))
		{
			if (!DocTypeListView::isSupportExt(CompareDirs::fileSuffix(leftPath)) || !DocTypeListView::isSupportExt(CompareDirs::fileSuffix(rightPath)))
			{
				if (QMessageBox::Yes == QMessageBox::question(this, "Notice", tr("file [%1] may be not a text file, cmp in hex mode?").arg(leftPath)))
				{
					isCmpHex = true;
				}
			}
			if(!isCmpHex)
			{
			CompareWin * newWin = new CompareWin(fileList.at(0), fileList.at(1), nullptr);
			m_cmpFileMgr.append(newWin);
			connect(newWin, &CompareWin::signCmpFileClose, this, &RealCompare::slot_cmpFileClose);
			connect(newWin, &CompareWin::receneFilePath, this, &RealCompare::slot_recentCmpFile);

			newWin->setAttribute(Qt::WA_DeleteOnClose);
			newWin->show();
			newWin->slot_doCmp();
			}
			else
			{
				CompareHexWin * newWin = new CompareHexWin(fileList.at(0), fileList.at(1), nullptr);
				newWin->setAttribute(Qt::WA_DeleteOnClose);
				newWin->show();
				newWin->slot_doCmp();
			}
			return;
		}
	}
	else
	{
		QMessageBox::information(this, tr("notice"), tr("file path not exist, remove recent record!"));
	}
}

void RealCompare::slot1_mainweb()
{
	QUrl url("https://gitee.com/cxasm/cc-compare");
	QDesktopServices::openUrl(url);
}

void RealCompare::slot_newVersion(const QString &link)
{
	QUrl url(link);
	QDesktopServices::openUrl(url);
}
