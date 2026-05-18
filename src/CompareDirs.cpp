#include "CompareDirs.h"
#include <QFileDialog>
#include <QTreeWidgetItem>
#include <QStyleFactory> 
#include <QFutureWatcher>
#include <QDateTime>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QFont>
#include <QClipboard>
#include <QHeaderView>

#include "rcglobal.h"
#include "QTreeWidgetSortItem.h"
#include "MediatorFileTree.h"
#include "RcTreeWidget.h"
#include "comparewin.h"
#include "dirfindfile.h"
#include "progresswin.h"
#include "optionsview.h"
#include "doctypelistview.h"
#include "cmpsql.h"
#include "comparehexwin.h"
#include "draglineedit.h"
#include "comparedirsdata.h"

#ifdef _DEBUG
#include <QThreadPool>
#endif

//超过文件数量则开启进度条。20221104修改，发现1个文件如果很大，也是很慢的。无条件需要进度条
const int FILE_NUM_PROCESS_LIMIT = 0;

//item是否相等的表示。1 不等，其他：相等。没有也当相等
const int Item_Eqult_Status = Qt::UserRole +3;

bool CompareDirs::s_compareHideDir = false;

bool CompareDirs::s_compareChildDir = true;

//1对比所有 2:对比支持的ext
int CompareDirs::s_compareAllFiles = 1; 

//0 快速模式 1 慢速模式，深入对比文本是否有不同点
int CompareDirs::s_compareMode = 0;

//0不跳过 1 跳过
int CompareDirs::s_isSkipDir = 0;
int CompareDirs::s_isSkipFile = 0;
int CompareDirs::s_isSkipNamePrefix = 0;

QString CompareDirs::s_skipDirs = QString(".svn:.vs");
QString CompareDirs::s_skipFileExt = QString(".sln:.vcxproj");
QString CompareDirs::s_skipNamePrefix = QString("ui_");

//独有颜色
static QColor ALONE_COLOR(Qt::blue);
static QColor UN_EQUAL_COLOR(Qt::red);
static QColor EQUAL_COLOR(Qt::black);

QString formatDirFilePath(QString filepath, bool * isChange = nullptr)
{

	//发现在macos下面，如果目录以 \ /结尾，会导致一些识别错误的问题。
	if(!filepath.isEmpty() && (filepath.endsWith('/') || filepath.endsWith('\\')))
	{
		filepath = filepath.mid(0, filepath.size() - 1);

		if (isChange != nullptr)
		{
			*isChange = true;
	}
	}

	return filepath;
}

CompareDirs::CompareDirs(QWidget *parent)
	: QMainWindow(parent), m_currentLeftFindIndex(0), m_currentRightFindIndex(0), m_findCaseSenstive(true), m_findWholeWord(false),m_leftFileNums(0), \
	m_rightFileNums(0), m_leftRightOrder(0), m_isFirstDirCmp(true), m_workStatus(FREE_STATUS), m_isCompareCancel(false),m_doWithAsk(true),m_commitCmpFileNums(0), m_loadDirCancel(false),\
	m_workThread(nullptr), m_cmpDataThread(nullptr), m_curRootDirNode(nullptr), m_loadDirFinished(false)
{
	ui.setupUi(this);

#ifdef _DEBUG
	//QThreadPool::globalInstance()->setMaxThreadCount(1);
#endif

	//ui.mainToolBar->setStyleSheet(toolbarQss);
	DocTypeListView::initSupportFileTypes();

	connect(this, &CompareDirs::delayWork,this,&CompareDirs::slot_delayWork, Qt::QueuedConnection);

	QToolButton* ruleBt = new QToolButton(NULL);
	ruleBt->setFixedSize(48, 48);
	ruleBt->setIcon(QIcon(":/Resources/img/rule.png"));
	ruleBt->setText(tr("rule"));
	ruleBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(ruleBt);
	connect(ruleBt, &QToolButton::clicked, this, &CompareDirs::slot_rultBt);

	ui.mainToolBar->addSeparator();
	QToolButton* showAllBt = new QToolButton(NULL);
	showAllBt->setFixedSize(48, 48);
	showAllBt->setIcon(QIcon(":/Resources/img/showall.png"));
	showAllBt->setText(tr("all"));
	showAllBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(showAllBt);
	connect(showAllBt, &QToolButton::clicked, this, &CompareDirs::slot_showAllBt);

	QToolButton* showDiffBt = new QToolButton(NULL);
	showDiffBt->setFixedSize(48, 48);
	showDiffBt->setIcon(QIcon(":/Resources/img/diff.png"));
	showDiffBt->setText(tr("diff"));
	showDiffBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(showDiffBt);
	connect(showDiffBt, &QToolButton::clicked, this, &CompareDirs::slot_showDiffBt);

	QToolButton* showOnlyDiffBt = new QToolButton(NULL);
	showOnlyDiffBt->setFixedSize(48, 48);
	showOnlyDiffBt->setIcon(QIcon(":/Resources/img/onlydiff.png"));
	showOnlyDiffBt->setText(tr("only diff"));
	showOnlyDiffBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(showOnlyDiffBt);
	connect(showOnlyDiffBt, &QToolButton::clicked, this, &CompareDirs::slot_showOnlyDiffBt);

	QToolButton* showUniqueBt = new QToolButton(NULL);
	showUniqueBt->setFixedSize(48, 48);
	showUniqueBt->setIcon(QIcon(":/Resources/img/unique.png"));
	showUniqueBt->setText(tr("unique"));
	showUniqueBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(showUniqueBt);
	connect(showUniqueBt, &QToolButton::clicked, this, &CompareDirs::slot_showUniqueBt);


	ui.mainToolBar->addSeparator();

	//
	QToolButton* expandBt = new QToolButton(NULL);
	expandBt->setFixedSize(48, 48);
	expandBt->setIcon(QIcon(":/Resources/img/expand.png"));
	expandBt->setText(tr("expand"));
	expandBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(expandBt);
	connect(expandBt, &QToolButton::clicked, this, &CompareDirs::slot_expandBt);

	QToolButton* foldBt = new QToolButton(NULL);
	foldBt->setFixedSize(48, 48);
	foldBt->setIcon(QIcon(":/Resources/img/fold.png"));
	foldBt->setText(tr("fold"));
	foldBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(foldBt);
	connect(foldBt, &QToolButton::clicked, this, &CompareDirs::slot_foldBt);


	ui.mainToolBar->addSeparator();

	QToolButton* clearBt = new QToolButton(NULL);
	clearBt->setFixedSize(48, 48);
	clearBt->setIcon(QIcon(":/Resources/img/clear.png"));
	clearBt->setText(tr("clear"));
	clearBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(clearBt);
	connect(clearBt, &QToolButton::clicked, this, &CompareDirs::slot_clearBt);

	QToolButton* swapBt = new QToolButton(NULL);
	swapBt->setFixedSize(48, 48);
	swapBt->setIcon(QIcon(":/Resources/img/swap.png"));
	swapBt->setText(tr("swap"));
	swapBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(swapBt);
	connect(swapBt, &QToolButton::clicked, this, &CompareDirs::slot_swapBt);

	QToolButton* reloadBt = new QToolButton(NULL);
	reloadBt->setFixedSize(48, 48);
	reloadBt->setIcon(QIcon(":/Resources/img/reload.png"));
	reloadBt->setText(tr("reload"));
	reloadBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(reloadBt);
	connect(reloadBt, &QToolButton::clicked, this, &CompareDirs::slot_reloadBt);

	QToolButton* findBt = new QToolButton(NULL);
	findBt->setFixedSize(48, 48);
	findBt->setIcon(QIcon(":/Resources/img/find.png"));
	findBt->setText(tr("find"));
	findBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(findBt);
	connect(findBt, &QToolButton::clicked, this, &CompareDirs::slot_findFile);


	m_curItemIndex = 0;

	//设置左边的缩起来按钮为加号，而不是箭头
	//ui.leftTreeDir->setStyle(QStyleFactory::create("windows"));
	//ui.rightTreeDir->setStyle(QStyleFactory::create("windows"));

	//设置item的行高
	ui.leftTreeDir->setStyleSheet("QTreeWidget::item{height:22px} QTreeView::item::selected{background-color:#00CCFF;}");
	ui.rightTreeDir->setStyleSheet("QTreeWidget::item{height:22px} QTreeView::item::selected{background-color:#00CCFF;}");


	ui.leftTreeDir->setDirection(RC_LEFT);
	ui.rightTreeDir->setDirection(RC_RIGHT);

	m_mediator = new MediatorFileTree();

	ui.leftTreeDir->setMediator(m_mediator);
	ui.rightTreeDir->setMediator(m_mediator);

	ui.leftTreeDir->setAlternatingRowColors(true);
	ui.rightTreeDir->setAlternatingRowColors(true);


	m_menuCallbackFun = std::bind(&CompareDirs::contextUserDefineItemMenuEvent,this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

	ui.leftTreeDir->setContextUserDefineItemMenuCallBack(&m_menuCallbackFun);
	ui.rightTreeDir->setContextUserDefineItemMenuCallBack(&m_menuCallbackFun);

	m_dataMode = new CompareDirMode;

	//用于同步左右两边的滚动条
	connect(m_mediator, &MediatorFileTree::syncCurScrollValue, this, &CompareDirs::slot_syncCurScrollValue);

	//用于同步左右两边的ITEM展开与收起
	connect(m_mediator, &MediatorFileTree::syncExpandStatus, this, &CompareDirs::slot_syncExpandStatus);

	connect(ui.leftTreeDir, &QTreeWidget::itemDoubleClicked, this, &CompareDirs::slot_itemLeftDoubleClick);

	connect(ui.rightTreeDir, &QTreeWidget::itemDoubleClicked, this, &CompareDirs::slot_itemRightDoubleClick);

	connect(ui.leftTreeDir, &QTreeWidget::itemClicked, this, &CompareDirs::slot_itemLeftClick);

	connect(ui.rightTreeDir, &QTreeWidget::itemClicked, this, &CompareDirs::slot_itemRightClick);

	//监控回车
	connect(ui.leftPath, &QLineEdit::returnPressed, this, &CompareDirs::slot_leftPathReturnPressed);
	connect(ui.rightPath, &QLineEdit::returnPressed, this, &CompareDirs::slot_rightPathReturnPressed);

	QHeaderView* leftHeadView = ui.leftTreeDir->header();
	QHeaderView* rightHeadView = ui.rightTreeDir->header();

	//禁用自定义排序，点击手动去排序
	ui.leftTreeDir->setSortingEnabled(false);
	leftHeadView->setSortIndicatorShown(true);
	leftHeadView->setSectionsClickable(true);

	ui.rightTreeDir->setSortingEnabled(false);
	rightHeadView->setSortIndicatorShown(true);
	rightHeadView->setSectionsClickable(true);

	connect(leftHeadView, SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(slot_onLeftSort(int, Qt::SortOrder)));
	connect(rightHeadView, SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(slot_onRightSort(int, Qt::SortOrder)));

	//顺序改变后，另外一边去同步调整顺序。必须是队列连接，否则顺序可能失效
	connect(this, &CompareDirs::synvOrder, this, &CompareDirs::slot_syncOrder, Qt::QueuedConnection);

#if 0
	//表头的点击排序同步。暂时不做，先把文件大小做出来
	QTreeWidgetItem *leftHeadItem = ui.leftTreeDir->headerItem();
	leftHeadItem->setDisabled(true);
	QTreeWidgetItem *rightHeadItem = ui.rightTreeDir->headerItem();
	rightHeadItem->setDisabled(true);
#endif

	m_findFileWin = nullptr;
	m_loadFileProcessWin = nullptr;

	//启用拖动
	setAcceptDrops(true);

	//设置查找的快捷键
	QAction* findAction = new QAction(this);
	QKeySequence findKey = QKeySequence(Qt::CTRL | Qt::Key_F);
	findAction->setShortcut(findKey);

	connect(findAction, &QAction::triggered, this, &CompareDirs::slot_findFile);
	this->addAction(findAction);

	m_sbarWorkStatusLable = new QLabel(tr("Status: normal"), ui.statusBar);

	//0在前面，越小越在左边
	ui.statusBar->insertPermanentWidget(0, m_sbarWorkStatusLable);

	//ui.leftTreeDir->sortItems(0, Qt::AscendingOrder);
	//ui.rightTreeDir->sortItems(0, Qt::AscendingOrder);

	CmpSql::init();

	//开启工作线程
	createWorkThread();
	QString key("cmphidefile");
	s_compareHideDir = CmpSql::getKeyValueFromNumSets(key);

	QString key1("cmpallfile");
	s_compareAllFiles = CmpSql::getKeyValueFromNumSets(key1);

	QString key2("dirmode");
	s_compareMode = CmpSql::getKeyValueFromNumSets(key2);

	QString key3("skipdir");
	s_isSkipDir = CmpSql::getKeyValueFromNumSets(key3);
	s_skipDirs = CmpSql::getKeyValueFromSets(key3);

	QString key4("skipext");
	s_isSkipFile = CmpSql::getKeyValueFromNumSets(key4);
	s_skipFileExt = CmpSql::getKeyValueFromSets(key4);

	QString key5("skipprefix");
	s_isSkipNamePrefix = CmpSql::getKeyValueFromNumSets(key5);
	s_skipNamePrefix = CmpSql::getKeyValueFromSets(key5);

	QString key6("childdirs");
	//避免读取不到时，默认是0返回，所以不使用0，必须为2才是不处理
	s_compareChildDir = ((2 == CmpSql::getKeyValueFromNumSets(key6)) ? false : true);


}

CompareDirs::~CompareDirs()
{
	if (m_mediator != nullptr)
	{
		delete m_mediator;
		m_mediator = nullptr;
	}

	if (m_dataMode != nullptr)
	{
		delete m_dataMode;
		m_dataMode = nullptr;
	}

	if (m_findFileWin != nullptr)
	{
		delete m_findFileWin;
		m_findFileWin = nullptr;
	}

	if (m_loadFileProcessWin != nullptr)
	{
		delete m_loadFileProcessWin;
		m_loadFileProcessWin = nullptr;
	}

	if (m_workThread != nullptr)
	{
		m_workThread->quit();
		m_workThread->wait();
		delete m_workThread;
		m_workThread = nullptr;
	}
	QString key("cmphidefile");
	CmpSql::updataKeyValueFromNumSets(key, s_compareHideDir?1:0);

	QString key1("cmpallfile");
	CmpSql::updataKeyValueFromNumSets(key1, s_compareAllFiles);

	QString key2("dirmode");
	CmpSql::updataKeyValueFromNumSets(key2, s_compareMode);

	QString key3("skipdir");
	CmpSql::updataKeyValueFromNumSets(key3, s_isSkipDir);
	CmpSql::updataKeyValueFromSets(key3, s_skipDirs);

	QString key4("skipext");
	CmpSql::updataKeyValueFromNumSets(key4, s_isSkipFile);
	CmpSql::updataKeyValueFromSets(key4, s_skipFileExt);


	QString key5("skipprefix");
	CmpSql::updataKeyValueFromNumSets(key5, s_isSkipNamePrefix);
	CmpSql::updataKeyValueFromSets(key5, s_skipNamePrefix);

	QString key6("childdirs");
	CmpSql::updataKeyValueFromNumSets(key6, s_compareChildDir ? 1 : 2);

	CmpSql::close();
}


void CompareDirs::slot_onLeftSort(int logicalIndex, Qt::SortOrder order)
{
	QTreeWidgetSortItem::setSortColumn(logicalIndex);

	disconnect(ui.leftTreeDir->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(slot_onLeftSort(int, Qt::SortOrder)));

	ui.leftTreeDir->sortItems(logicalIndex, order);

	connect(ui.leftTreeDir->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(slot_onLeftSort(int, Qt::SortOrder)));

	emit synvOrder(0,order);
}

void CompareDirs::slot_onRightSort(int logicalIndex, Qt::SortOrder order)
{
	QTreeWidgetSortItem::setSortColumn(logicalIndex);

	disconnect(ui.rightTreeDir->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(slot_onRightSort(int, Qt::SortOrder)));

	ui.rightTreeDir->sortItems(logicalIndex, order);

	connect(ui.rightTreeDir->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(slot_onRightSort(int, Qt::SortOrder)));

	emit synvOrder(1, order);
}

//使左右两边能够同步排序显示
void CompareDirs::slot_syncOrder(int dire, Qt::SortOrder order)
{
	auto loopChildItem = [](QTreeWidgetItem* dirItem, QMap<QString, fileAttriNode>& fileAttrisMap)
	{
		QList< QTreeWidgetItem*> dirNodes;
		dirNodes.append(dirItem);

		while (!dirNodes.isEmpty())
			{
			QTreeWidgetItem* rootItem = dirNodes.takeLast();

			for (int i = 0, s = rootItem->childCount(); i < s; ++i)
				{
				QTreeWidgetItem* it = rootItem->child(i);
				QString relativePathName = it->data(0, Item_RelativePath).toString();

				QMap<QString, fileAttriNode>::iterator otherIt = fileAttrisMap.find(relativePathName);
				if (otherIt != fileAttrisMap.end())
				{
					otherIt.value().selfItem->setData(0, Item_Index, i);
				}
				else
				{
					//qDebug() << relativePathName;
					}

				//如果遇到文件夹
				if (it->childCount() > 0)
					{
					//qDebug() << relativePathName << i;

					dirNodes.push_front(it);
						}
					}
				}
	};

	if (dire == 0)
			{
		//qDebug() << ui.leftTreeDir->topLevelItemCount();
		QTreeWidgetItem* rootItem = ui.leftTreeDir->topLevelItem(0);

		//20221026 发现bug,在清空文件夹对比后，重新加载对比时，加载右边树目录，此时左边目前树还是空导致崩溃
		if (rootItem == nullptr)
		{
			return;
		}

		//qDebug() << rootItem->childCount();

		loopChildItem(rootItem, m_rightFileAttrisMap);

		disconnect(ui.rightTreeDir->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(slot_onRightSort(int, Qt::SortOrder)));

		QTreeWidgetSortItem::setSyncOrder(true);
		QTreeWidgetSortItem::setSortColumn(0);
		ui.rightTreeDir->sortItems(0, Qt::DescendingOrder);
		QTreeWidgetSortItem::setSyncOrder(false);

		connect(ui.rightTreeDir->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(slot_onRightSort(int, Qt::SortOrder)));

	}
	else
	{
		//qDebug() << ui.rightTreeDir->topLevelItemCount();
		QTreeWidgetItem* rootItem = ui.rightTreeDir->topLevelItem(0);
		if (rootItem == nullptr)
		{
			return;
		}

		//qDebug() << rootItem->childCount();

		loopChildItem(rootItem, m_leftFileAttrisMap);

		disconnect(ui.leftTreeDir->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(slot_onLeftSort(int, Qt::SortOrder)));

		QTreeWidgetSortItem::setSyncOrder(true);
		QTreeWidgetSortItem::setSortColumn(0);
		ui.leftTreeDir->sortItems(0, Qt::DescendingOrder);
		QTreeWidgetSortItem::setSyncOrder(false);
		connect(ui.leftTreeDir->header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(slot_onLeftSort(int, Qt::SortOrder)));

	}
}

void CompareDirs::slot_leftPathReturnPressed()
{
	m_isFirstDirCmp = false;
	QString rootpath = ui.leftPath->text();

	QDir d(rootpath);
	if (!d.exists())
	{
		ui.leftPath->clear();
		ui.statusBar->showMessage(tr("error: %1 not a dir !").arg(rootpath));
		return;
	}
	if (!rootpath.isEmpty())
	{
		/*m_leftFileNums = loadDir(RC_LEFT, rootpath);
		if (m_rightFileNums > 0)
		{
	cmpFileTree();
		}*/

		ui.leftPath->setText(formatDirFilePath(rootpath));

		if (!ui.leftPath->text().isEmpty() && !ui.rightPath->text().isEmpty())
		{
			setCmpFile(ui.leftPath->text(), ui.rightPath->text());
}
	}
}


void CompareDirs::slot_rightPathReturnPressed()
{
	m_isFirstDirCmp = false;

	QString rootpath = ui.rightPath->text();
	QDir d(rootpath);
	if (!d.exists())
	{
		ui.rightPath->clear();
		ui.statusBar->showMessage(tr("error: %1 not a dir !").arg(rootpath));
		return;
	}
	if (!rootpath.isEmpty())
	{
		/*m_rightFileNums = loadDir(RC_RIGHT, rootpath);
		if (m_leftFileNums > 0)
		{
			cmpFileTree();
		}*/

		ui.rightPath->setText(formatDirFilePath(rootpath));

		if (!ui.leftPath->text().isEmpty() && !ui.rightPath->text().isEmpty())
		{
			setCmpFile(ui.leftPath->text(), ui.rightPath->text());
		}
	}
}

void CompareDirs::slot_delayWork(int dir, QString path)
{ 
	//int fileNum = loadDir(dir, path);

	if (dir == RC_LEFT)
	{
		//m_leftFileNums = fileNum;
		ui.leftPath->setText(formatDirFilePath(path));
	}
	else
	{
		//m_rightFileNums = fileNum;
		ui.rightPath->setText(formatDirFilePath(path));
	}

	if (!ui.leftPath->text().isEmpty() && !ui.rightPath->text().isEmpty())
	{
		setCmpFile(ui.leftPath->text(), ui.rightPath->text());
	}
}

void CompareDirs::setWorkStatusInfo(QString info)
{
	m_sbarWorkStatusLable->setText(info);
}

void CompareDirs::slot_rultBt(bool)
{
	OptionsView* p = new OptionsView();
	p->setAttribute(Qt::WA_DeleteOnClose);
	p->setWindowModality(Qt::ApplicationModal);
	p->show();
}

void CompareDirs::slot_showAllBt(bool)
{
	auto work = [&](RcTreeWidget *treeDir) {

		QTreeWidgetItemIterator it(treeDir);
		while (*it) {
			if ((*it)->isHidden())
				(*it)->setHidden(false);
			++it;
		}
	};
	work(ui.leftTreeDir);
	work(ui.rightTreeDir);

	//一旦切换把查找的内容也置空，这样下次查找将无条件查找
	m_leftFindText.clear();
	m_rightFindText.clear();
}

void CompareDirs::slot_expandBt(bool)
{
	auto work = [&](RcTreeWidget *treeDir) {

		QTreeWidgetItemIterator it(treeDir);
		while (*it) {
			if (RC_DIR == (*it)->type())
			{
				(*it)->setExpanded(true);
			}
			++it;
		}
	};

	work(ui.leftTreeDir);
	work(ui.rightTreeDir);
}

void CompareDirs::slot_foldBt(bool)
{
	auto work = [&](RcTreeWidget *treeDir) {

		QTreeWidgetItemIterator it(treeDir);
		while (*it) {
			if (RC_DIR == (*it)->type())
			{
				if ((*it)->parent() != nullptr)
				{
					(*it)->setExpanded(false);
				}
			}
			++it;
		}
	};

	work(ui.leftTreeDir);
	work(ui.rightTreeDir);
}

//把item所在的parent node全部显示出来
void showItemParentNode(QTreeWidgetItem* item)
{
	QTreeWidgetItem* parent = item->parent();

	//qDebug() << item->text(0);

	while (parent != nullptr)
	{
		//qDebug() << parent->text(0);

		if (parent->isHidden())
		{
			parent->setHidden(false);
		}
		if (!parent->isExpanded())
		{
			parent->setExpanded(true);
		}
		parent = parent->parent();
	}
}


//对类型是status的文件，进行显示。如果是文件夹，则不会处理。
void showTreeDiffFileItem(RcTreeWidget* treeDir)
{
	QTreeWidgetItemIterator it(treeDir);
	while (*it) {
		if (NO_EQUAL_FILE == CompareDirs::getItemEqualStatus(*it) && ((*it)->type() == RC_FILE))
		{
			showItemParentNode(*it);
		}
		++it;
	}
}

void showTreeUniqueFileItem(RcTreeWidget* treeDir)
{
	ITEM_FILE_STATUS status;

	QTreeWidgetItemIterator it(treeDir);
	while (*it) {
		status = CompareDirs::getItemEqualStatus(*it);

		if ((ONLY_ONE_SIDE_FILE == status) || (LOST_SIDE_FILE == status))
		{
			showItemParentNode(*it);
		}
		++it;
	}
}

void CompareDirs::slot_showDiffBt(bool)
{
	auto work = [&](RcTreeWidget *treeDir){

		QTreeWidgetItemIterator it(treeDir);
		while (*it) {
			if (RC_DIR == (*it)->type())
			{
				(*it)->setExpanded(true);
			}
			else if (EQUAL_FILE >= getItemEqualStatus(*it))
			{
				(*it)->setHidden(true);
			}
			else
			{
				(*it)->setHidden(false);
			}
			++it;
		}
	};

	work(ui.leftTreeDir);
	work(ui.rightTreeDir);

	showTreeDiffFileItem(ui.leftTreeDir);
	showTreeDiffFileItem(ui.rightTreeDir);

	showTreeUniqueFileItem(ui.leftTreeDir);
	showTreeUniqueFileItem(ui.rightTreeDir);

	m_leftFindText.clear();
	m_rightFindText.clear();
}


//严格显示不同的，如果是独有则不显示
void CompareDirs::slot_showOnlyDiffBt(bool)
{
	auto work = [&](RcTreeWidget* treeDir) {

		QTreeWidgetItemIterator it(treeDir);
		while (*it) {
			if (RC_DIR == (*it)->type())
			{
				(*it)->setHidden(true);
				(*it)->setExpanded(false);
			}
			else if (NO_EQUAL_FILE == getItemEqualStatus(*it))
			{
				(*it)->setHidden(false);
			}
			else
			{
				(*it)->setHidden(true);
			}
			++it;
		}
	};

	work(ui.leftTreeDir);
	work(ui.rightTreeDir);

	showTreeDiffFileItem(ui.leftTreeDir);
	showTreeDiffFileItem(ui.rightTreeDir);

	m_leftFindText.clear();
	m_rightFindText.clear();
}



//严格显示独有的，如果是不同则不显示
void CompareDirs::slot_showUniqueBt(bool)
{
	auto work = [&](RcTreeWidget* treeDir) {
		ITEM_FILE_STATUS s;

		QTreeWidgetItemIterator it(treeDir);
		while (*it) {
			if (RC_DIR == (*it)->type())
			{
				(*it)->setHidden(true);
				(*it)->setExpanded(false);
			}
			else
			{
				s = getItemEqualStatus(*it);
				if ((s == ONLY_ONE_SIDE_FILE) || (s == LOST_SIDE_FILE))
				{
					(*it)->setHidden(false);
				}
				else
				{
					(*it)->setHidden(true);
				}
			}
			++it;
		}
	};

	work(ui.leftTreeDir);
	work(ui.rightTreeDir);


	showTreeUniqueFileItem(ui.leftTreeDir);
	showTreeUniqueFileItem(ui.rightTreeDir);


	m_leftFindText.clear();
	m_rightFindText.clear();
}


//清空目录项目
void CompareDirs::slot_clearBt(bool)
{

	if (m_workStatus == FREE_STATUS)
	{
		ui.leftPath->clear();
		ui.rightPath->clear();

		ui.leftTreeDir->clear();
		ui.rightTreeDir->clear();

		m_leftFileAttris.clear();
		m_rightFileAttris.clear();

		m_leftFileAttrisMap.clear();
		m_rightFileAttrisMap.clear();

		m_leftFileNums = 0;
		m_rightFileNums = 0;

		m_isFirstDirCmp = true;
	}
	else
	{
		ui.statusBar->showMessage(tr("now busy, please try later ..."));
	}
}

void CompareDirs::slot_swapBt(bool)
{

	QString leftPath = ui.leftPath->text();
	QString rightPath = ui.rightPath->text();

	if (leftPath.isEmpty() && rightPath.isEmpty())
	{
		return;
	}

	if (m_leftRightOrder == 0)
	{
		m_leftRightOrder = 1;

		ui.verticalLayout_left->addWidget(ui.rightChildWidget);
		ui.verticalLayout_right->addWidget(ui.leftChildWidget);

	}
	else if (m_leftRightOrder == 1)
	{
		m_leftRightOrder = 0;

		ui.verticalLayout_left->addWidget(ui.leftChildWidget);
		ui.verticalLayout_right->addWidget(ui.rightChildWidget);

	}
}

void CompareDirs::slot_reloadBt()
{
	//左右文件夹均不为空，才进行比较
	if (ui.leftPath->text().isEmpty() || ui.rightPath->text().isEmpty())
	{
		bool isChange = false;

		QString leftPath = formatDirFilePath(ui.leftPath->text(),&isChange);
		if (isChange)
		{
			ui.leftPath->setText(leftPath);
		}

		QString rightPath = formatDirFilePath(ui.rightPath->text(), &isChange);
		if (isChange)
		{
			ui.rightPath->setText(rightPath);
		}

		//只做目录重新加载功能
		if (!leftPath.isEmpty())
		{
			m_leftFileNums = loadDir(RC_LEFT, leftPath);
		}
		if (!rightPath.isEmpty())
		{
			m_rightFileNums = loadDir(RC_RIGHT, rightPath);
		}
	}
	else
	{
	cmpFileTree();
}
}

//增加用户菜单项目
void CompareDirs::contextUserDefineItemMenuEvent(int dire,QMenu* menu, QTreeWidgetItem* curItem)
{
	if (menu != nullptr && curItem != nullptr)
	{

		QString fileName = curItem->text(0);
		QString relativePathName = curItem->data(0, Item_RelativePath).toString();

		QString copyInfo;
		QString coverDiffFile;
		//遍历覆盖不同文件
		QString coverDiffFileWalkDir;
		QString filePath;

		if (dire == 0)
		{
			if (m_leftRightOrder == 0)
			{
				copyInfo = tr("Copy to right (Ctrl mult sel)");
				coverDiffFile = tr("Cover All Diffent File To Right in this dir");
				coverDiffFileWalkDir = tr("Cover Diffent File To Right (Traverse subdirectories)");
			}
			else
			{
                copyInfo = tr("Copy to left (Ctrl mult sel)");
				coverDiffFile = tr("Cover All Diffent File To Left in this dir");
				coverDiffFileWalkDir = tr("Cover Diffent File To Left (Traverse subdirectories)");
			}
			filePath = QString("%1/%2").arg(ui.leftTreeDir->getRootDir()).arg(relativePathName);
		}
		else
		{
			if (m_leftRightOrder == 0)
			{
				copyInfo = tr("Copy to left (Ctrl mult sel)");
				coverDiffFile = tr("Cover All Diffent File To Left in this dir");
				coverDiffFileWalkDir = tr("Cover Diffent File To Left (Traverse subdirectories)");
			}
			else
			{
				copyInfo = tr("Copy to right (Ctrl mult sel)");
				coverDiffFile = tr("Cover All Diffent File To Right in this dir");
				coverDiffFileWalkDir = tr("Cover Diffent File To Right (Traverse subdirectories)");
			}
			filePath = QString("%1/%2").arg(ui.rightTreeDir->getRootDir()).arg(relativePathName);

		}

		QAction* copyAction = menu->addAction(copyInfo, this, [=]() {
			//只有这里是多选模式，其余调用slot_copyFileToOther都是单选模式
			slot_copyFileToOther(dire, relativePathName, curItem, true);
			});

		//标记为相等
		menu->addAction(tr("Mark as equal (Ctrl mult sel)"), this, [=]() {
			slot_MarkAsEqual(dire, relativePathName, curItem);
		});

		QAction* coverDiffAction = menu->addAction(coverDiffFile, this, [=]() {
			slot_coverDiffFilesToOther(dire, relativePathName, curItem);
		});


		QAction* copyOnlyFilesAction = menu->addAction(tr("Copy Unique File To Other Side"), this, [=]() {
			slot_copyUniqueFilesToOther(dire, relativePathName, curItem);
		});

		menu->addSeparator();

		QAction* coverDiffWalkDirAction = menu->addAction(coverDiffFileWalkDir, this, [=]() {
			slot_coverDiffFilesWalkDirToOther(dire, relativePathName, curItem);
		});

		QAction* copyOnlyFilesWalkDirAction = menu->addAction(tr("Copy Unique File To Other Side (Traverse subdirectories)"), this, [=]() {
			slot_copyUniqueFilesWalkDirToOther(dire, relativePathName, curItem);
		});

		menu->addSeparator();

		QAction* delAction = menu->addAction(tr("Delete These File (Ctrl mult sel)"), this, [=]() {
			slot_deleteFile(dire, relativePathName, curItem,true);
		});

		QAction* delAllOnlyFileAction = menu->addAction(tr("Delete Only in This Side"), this, [=]() {
			slot_deleteOnlyFileInThisSide(dire, relativePathName, curItem);
		});

		menu->addSeparator();

		menu->addAction(tr("Cope Path To Clipboard"), this, [=]() {

			QClipboard *clipboard = QGuiApplication::clipboard();
			clipboard->setText(filePath);

		});

		menu->addAction(tr("Select Left File To Cmp"), this, [=]() {
			m_selectLeftCmpFile = (filePath);
			cmpSelectFile();
			});

		menu->addAction(tr("Select Right File To Cmp"), this, [=]() {
			m_selectRightCmpFile = (filePath);
			cmpSelectFile();
			});

		menu->addSeparator();

		if ((curItem->type() != RC_DIR) || fileName.isEmpty())
		{
			coverDiffAction->setEnabled(false);
				coverDiffWalkDirAction->setEnabled(false);
				delAllOnlyFileAction->setEnabled(false);
			copyOnlyFilesAction->setEnabled(false);
				copyOnlyFilesWalkDirAction->setEnabled(false);
		}

		if (fileName.isEmpty() || (curItem->type() == RC_DIR))
		{
			copyAction->setEnabled(false);
			delAction->setEnabled(false);
		}

		menu->addAction(tr("Find File By Name"), this, [=]() {
			slot_findFile(dire);
			});
	}
}

void CompareDirs::cmpSelectFile()
{
	if (!m_selectLeftCmpFile.isEmpty() && !m_selectRightCmpFile.isEmpty())
	{
		CompareWin* newWin = nullptr;
		newWin = new CompareWin(m_selectLeftCmpFile, m_selectRightCmpFile, nullptr);
		newWin->setAttribute(Qt::WA_DeleteOnClose);
		newWin->show();
		newWin->slot_doCmp();

		m_selectLeftCmpFile.clear();
		m_selectRightCmpFile.clear();
	}
}

void CompareDirs::slot_findFile(int src)
{
	m_currentLeftFindIndex = 0;
	m_currentRightFindIndex = 0;

	if (m_leftRightOrder == 1)
	{
		if (src == 0)
		{
			src = 1;
		}
		else
		{
			src = 0;
		}
	}

	if (m_findFileWin == nullptr)
	{
        m_findFileWin = new DirFindFile(src,this);
        m_findFileWin->setWindowFlag(Qt::Window);
		connect(m_findFileWin, &DirFindFile::signFindFile, this, &CompareDirs::slot_FindFileWithPara);
	}
	m_findFileWin->getForce();
	m_findFileWin->activateWindow();
    m_findFileWin->raise();
	m_findFileWin->showNormal();

}

//删除文件
void CompareDirs::slot_deleteFile(int src, QString fileName, QTreeWidgetItem* curItem, bool multiSelete)
{
    auto work = [this](int direction, QLineEdit *srcPath, QString& fileName, QTreeWidgetItem* curItem) {
		QString filePath = QString("%1/%2").arg(srcPath->text()).arg(fileName);
		//删除左边文件
		if (!QFile::exists(filePath))
		{
			if (m_doWithAsk)
			{
				QString notice = tr("%1 not exist, please check!").arg(filePath);
				QMessageBox::warning(this, tr("Notice"), notice);
			}

			return;
		}

		if (QFile::remove(filePath))
		{
			QFont f = curItem->font(0);
			f.setStrikeOut(true);
			curItem->setFont(0, f);
			curItem->setFont(1, f);
			curItem->setFont(2, f);

			setItemEqualStatus(curItem, DELETED_FILE);

			QString relativePathName = curItem->data(0, Item_RelativePath).toString();

			//找到右边文件
			QTreeWidgetItem* item = findItemByRelativePath((direction == RC_LEFT)? RC_RIGHT:RC_LEFT, relativePathName);
			if (item != nullptr)
			{
				ITEM_FILE_STATUS status = getItemEqualStatus(item);
				if (status != DELETED_FILE)
				{
					//只要不是已经删除，则统一设置为独有
					setItemEqualStatus(item, ONLY_ONE_SIDE_FILE);
				}
			}
			ui.statusBar->showMessage(tr("del file %1 success!").arg(filePath));
		}
		else
		{
			ui.statusBar->showMessage(tr("del file %1 failed, maybe other place using !").arg(filePath));
		}
	};

	auto doOne = [=](QString fileName, QTreeWidgetItem* curItem) {

	if (0 == src)
	{
		work(RC_LEFT, ui.leftPath, fileName, curItem);
	}
	else if (1 == src)
	{
		work(RC_RIGHT, ui.rightPath, fileName, curItem);
	}
	};

	if (!multiSelete)
	{
		//询问是否做
		if (m_doWithAsk && QMessageBox::No == QMessageBox::question(this, tr("Notice"), tr("Do you want to delete file %1 ?").arg(fileName)))
		{
			return;
}
		doOne(fileName, curItem);
	}
	else
	{
	QList<QTreeWidgetItem*> selectItems = ((src == 0) ? ui.leftTreeDir->selectedItems() : ui.rightTreeDir->selectedItems());

	//询问是否做
	if (QMessageBox::No == QMessageBox::question(this, tr("Notice"), tr("Do you want to delete these %1 files ?").arg(selectItems.size())))
	{
		return;
	}

	for (int i = 0; i < selectItems.size(); ++i)
	{
		QTreeWidgetItem* item = selectItems.at(i);

		if (item->type() != RC_FILE)
		{
			continue;
		}
		QString fileName = item->data(0, Item_RelativePath).toString();

		doOne(fileName, item);
	}
}
}


//接收参数来查询文件
//dir 左0 右1
//prevOrNext 前0 后1
//区分大小写
void CompareDirs::slot_FindFileWithPara(int dire, int prevOrNext, QString fileName, bool caseSenstive, bool wholeWord)
{

	if (m_leftRightOrder == 1)
	{
		if (dire == 0)
		{
			dire = 1;
		}
		else
		{
			dire = 0;
		}
	}

	auto work = [this](int dire, QString& fileName, int prevOrNext, bool caseSenstive, bool whole) {

		QString &FindText = (((RC_LEFT == dire)/* || (RC_RIGHT == dire && m_leftRightOrder == 1)*/) ? m_leftFindText : m_rightFindText);
		QList<QTreeWidgetItem *> &FindResult = (((RC_LEFT == dire)/* || (RC_RIGHT == dire && m_leftRightOrder == 1)*/) ? m_leftFindResult : m_rightFindResult);
		const QString noticeMsgPrev = (((RC_LEFT == dire && m_leftRightOrder == 0) || (RC_RIGHT == dire && m_leftRightOrder == 1)) ? tr("left Dirs No Find Prev!") : tr("right Dirs No Find Prev!"));
		const QString noticeMsgNext = (((RC_LEFT == dire && m_leftRightOrder == 0) || (RC_RIGHT == dire && m_leftRightOrder == 1)) ? tr("left Dirs No Find Next!") : tr("right Dirs No Find Next!"));
		int & currentFindIndex = (((RC_LEFT == dire)) ? m_currentLeftFindIndex : m_currentRightFindIndex);
		RcTreeWidget* pTreeSrc = (((RC_LEFT == dire)) ? ui.leftTreeDir : ui.rightTreeDir);

		if ((FindText != fileName) || (m_findCaseSenstive != caseSenstive) ||(m_findWholeWord != whole)|| m_curFindDire != dire)
		{
			FindText = fileName;
			m_findCaseSenstive = caseSenstive;
			m_findWholeWord = whole;

			m_curFindDire = (RC_DIRECTION)dire;
			FindResult.clear();

			Qt::MatchFlags flag = Qt::MatchContains | Qt::MatchRecursive;

			if (caseSenstive)
			{
				flag |= Qt::MatchCaseSensitive;
			}

			FindResult = pTreeSrc->findItems(fileName, flag, 0);

			//隐藏的项目不展示
			for (int i = FindResult.size() - 1; i >= 0; --i)
			{
				if (FindResult.at(i)->isHidden())
				{
					FindResult.removeAt(i);
				}

				if (whole && (FindResult.at(i)->text(0) != fileName))
				{
					FindResult.removeAt(i);
			}
			}

			if (FindResult.isEmpty())
			{
				QMessageBox::information(this, tr("Not Find"), tr("can not find %1").arg(fileName));
				return;
			}
			//定位到第0个
			currentFindIndex = 0;

			QTreeWidgetItem* item = FindResult.at(0);
			pTreeSrc->setCurrentItem(item);

			if (RC_LEFT == dire)
			{
				slot_itemLeftClick(item, 0);
			}
			else
			{
				slot_itemRightClick(item, 0);
			}
			return;
		}

		if (prevOrNext == 0)
		{
			//往前
			currentFindIndex--;

			if ((0 <= currentFindIndex) && (currentFindIndex < FindResult.count()))
			{
				QTreeWidgetItem* item = FindResult.at(currentFindIndex);
				pTreeSrc->setCurrentItem(item);
				pTreeSrc->scrollToItem(item);
				if (RC_LEFT == dire)
				{
					slot_itemLeftClick(item, 0);
				}
				else
				{
					slot_itemRightClick(item, 0);
				}
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

				pTreeSrc->setCurrentItem(item);

				pTreeSrc->scrollToItem(item);
				if (RC_LEFT == dire)
				{
					slot_itemLeftClick(item, 0);
				}
				else
				{
					slot_itemRightClick(item, 0);
				}

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
	work(dire, fileName,  prevOrNext,  caseSenstive, wholeWord);
}

//拷贝目录下所有不同文件到对方，只覆盖左右都有，但是不同的文件。单独有的不覆盖
void CompareDirs::slot_coverDiffFilesToOther(int src, QString fileName, QTreeWidgetItem* curItem)
{
	QTreeWidgetItem* childItem = nullptr;

	//询问是否做
	if (QMessageBox::No == QMessageBox::question(this, tr("Notice"), tr("Do you want to overwrite all files (excluding folders) to the other side?")))
	{
		return;
	}

	//批量操作，所有不再多次询问
	m_doWithAsk = false;

	int coverNums = 0;

	for (int i = 0; i < curItem->childCount(); ++i)
	{
		childItem = curItem->child(i);
		QString relativePathName = childItem->data(0, Item_RelativePath).toString();

		//只处理文件，不处理文件夹,而且只处理不相等的文件
		if (!relativePathName.isEmpty() && (childItem->type() == RC_FILE) && (getItemEqualStatus(childItem) == NO_EQUAL_FILE))
		{
			slot_copyFileToOther(src, relativePathName, childItem, false);
			coverNums++;
			ui.statusBar->showMessage(tr("cover file %1 please waiting").arg(relativePathName), 2000);
		}
	}

	m_doWithAsk = true;

	ui.statusBar->showMessage(tr("cover file finish, total cover %1 files").arg(coverNums), 10000);
}

//拷贝目录下所有不同文件到对方，只覆盖左右都有，但是不同的文件。单独有的不覆盖。而且要递归子目录
void CompareDirs::slot_coverDiffFilesWalkDirToOther(int src, QString fileName, QTreeWidgetItem* curItem)
{
	QTreeWidgetItem* childItem = nullptr;

	//询问是否做
	if (QMessageBox::No == QMessageBox::question(this, tr("Notice"), tr("Do you want to overwrite all files (Traverse subdirs) to the other side?")))
	{
		return;
	}

	//批量操作，所有不再多次询问
	m_doWithAsk = false;

	int coverNums = 0;

	QList<QTreeWidgetItem*> items;
	items << curItem;

	while (!items.isEmpty())
	{
		QTreeWidgetItem* pItem = items.takeFirst();

		for (int i = 0; i < pItem->childCount(); ++i)
		{
			childItem = pItem->child(i);
			QString relativePathName = childItem->data(0, Item_RelativePath).toString();

			//如果是目录，加入后下次处理
			if (!relativePathName.isEmpty() && (childItem->type() == RC_DIR))
			{
				items.append(childItem);
			}
			//如果是处理文件，而且只处理不相等的文件
			else if (!relativePathName.isEmpty() && (childItem->type() == RC_FILE) && (getItemEqualStatus(childItem) == NO_EQUAL_FILE))
			{
				slot_copyFileToOther(src, relativePathName, childItem, false);
				coverNums++;
				ui.statusBar->showMessage(tr("cover file %1 please waiting").arg(relativePathName), 2000);
			}
		}
	}


	m_doWithAsk = true;

	ui.statusBar->showMessage(tr("cover file finish, total cover %1 files").arg(coverNums), 10000);
}

//拷贝目录下独有的文件到另外一方
void CompareDirs::slot_copyUniqueFilesToOther(int src, QString fileName, QTreeWidgetItem* curItem)
{
	QTreeWidgetItem* childItem = nullptr;

	//询问是否做
	if (QMessageBox::No == QMessageBox::question(this, tr("Notice"), tr("Do you want to copy unique files (excluding folders) to the other side?")))
	{
		return;
	}

	//批量操作，所有不再多次询问
	m_doWithAsk = false;

	int copyNums = 0;

	for (int i = 0; i < curItem->childCount(); ++i)
	{
		childItem = curItem->child(i);
		QString relativePathName = childItem->data(0, Item_RelativePath).toString();

		//只处理文件，不处理文件夹,而且只处理不相等的文件
		if (!relativePathName.isEmpty() && (childItem->type() == RC_FILE) && (getItemEqualStatus(childItem) == ONLY_ONE_SIDE_FILE))
		{
			slot_copyFileToOther(src, relativePathName, childItem,false);
			copyNums++;
		}
	}

	m_doWithAsk = true;

	ui.statusBar->showMessage(tr("cover file finish, total cover %1 files").arg(copyNums), 10000);
}

//拷贝目录下独有的文件到另外一方
void CompareDirs::slot_copyUniqueFilesWalkDirToOther(int src, QString fileName, QTreeWidgetItem* curItem)
{
	QTreeWidgetItem* childItem = nullptr;

	//询问是否做
	if (QMessageBox::No == QMessageBox::question(this, tr("Notice"), tr("Do you want to copy unique files (Traverse subdirs) to the other side?")))
	{
		return;
	}

	//批量操作，所有不再多次询问
	m_doWithAsk = false;

	int copyNums = 0;

	QList<QTreeWidgetItem*> items;
	items << curItem;

	while (!items.isEmpty())
	{
		QTreeWidgetItem* pItem = items.takeFirst();

		for (int i = 0; i < pItem->childCount(); ++i)
		{
			childItem = pItem->child(i);
			QString relativePathName = childItem->data(0, Item_RelativePath).toString();

			//如果是目录，加入后下次处理
			if (!relativePathName.isEmpty() && (childItem->type() == RC_DIR))
			{
				items.append(childItem);
			}
			//只处理文件，不处理文件夹,而且只处理不相等的文件
			else if (!relativePathName.isEmpty() && (childItem->type() == RC_FILE) && (getItemEqualStatus(childItem) == ONLY_ONE_SIDE_FILE))
			{
				slot_copyFileToOther(src, relativePathName, childItem, false);
				copyNums++;
			}
		}
	}
	m_doWithAsk = true;

	ui.statusBar->showMessage(tr("cover file finish, total cover %1 files").arg(copyNums), 10000);
}


//删除此边独有的文件
void  CompareDirs::slot_deleteOnlyFileInThisSide(int src, QString fileName, QTreeWidgetItem* curItem)
{
	QTreeWidgetItem* childItem = nullptr;

	//询问是否做
	if (QMessageBox::No == QMessageBox::question(this, tr("Notice"), tr("Do you want to delete all files (excluding folders) only in this side?")))
	{
		return;
	}

	//批量操作，所有不再多次询问
	m_doWithAsk = false;

	int delNums = 0;

	for (int i = 0; i < curItem->childCount(); ++i)
	{
		childItem = curItem->child(i);
		QString relativePathName = childItem->data(0, Item_RelativePath).toString();

		//只处理文件，不处理文件夹,而且只处理不相等的文件
		if (!relativePathName.isEmpty() && (childItem->type() == RC_FILE) && (getItemEqualStatus(childItem) == ONLY_ONE_SIDE_FILE))
		{
			slot_deleteFile(src, relativePathName, childItem, false);

			delNums++;
		}
	}

	m_doWithAsk = true;

	ui.statusBar->showMessage(tr("delete file finish, total del %1 files").arg(delNums), 5000);
}

//标记颜色为相等
void CompareDirs::slot_MarkAsEqual(int dir, QString fileName, QTreeWidgetItem* curItem)
{
#if 0
	//0相等 1 不等 2 独有
	setItemForeground(curItem, EQUAL_COLOR);
	setItemEqualStatus(curItem, EQUAL_FILE);

	int direction = ((dir == 0) ? 1 : 0);

	QTreeWidgetItem* item = findItemByRelativePath(direction, fileName);
	if (item != nullptr)
	{
		setItemForeground(item, EQUAL_COLOR);
		setItemEqualStatus(item, EQUAL_FILE);
	}
#endif
	int direction = ((dir == 0) ? 1 : 0);

	QList<QTreeWidgetItem*> selectItems = ((dir == 0) ? ui.leftTreeDir->selectedItems() : ui.rightTreeDir->selectedItems());

	for (int i = 0; i < selectItems.size(); ++i)
	{
		QTreeWidgetItem* item = selectItems.at(i);

		if (item->type() != RC_FILE)
		{
			continue;
}
		QString fileName = item->data(0, Item_RelativePath).toString();

		//0相等 1 不等 2 独有
		setItemForeground(item, EQUAL_COLOR);
		setItemEqualStatus(item, EQUAL_FILE);

		QTreeWidgetItem* otherItem = findItemByRelativePath(direction, fileName);
		if (otherItem != nullptr)
{
			setItemForeground(otherItem, EQUAL_COLOR);
			setItemEqualStatus(otherItem, EQUAL_FILE);
		}
	}

}

//multiSelete:是否多选模式。默认false，非多选模式，直接处理curItem。多选模式：需要遍历选中项目
void CompareDirs::slot_copyFileToOther(int src, QString fileName, QTreeWidgetItem* curItem, bool multiSelete)
{
	auto getDirNameByRelativePath = [](QString &replativePath) {
		QString dirName;
		int pos = replativePath.lastIndexOf('/');
		if (-1 != pos)
		{
			dirName = replativePath.mid(pos + 1);
		}
		else
		{
			//如果只有一级目录，则其父节点就是根节点,访问根节点
			dirName = replativePath;
		}

		return dirName;
	};

	auto showParentNodeName = [&](QTreeWidgetItem* item) {

		if (item != nullptr)
		{
			QTreeWidgetItem* parentItem = item->parent();

			while ((parentItem != nullptr) && parentItem->text(0).isEmpty())
			{
				QString relativePathName = parentItem->data(0, Item_RelativePath).toString();

				parentItem->setText(0, getDirNameByRelativePath(relativePathName));
				parentItem->setIcon(0, QIcon(":/Resources/img/dir.png"));

				parentItem = parentItem->parent();
			}
		}
	};

	auto setItemEqual = [this](int dir, QString& fileName) {
		QTreeWidgetItem* item = findItemByRelativePath(dir, fileName);
		if (item != nullptr)
		{
			setItemForeground(item, EQUAL_COLOR);
			setItemEqualStatus(item, EQUAL_FILE);

			QFont f = item->font(0);
			if (f.strikeOut())
			{
				f.setStrikeOut(false);
				item->setFont(0, f);
				item->setFont(1, f);
				item->setFont(2, f);
			}

		}
	};

	//修改1 2 项对应的文件大小和时间。用于拷贝后，直接把目标项的文件大小和时间，设置为源方向。
	auto setItemSizeAndTime = [this](QTreeWidgetItem* srcItem, int dir, QString& fileName) {
		QTreeWidgetItem* item = findItemByRelativePath(dir, fileName);
		if (item != nullptr)
		{
			item->setText(1, srcItem->text(1));
			item->setText(2, srcItem->text(2));
		}
	};



	auto work = [this, setItemEqual, setItemSizeAndTime, showParentNodeName](int dire, QTreeWidgetItem* curItem, QString &srcPath, QString &otherPath, QString& fileName) {
		if (!QFile::exists(srcPath))
		{
			ui.statusBar->showMessage(tr("%1 not exist, skip ...").arg(srcPath), 5000);
			return;
		}

		int otherDire = ((dire == RC_LEFT) ? RC_RIGHT : RC_LEFT);

		//左边拷贝到右边
		if (QFile::exists(otherPath))
		{
			QString notice = tr("%1 is exist, if replace ?").arg(otherPath);

			if (m_doWithAsk && (QMessageBox::No == QMessageBox::question(this, tr("Replace ?"), notice)))
			{
				return;
			}
			QFile::remove(otherPath);
			QFile::copy(srcPath, otherPath);

			setItemForeground(curItem, EQUAL_COLOR);
			setItemEqualStatus(curItem, EQUAL_FILE);

			setItemEqual(otherDire, fileName);
			setItemSizeAndTime(curItem, otherDire, fileName);
		}
		else
		{
			//检查目录是否存在
			QFileInfo FileInfo(otherPath);
			QDir dir = FileInfo.absoluteDir();
			QString relativePath = getRelativePath(otherDire, otherPath);

			if (!dir.exists())
			{
				dir.mkpath(dir.absolutePath());
			}

			if (QFile::copy(srcPath, otherPath))
			{
				//拷贝成功，则需要新增节点，把节点同步显示在树上
				//找其父节点
				QTreeWidgetItem* item = findItemByRelativePath(otherDire, relativePath);
				if (item != nullptr)
				{
					item->setText(0, FileInfo.fileName());
					item->setIcon(0, QIcon(":/Resources/img/point.png"));
					showParentNodeName(item);

					QFont f = item->font(0);
					if (f.strikeOut())
					{
						f.setStrikeOut(false);
						item->setFont(0, f);
						item->setFont(1, f);
						item->setFont(2, f);
					}

					setItemForeground(item, EQUAL_COLOR);
					setItemEqualStatus(item, EQUAL_FILE);
				}

				setItemForeground(curItem, EQUAL_COLOR);
				setItemEqualStatus(curItem, EQUAL_FILE);

			}
			else
			{
				ui.statusBar->showMessage(tr("copy file %1 failed, please check file auth !").arg(srcPath), 5000);
			}
		}
	};

	auto doOne = [=](int src, QTreeWidgetItem* curItem, QString& leftPath, QString& rightPath, QString& fileName)
	{
	if (0 == src)
	{
		work(src, curItem, leftPath, rightPath, fileName);
	}
	else
	{
		work(src, curItem, rightPath, leftPath, fileName);
	}
	};


	int copyfile = 0;
	int skipfile = 0;

	if (!multiSelete)
	{
		//是单选
		QString leftPath = QString("%1/%2").arg(ui.leftPath->text()).arg(fileName);
		QString rightPath = QString("%1/%2").arg(ui.rightPath->text()).arg(fileName);
		++copyfile;
		doOne(src, curItem, leftPath, rightPath,fileName);
		}
		else
		{
		//是多选
		QList<QTreeWidgetItem*> selectItems = ((src == 0) ? ui.leftTreeDir->selectedItems() : ui.rightTreeDir->selectedItems());

		int dealFileNums = selectItems.size();

		m_doWithAsk = false;

		//总的询问一次
		if (QMessageBox::No == QMessageBox::question(this, tr("Copy Or Replace ALL"),tr("Copy Or Replace %1 item files ?").arg(dealFileNums)))
			{
			return;
		}

		//是多选，遍历执行
		for (int i = 0; i < selectItems.size(); ++i)
		{
			QTreeWidgetItem* item = selectItems.at(i);

			if (item->type() != RC_FILE)
			{
				++skipfile;
				continue;
			}
			++copyfile;
			QString fileName = item->data(0, Item_RelativePath).toString();

			QString leftPath = QString("%1/%2").arg(ui.leftPath->text()).arg(fileName);
			QString rightPath = QString("%1/%2").arg(ui.rightPath->text()).arg(fileName);

			doOne(src, item, leftPath, rightPath, fileName);

			//这里很卡，加个UI响应。
			if (0 == src)
			{
				ui.statusBar->showMessage(tr("copy file %1 to %2 .").arg(leftPath).arg(rightPath));
			}
			else
			{
				ui.statusBar->showMessage(tr("copy file %1 to %2 .").arg(rightPath).arg(leftPath));
			}
			QCoreApplication::processEvents();
		}

		m_doWithAsk = true;
			}
	ui.statusBar->showMessage(tr("copy %1 files, skip %2 files").arg(copyfile).arg(skipfile));
					}

void CompareDirs::slot_itemLeftClick(QTreeWidgetItem *item, int )
{
	QString filePath = item->data(0, Item_RelativePath).toString();

	if (m_rightFileAttrisMap.contains(filePath))
	{
		ui.rightTreeDir->setCurrentItem(m_rightFileAttrisMap.value(filePath).selfItem);
	}

	ui.statusBar->showMessage(tr("current file: %1").arg(filePath));
}

void CompareDirs::slot_itemRightClick(QTreeWidgetItem *item, int )
{
	QString filePath = item->data(0, Item_RelativePath).toString();

	if (m_leftFileAttrisMap.contains(filePath))
	{
		ui.leftTreeDir->setCurrentItem(m_leftFileAttrisMap.value(filePath).selfItem);
	}
	ui.statusBar->showMessage(tr("current file: %1").arg(filePath));
}

//双击对比文件
void CompareDirs::slot_itemLeftDoubleClick(QTreeWidgetItem *item, int )
{
	//只有文件才执行，文件夹直接返回
	if (item->type() == RC_DIR)
	{
		return;
	}

	QString leftPath = item->text(0);

	QString rightPath;

	QString filePath = item->data(0, Item_RelativePath).toString();

	if (m_leftFileAttrisMap.contains(filePath))
	{
		//如果是文件夹，则不做事情，返回
		if (m_leftFileAttrisMap.value(filePath).type == RC_DIR)
		{
			return;
		}
	}

	if (m_rightFileAttrisMap.contains(filePath))
	{
		if (m_rightFileAttrisMap.value(filePath).type == RC_DIR)
		{
			return;
		}
		rightPath = m_rightFileAttrisMap.value(filePath).selfItem->text(0);
	}

    int isCmpHex = 0;

	if (!leftPath.isEmpty())
	{
		leftPath = QString("%1/%2").arg(ui.leftTreeDir->getRootDir()).arg(filePath);

		if (leftPath.endsWith(".exe") || leftPath.endsWith(".dll") || leftPath.endsWith(".db") || leftPath.endsWith(".obj"))
		{
            isCmpHex = 1;
		}
		else if (!DocTypeListView::isSupportExt(fileSuffix(leftPath)))
		{
			int ret = QMessageBox::question(this, "Notice", tr("file [%1] may be not a text file, cmp in hex mode?").arg(leftPath), tr("Yes[hex cmp]"), tr("No[text cmp]"), tr("Cancel"));
			if (0 == ret)
			{
                isCmpHex = 2;
			}
            else if(1 == ret)
            {
                isCmpHex = 3;
		}
			else
			{
				return; //cancel
	}
	}
	}

	if (!rightPath.isEmpty())
	{
		rightPath = QString("%1/%2").arg(ui.rightTreeDir->getRootDir()).arg(filePath);

		if (rightPath.endsWith(".exe") || rightPath.endsWith(".dll") || rightPath.endsWith(".db") || rightPath.endsWith(".obj"))
		{
            isCmpHex = 1;
		}
		else if (!DocTypeListView::isSupportExt(fileSuffix(rightPath)))
		{
            if (0 == isCmpHex)
			{
				int ret = QMessageBox::question(this, "Notice", tr("file [%1] may be not a text file, cmp in hex mode?").arg(rightPath), tr("Yes[hex cmp]"), tr("No[text cmp]"), tr("Cancel"));
				if (0 == ret)
				{
                    isCmpHex = 2;
				}
				else if (1 == ret)
                {
                    isCmpHex = 3;
			}
				else
				{
					return; //cancel
		}
	}
	}
	}

    if ((0 == isCmpHex) || (3 == isCmpHex))
	{
		CompareWin* newWin = nullptr;

		if (m_leftRightOrder == 0)
		{
			newWin = new CompareWin(leftPath, rightPath, nullptr);
		}
		else
		{
			newWin = new CompareWin(rightPath, leftPath, nullptr);
		}

		connect(newWin, &CompareWin::sendCmpResultAtClose, this, &CompareDirs::slot_cmpResultStatusChange);

		newWin->setAttribute(Qt::WA_DeleteOnClose);
		newWin->show();
		newWin->slot_doCmp();
	}
	else
	{
		CompareHexWin* newWin = nullptr;

		if (m_leftRightOrder == 0)
		{
			newWin = new CompareHexWin(leftPath, rightPath, nullptr);
		}
		else
		{
			newWin = new CompareHexWin(rightPath, leftPath, nullptr);
		}

		newWin->setAttribute(Qt::WA_DeleteOnClose);
		newWin->show();
		newWin->slot_doCmp();
	}
}

QString CompareDirs::fileSuffix(const QString& filePath)
{
	QFileInfo fi(filePath);
	return fi.suffix();
}

void CompareDirs::setCmpFile(QString leftFile, QString rightFile)
{
	//这里不加载文件了，强行设置为非第一次加载，这样cmpFileTree里面本来就会重新加载左右文件夹
	m_isFirstDirCmp = false;

	ui.leftPath->setText(formatDirFilePath(leftFile));
	ui.rightPath->setText(formatDirFilePath(rightFile));

	cmpFileTree();
}

//文件比较关闭后，会释放最终比较结果是否相等，如果相等，则修改文字的前景色
void CompareDirs::slot_cmpResultStatusChange(int status, QString leftFilePath, QString rightFilePath)
{
	//相等
	QString leftRelativePath;
	QString rightRelativePath;

	if (m_leftRightOrder == 0)
	{
		leftRelativePath = getRelativePath(RC_LEFT, leftFilePath);
		rightRelativePath = getRelativePath(RC_RIGHT, rightFilePath);
	}
	else
	{
		leftRelativePath = getRelativePath(RC_RIGHT, leftFilePath);
		rightRelativePath = getRelativePath(RC_LEFT, rightFilePath);
	}

	if (status == RC_CMP_RESULT::RC_RESULT_EQUAL)
	{
		QTreeWidgetItem* leftItem = findItemByRelativePath(RC_LEFT, leftRelativePath);
		if (leftItem != nullptr)
		{
			setItemForeground(leftItem, EQUAL_COLOR);
			setItemEqualStatus(leftItem, EQUAL_FILE);
		}

		QTreeWidgetItem* rightItem = findItemByRelativePath(RC_RIGHT, rightRelativePath);
		if (rightItem != nullptr)
		{
			setItemForeground(rightItem, EQUAL_COLOR);
			setItemEqualStatus(rightItem, EQUAL_FILE);
		}
	}
	else
	{
		QTreeWidgetItem* leftItem = findItemByRelativePath(RC_LEFT, leftRelativePath);
		if (leftItem != nullptr)
		{
			setItemForeground(leftItem, UN_EQUAL_COLOR);
			setItemEqualStatus(leftItem, NO_EQUAL_FILE);
		}

		QTreeWidgetItem* rightItem = findItemByRelativePath(RC_RIGHT, rightRelativePath);
		if (rightItem != nullptr)
		{
			setItemForeground(rightItem, UN_EQUAL_COLOR);
			setItemEqualStatus(rightItem, NO_EQUAL_FILE);
		}
	}
}

void CompareDirs::slot_itemRightDoubleClick(QTreeWidgetItem *item, int )
{
	//只有文件才执行，文件夹直接返回
	if (item->type() == RC_DIR)
	{
		return;
	}

	QString rightPath = item->text(0);

	QString leftPath;

	QString filePath = item->data(0, Item_RelativePath).toString();

	if (m_rightFileAttrisMap.contains(filePath))
	{
		//如果是文件夹，则不做事情，返回
		if (m_rightFileAttrisMap.value(filePath).type == RC_DIR)
		{
			return;
		}
	}

	if (m_leftFileAttrisMap.contains(filePath))
	{
		if (m_leftFileAttrisMap.value(filePath).type == RC_DIR)
		{
			return;
		}
		leftPath = m_leftFileAttrisMap.value(filePath).selfItem->text(0);
	}

    int isCmpHex = 0;

	if (!leftPath.isEmpty())
	{
		leftPath = QString("%1/%2").arg(ui.leftTreeDir->getRootDir()).arg(filePath);

		if (leftPath.endsWith(".exe") || leftPath.endsWith(".dll") || leftPath.endsWith(".db") || leftPath.endsWith(".obj"))
		{
            isCmpHex = 1;
		}
		else if (!DocTypeListView::isSupportExt(fileSuffix(leftPath)))
		{

			int ret = QMessageBox::question(this, "Notice", tr("file [%1] may be not a text file, cmp in hex mode?").arg(leftPath), tr("Yes[hex cmp]"), tr("No[text cmp]"), tr("Cancel"));
			if (0 == ret)
			{
                isCmpHex = 2;
			}
			else if (1 == ret)
			{
				isCmpHex = 3;
			}
            else
            {
				return; //cancel
		}
	}
	}

	if (!rightPath.isEmpty())
	{
		rightPath = QString("%1/%2").arg(ui.rightTreeDir->getRootDir()).arg(filePath);

		if (rightPath.endsWith(".exe") || rightPath.endsWith(".dll") || rightPath.endsWith(".db") || rightPath.endsWith(".obj"))
		{
            isCmpHex = 1;
		}
		else if (!DocTypeListView::isSupportExt(fileSuffix(rightPath)))
		{
            if (0 == isCmpHex)
			{
				int ret = QMessageBox::question(this, "Notice", tr("file [%1] may be not a text file, cmp in hex mode?").arg(rightPath), tr("Yes[hex cmp]"), tr("No[text cmp]"), tr("Cancel"));
				if (0 == ret)
				{
                    isCmpHex = 2;
				}
				else if (1 == ret)
                {
                    isCmpHex = 3;
			}
				else
				{
					return; //cancel
		}
	}
	}

	}
    if ((0 == isCmpHex) || (3 == isCmpHex))
	{
		CompareWin* newWin = nullptr;

		if (m_leftRightOrder == 0)
		{
			newWin = new CompareWin(leftPath, rightPath, nullptr);
		}
		else
		{
			newWin = new CompareWin(rightPath, leftPath, nullptr);
		}

		connect(newWin, &CompareWin::sendCmpResultAtClose, this, &CompareDirs::slot_cmpResultStatusChange);

		newWin->setAttribute(Qt::WA_DeleteOnClose);
		newWin->show();
		newWin->slot_doCmp();
	}
	else
	{
		CompareHexWin* newWin = nullptr;

		if (m_leftRightOrder == 0)
		{
			newWin = new CompareHexWin(leftPath, rightPath, nullptr);
		}
		else
		{
			newWin = new CompareHexWin(rightPath, leftPath, nullptr);
		}

		newWin->setAttribute(Qt::WA_DeleteOnClose);
		newWin->show();
		newWin->slot_doCmp();
	}
}

//中介者发过来的同步对方滚动条当前值信息
void CompareDirs::slot_syncCurScrollValue(int direction)
{
	if (RC_LEFT == direction)
	{
		ui.rightTreeDir->setVerticalValue(m_mediator->getLeftScrollValue());
		//一旦对方同步后，则更新对方当前的值
		m_mediator->setRightScrollValue(m_mediator->getLeftScrollValue());
	}
	else
	{
		ui.leftTreeDir->setVerticalValue(m_mediator->getRightScrollValue());
		m_mediator->setLeftScrollValue(m_mediator->getRightScrollValue());
	}
}

//中介者发过来的设置子项展开与缩放的消息
void CompareDirs::slot_syncExpandStatus(QString name, int direction, int status)
{
	bool isExpand = (status == RC_EXPANDED) ? true : false;

	//左边的ITEM需要设置
	if (RC_LEFT == direction)
	{
		if (m_leftFileAttrisMap.contains(name))
		{
			fileAttriNode fn = m_leftFileAttrisMap.value(name);

			//如果是文件夹Item,而且当前状态与需要设置的状态 相反
			if (fn.type == RC_DIR && (fn.selfItem->isExpanded() != isExpand))
			{
				fn.selfItem->setExpanded(isExpand);
			}
		}
	}
	else
	{
		//右边的ITEM需要设置
		if (m_rightFileAttrisMap.contains(name))
		{
			fileAttriNode fn = m_rightFileAttrisMap.value(name);

			//如果是文件夹Item,而且当前状态与需要设置的状态 相反
			if (fn.type == RC_DIR && (fn.selfItem->isExpanded() != isExpand))
			{
				fn.selfItem->setExpanded(isExpand);
			}
		}
	}
}

void CompareDirs::slot_openLeftDir()
{
	//加载左边的文件树
	QString workDir = ui.leftPath->text();
	QString rootpath = QFileDialog::getExistingDirectory(this, tr("Open Directory"), workDir, QFileDialog::DontResolveSymlinks);

	if (!rootpath.isEmpty())
	{
		ui.leftPath->setText(rootpath);

		if (!ui.leftPath->text().isEmpty() && !ui.rightPath->text().isEmpty())
		{
			setCmpFile(ui.leftPath->text(), ui.rightPath->text());
		}
	}

}

void CompareDirs::createWorkThread()
{
		//保证只执行一次
	if (m_workThread == nullptr)
	{
		m_workThread = new QThread(this);
		m_workThread->start();

		//这里不能给parent，否则会导致父子对象不在同一个线程的错误
		m_cmpDataThread = new CompareDirsData(nullptr);
		m_cmpDataThread->moveToThread(m_workThread);

		//QObject::connect(pNetReg, &NetRegister::showMessage, pStatusBar, &QStatusBar::showMessage);

		//下面槽函数会在workThread删除的时候执行
		//启动一个工作线程，后面加载目录，在工作线程中执行，分担主界面的工作，避免卡顿。
		//m_cmpDataThread 不需要手动释放，在线程退出时，会自动释放该对象。

		QObject::connect(m_workThread, &QThread::finished, m_cmpDataThread, [&]() {
			m_cmpDataThread->deleteLater();
			m_cmpDataThread = nullptr;
			});

		connect(this, &CompareDirs::s_walkFile, m_cmpDataThread, &CompareDirsData::on_walkFile, Qt::QueuedConnection);
		connect(m_cmpDataThread, &CompareDirsData::outMsg, this, &CompareDirs::on_childThreadMsg, Qt::QueuedConnection);
		connect(m_cmpDataThread, &CompareDirsData::addNode, this, &CompareDirs::on_addNode, Qt::QueuedConnection);
	}

}

void CompareDirs::slot_openRightDir()
{
	//加载左边的文件树
	QString workDir = ui.rightPath->text();
	QString rootpath = QFileDialog::getExistingDirectory(this, tr("Open Directory"), workDir, QFileDialog::DontResolveSymlinks);

	if (!rootpath.isEmpty())
	{
#if 0
		m_rightFileNums = loadDir(RC_RIGHT, rootpath);
		if (m_leftFileNums > 0)
		{
			cmpFileTree();
		}
#endif
		//m_selectRightCmpDirPath = rootpath;

		ui.rightPath->setText(rootpath);

		if (!ui.leftPath->text().isEmpty() && !ui.rightPath->text().isEmpty())
		{
			setCmpFile(ui.leftPath->text(), ui.rightPath->text());
	}
}
}


//对比文件树，将左右文件树进行对比补齐
void CompareDirs::cmpFileTree()
{
	if (m_loadDirCancel)
	{
		return;
	}
	if (m_workStatus == FREE_STATUS)
	{
		m_workStatus = CMP_WORKING;

		m_isCompareCancel = false;
		m_dataMode->setCompareCancel(false);
	}
	else if (m_workStatus != FREE_STATUS)
	{
		//已经在比较中，不能重入
		ui.statusBar->showMessage(tr("Comparison in progress, please wait ..."));
		return;
	}

	//左右文件夹均不为空，才进行比较
	if (!ui.leftPath->text().isEmpty() && !ui.rightPath->text().isEmpty())
	{
		bool isChange = false;

		QString leftPath = formatDirFilePath(ui.leftPath->text(), &isChange);
		if (isChange)
		{
			ui.leftPath->setText(leftPath);
		}

		QString rightPath = formatDirFilePath(ui.rightPath->text(), &isChange);
		if (isChange)
		{
			ui.rightPath->setText(rightPath);
		}

		emit receneDirPath(ui.leftPath->text(), ui.rightPath->text());

		//每次都重新加载左右文件。因为用户可能多次选择修改一遍的数据，避免残留文件树等问题
		//如果两边都已经与数据，说明之前已经比较过了。是二次选择文件夹对比，此时需要重新加载
		if (!m_isFirstDirCmp)
		{
			m_leftFileNums = loadDir(RC_LEFT, ui.leftPath->text());

			//
			if (m_loadDirCancel)
			{
				m_isCompareCancel = true;
			}
			m_rightFileNums = loadDir(RC_RIGHT, ui.rightPath->text());

			if (m_loadDirCancel)
			{
				m_isCompareCancel = true;
		}
		}
		//用户在加载目录是就取消了。
		if (m_isCompareCancel)
		{
			m_workStatus = FREE_STATUS;
			return;
		}

		if (m_isFirstDirCmp)
		{
			m_isFirstDirCmp = false;
		}

		ui.statusBar->showMessage(tr("Comparison in progress, please wait ..."));

		if (m_leftFileNums > FILE_NUM_PROCESS_LIMIT || m_rightFileNums > FILE_NUM_PROCESS_LIMIT)
		{
			if (m_loadFileProcessWin == nullptr)
			{
				m_loadFileProcessWin = new ProgressWin(this);
			}

			m_loadFileProcessWin->setWindowModality(Qt::WindowModal);

			m_loadFileProcessWin->info(tr("init dir file tree in progress\ntotal %1 file, please wait ...").arg(m_leftFileAttris.size() + m_rightFileAttris.size()));

			m_loadFileProcessWin->setTotalSteps(2*(m_leftFileAttris.size() + m_rightFileAttris.size()));

			m_loadFileProcessWin->show();
		}

		//以下每一步都算一个进度
		createFileAttriMapFromList();

		if ((m_loadFileProcessWin != nullptr) && m_loadFileProcessWin->isCancel())
		{
			m_isCompareCancel = true;
		}

		displayCmpFileList();

		if ((m_loadFileProcessWin != nullptr) && m_loadFileProcessWin->isCancel())
		{
			m_isCompareCancel = true;
		}

		//一切加载成功后，进行文件的初步对比工作，看文件是否相等

		m_commitCmpFileNums = 0;
		m_finishCmpFileNums = 0;

		m_unFinishedCmpFileMap.clear();

		for (QMap<QString, fileAttriNode>::iterator iter = m_leftFileAttrisMap.begin(); iter != m_leftFileAttrisMap.end(); ++iter)
		{
			if (m_isCompareCancel)
			{
				break;
			}

			//左右都存在该文件
			if (iter->type == RC_FILE && m_rightFileAttrisMap.contains(iter.key()) && m_rightFileAttrisMap.value(iter.key()).type == RC_FILE)
			{
				++m_commitCmpFileNums;

				m_unFinishedCmpFileMap.insert(m_commitCmpFileNums, iter.key());

				QString leftPath = QString("%1/%2").arg(ui.leftPath->text()).arg(iter.key());
				QString rightPath = QString("%1/%2").arg(ui.rightPath->text()).arg(iter.key());

				QFutureWatcher<ThreadParameter*> *futureWatcher = new QFutureWatcher<ThreadParameter*>();

				QObject::connect(futureWatcher, &QFutureWatcher<ThreadParameter>::finished, this, &CompareDirs::slot_fileCompareFinish);

				futureWatcher->setFuture(m_dataMode->compareFile(leftPath, rightPath, s_compareMode, m_commitCmpFileNums));

			}
		}

		if ((m_loadFileProcessWin != nullptr) && (m_commitCmpFileNums >0))
		{
			m_loadFileProcessWin->setTotalSteps(m_commitCmpFileNums);
			m_loadFileProcessWin->info(tr("Comparison in progress, please wait ..."));

			if (s_compareMode == 1)
			{
				m_loadFileProcessWin->info(tr("current exec rule mode is quick mode, please wait ..."));
			}
			else
			{
				m_loadFileProcessWin->info(tr("current exec rule mode is deep slow mode, please wait ..."));
			}

			int lastIndex = 0;

			QTime tm;
			tm.start();

			while (m_finishCmpFileNums < m_commitCmpFileNums)
			{
				if (lastIndex != m_finishCmpFileNums)
				{
					lastIndex = m_finishCmpFileNums;
					m_loadFileProcessWin->setStep(m_finishCmpFileNums);
				}
				QCoreApplication::processEvents();

				if (m_loadFileProcessWin->isCancel())
				{
					setWorkStatusInfo(tr("compare canceled ..."));
					m_loadFileProcessWin->info(tr("compare canceled ..."));
					m_isCompareCancel = true;
					m_dataMode->setCompareCancel(true);
					break;
				}
				int remainder = m_commitCmpFileNums - m_finishCmpFileNums;

				if (remainder <= 5)
				{
					//每分钟打印一次
					if (tm.elapsed() > 60 * 1000)
					{
						tm.restart();

						m_loadFileProcessWin->info(tr("There are still %1 files haven't returned comparison results").arg(remainder));

						for (QMap<int, QString>::iterator it = m_unFinishedCmpFileMap.begin(); it != m_unFinishedCmpFileMap.end(); ++it)
						{
							m_loadFileProcessWin->info(tr("file is %1 in comparing !").arg(*it));
			}
		}
				}
			}
		}

		if (m_isCompareCancel)
		{
			//还是等待取消完成
			while (m_finishCmpFileNums < m_commitCmpFileNums)
			{
				QCoreApplication::processEvents();
			}
		}
		if (m_loadFileProcessWin != nullptr)
		{
			delete m_loadFileProcessWin;
			m_loadFileProcessWin = nullptr;
		}

	if (m_isCompareCancel)
	{
		setWorkStatusInfo(tr("compare not finished, user canceled ..."));
		ui.statusBar->showMessage(tr("user canceled finished ..."));
	}
    else
    {
		//因为我们做了禁止自动排序，所有在对比完成后，不是默认按照字母顺序排序的，这里手动调用一下排序
		slot_onLeftSort(0, Qt::DescendingOrder);
        ui.statusBar->showMessage(tr("compare file finish ..."));
		this->showMaximized();
    }
	}

	m_isCompareCancel = false;
	m_workStatus = FREE_STATUS;
}

//取消进度条显示
void CompareDirs::slot_quitProgress()
{
	if (m_loadFileProcessWin != nullptr)
	{
		delete m_loadFileProcessWin;
		m_loadFileProcessWin = nullptr;
	}
}


//文件对比完毕，显示出文件是否一样，不一样则红色字符标识
//该函数里面不能调用QCoreApplication::processEvents 否则栈溢出
void CompareDirs::slot_fileCompareFinish()
{
	QFutureWatcher<ThreadParameter*> * s = dynamic_cast<QFutureWatcher<ThreadParameter*> *>(sender());

	ThreadParameter* result = s->result();

	int fileIndex = -1;

	//这里释放的内容，其实是在mode里面new出来的
	if (result != nullptr)
	{
		fileIndex = result->fileIndex;

		//如果已经取消了，啥也不干，加快速度
		if (m_isCompareCancel)
		{

		}
		//如果比较结果不等，则设置为红色字体
		else if (!result->isEqual)
		{
			QString leftPath = filePathToRelativePath(result->leftPath, RC_LEFT);
			QString rightPath = filePathToRelativePath(result->rightPath, RC_RIGHT);

			if (m_leftFileAttrisMap.contains(leftPath))
			{
				QTreeWidgetItem *item = m_leftFileAttrisMap.value(leftPath).selfItem;
				setItemForeground(item, UN_EQUAL_COLOR);
				setItemEqualStatus(item, NO_EQUAL_FILE);

				setAllParentDirForegroundNoEqual(item, UN_EQUAL_COLOR);

			}
			if (m_rightFileAttrisMap.contains(rightPath))
			{
				QTreeWidgetItem *item = m_rightFileAttrisMap.value(rightPath).selfItem;
				setItemForeground(item, UN_EQUAL_COLOR);
				setItemEqualStatus(item, NO_EQUAL_FILE);

				setAllParentDirForegroundNoEqual(item, UN_EQUAL_COLOR);

			}
		}

		delete result;
		result = nullptr;
	}

	delete s;
	s = nullptr;

	++m_finishCmpFileNums;

	if (m_finishCmpFileNums >= m_commitCmpFileNums)
	{

		if (m_isCompareCancel)
		{
			setWorkStatusInfo(tr("user canceled finished ..."));
		}
		else
		{
			setWorkStatusInfo(tr("compare file finish ..."));
			ui.statusBar->showMessage(tr("compare file finish ..."));
		}
		m_dataMode->setCompareCancel(false);
	}

	if (fileIndex != -1)
	{
		m_unFinishedCmpFileMap.remove(fileIndex);
}
}

int CompareDirs::loadDir(int type, QString rootDirPath)
{
	if (m_loadDirCancel)
	{
		return 0;
	}

	QString rootpath = rootDirPath; //QFileDialog::getExistingDirectory(this, tr("Open Directory"), QString(), QFileDialog::DontResolveSymlinks);

	QTreeWidgetItem* root = nullptr;

	int fileNums = 0;

	ui.statusBar->showMessage(tr("load dir files, please wait ..."));

	ui.statusBar->repaint();

	if (RC_LEFT == type)
	{
		ui.leftPath->setText(rootpath);

		ui.leftTreeDir->setRootDir(rootpath);
		ui.leftTreeDir->setColumnWidth(0, 400);
		ui.leftTreeDir->clear();

		root = new QTreeWidgetItem(ui.leftTreeDir,RC_DIR);
		root->setText(0, rootpath);
		root->setExpanded(true);

		m_leftFileAttris.clear();
		m_leftFileAttrisMap.clear();

		//第一个节点是目录根节点
		fileAttriNode node;
		node.type = RC_DIR;//是目录
		node.selfItem = root;
		node.parent = nullptr;
		node.relativePath = ".";
		appendAttriNode(node, type);

		m_curRootDirNode = root;

		fileNums = walkLoadFile(rootpath,type);
	}
	else
	{
		ui.rightPath->setText(rootpath);
		ui.rightTreeDir->setRootDir(rootpath);
		ui.rightTreeDir->setColumnWidth(0, 400);
		ui.rightTreeDir->clear();
		root = new QTreeWidgetItem(ui.rightTreeDir,RC_DIR);
		root->setText(0, rootpath);
		root->setExpanded(true);

		m_rightFileAttris.clear();
		m_rightFileAttrisMap.clear();

		fileAttriNode node;
		node.type = RC_DIR;//是目录
		node.selfItem = root;
		node.parent = nullptr;
		node.relativePath = ".";
		appendAttriNode(node, type);

		m_curRootDirNode = root;

		fileNums = walkLoadFile(rootpath,type);
	}

	ui.statusBar->showMessage(tr("load dir finish, total %1 files").arg(fileNums));

	return fileNums;
}

//从目录列表中，恢复出相对路径
QString CompareDirs::getRelativePathFromDirList(QStringList &dirList, int endPos)
{
	QString path;

	if (0 <= endPos)
	{
		path += dirList.at(0);
	}

	//注意这里的边界，是包含0的，如果是0，则返回第0个
	for (int i=1; i <= endPos; ++i)
	{
		path += "/" + dirList.at(i);
	}
	return path;
}

//从绝对路径中去掉目录路径，得到当前的相对路径
QString CompareDirs::getRelativePath(int dire, QString filePath)
{
	if (0 == dire)
	{
		return filePath.mid(ui.leftPath->text().length()+1);
	}
	else if (1 == dire)
	{
		return filePath.mid(ui.rightPath->text().length()+1);
	}
	return QString("");
}


//根据目录名称，查找其父目录在树上的节点
QTreeWidgetItem *CompareDirs::findItemParentByDirName(QString dirPath, int direction)
{
	QString parentDir;
	int pos = dirPath.lastIndexOf('/');
	if (-1 != pos)
	{
		parentDir = dirPath.left(pos);
	}
	else
	{
		//如果只有一级目录，则其父节点就是根节点,访问根节点
		QList<fileAttriNode>& node = getFileAttriNodeList(direction);

		if (!node.isEmpty())
		{
			return node.at(0).selfItem;
	}
		else
		{
			return nullptr;
		}
	}


	//查找父节点目录
	QMap<QString, fileAttriNode> * fileNodes = getFileAttriNodeMap(direction);

	QMap<QString, fileAttriNode>::iterator it = fileNodes->find(parentDir);

	if (fileNodes->end() != it)
	{
		return it->selfItem;
	}

	return nullptr;
}

QTreeWidgetItem* CompareDirs::findItemByRelativePath(int direction, QString relativePath)
{
	QMap<QString, fileAttriNode>::iterator it;

	if (RC_LEFT == direction)
	{
		it = m_leftFileAttrisMap.find(relativePath);
		if (it != m_leftFileAttrisMap.end())
		{
			return it->selfItem;
	}
	}
	else if(RC_RIGHT == direction)
	{
		it = m_rightFileAttrisMap.find(relativePath);

		if (it != m_rightFileAttrisMap.end())
		{
			return it->selfItem;
	}
	}

	return nullptr;
}

//在文件树上创建多级目录树。dirPath这个目录是不带最开始选择的目录前缀的，都是相对该前缀后的路径
void CompareDirs::createDirOnFileTree(QString dirPath, int direction)
{
	QMap<QString, fileAttriNode> * fileNodes = getFileAttriNodeMap(direction);

	QStringList dirList = dirPath.split('/', QString::SkipEmptyParts);

	//遍历每一个目录，从前开始，如果没有则在文件树上逐级创建目录
	for (int i = 0; i < dirList.size(); ++i)
	{
		QString subDirName = getRelativePathFromDirList(dirList, i);

		//检测目录是否已经存在
		if (fileNodes->contains(subDirName))
		{
			continue;
		}
		else
		{
			//目录不存在，则先在文件树上逐级创建后续的目录文件夹
			//QString subDirName = getRelativePathFromDirList(dirList,i);

			QTreeWidgetItem *newDirItem = new QTreeWidgetSortItem(RC_DIR);
			newDirItem->setData(0, Item_RelativePath, subDirName);

			//找其父节点
			QTreeWidgetItem *parentItem = findItemParentByDirName(subDirName, direction);
			if (parentItem != nullptr)
			{
				parentItem->addChild(newDirItem);

				//把新节点也加入到Map中，否则下次找不到该新节点本身
				fileAttriNode node;
				node.type = RC_DIR;
				node.parent = parentItem;
				node.selfItem = newDirItem;
				node.relativePath = subDirName;
				getFileAttriNodeMap(direction)->insert(subDirName, node);

			}
			QCoreApplication::processEvents();

		}
	}

}

//将左右两边的文件放在一行上进行对比。这个函数也很慢，需要进行多线程改造。
void CompareDirs::displayCmpFileList()
{
	auto isCancel = [this]()->bool {
		if (m_loadFileProcessWin != nullptr)
		{
			if (m_loadFileProcessWin->isCancel())
			{
				m_isCompareCancel = true;
				setWorkStatusInfo(tr("compare canceled ..."));
				return true;
			}
			m_loadFileProcessWin->moveStep();
		}
		return false;
	};

	//先用右边的去同步左边的
	for (int i = 0; i < m_rightFileAttris.count(); ++i)
	{
		//先同步文件夹，如果发现有不存在的文件夹，则需要从右边同步创建到左边
		//如果是文件夹，而且左边没有发现该文件夹
		if (m_rightFileAttris.at(i).type == RC_DIR && !m_leftFileAttrisMap.contains(m_rightFileAttris.at(i).relativePath))
		{
			createDirOnFileTree(m_rightFileAttris.at(i).relativePath,RC_LEFT);

			QCoreApplication::processEvents();
		}

		if (isCancel())
		{
			return;
		}
	}

	for (int i = 0; i < m_rightFileAttris.count(); ++i)
	{
		//没有找到
		if(!m_leftFileAttrisMap.contains(m_rightFileAttris.at(i).relativePath))
		{
			//如果是文件，则再找其所属于的目录
			if (m_rightFileAttris.at(i).type == RC_FILE)
			{
				QString filePath = m_rightFileAttris.at(i).relativePath;


				//对于缺失的文件，也必须标记设置为不同颜色
				setItemForeground(m_rightFileAttris.at(i).selfItem,ALONE_COLOR);
				setItemEqualStatus(m_rightFileAttris.at(i).selfItem, ONLY_ONE_SIDE_FILE);

				setAllParentDirForegroundNoEqual(m_rightFileAttris.at(i).selfItem, UN_EQUAL_COLOR);

				//在左侧找到其上级目录，则加入一条对齐节点
				QTreeWidgetItem * parentNode = findItemParentByDirName(filePath, RC_LEFT);
				if (parentNode != nullptr)
				{
					QTreeWidgetItem *newDirItem = new QTreeWidgetSortItem(RC_PAD_FILE);
					newDirItem->setIcon(0, QIcon(":/Resources/img/point.png"));
					setItemEqualStatus(newDirItem, LOST_SIDE_FILE);
					newDirItem->setData(0, Item_RelativePath, filePath);
					parentNode->addChild(newDirItem);

					fileAttriNode node;
					node.relativePath = filePath;
					node.type = RC_PAD_FILE;//是补齐
					node.selfItem = newDirItem;
					node.parent = parentNode;

					m_leftFileAttrisMap.insert(filePath, node);

					setAllParentDirForegroundNoEqual(newDirItem, UN_EQUAL_COLOR);
					//appendAttriNode(node, RC_LEFT);
				}

					QCoreApplication::processEvents();
			}
		}
		if (isCancel())
		{
			return;
		}
	}

	//再用左边的去同步右边的
	for (int i = 0; i < m_leftFileAttris.count(); ++i)
	{
		//先同步文件夹，如果发现有不存在的文件夹，则需要从右边同步创建到左边
		//如果是文件夹，而且左边没有发现该文件夹
		if (m_leftFileAttris.at(i).type == RC_DIR && !m_rightFileAttrisMap.contains(m_leftFileAttris.at(i).relativePath))
		{
			createDirOnFileTree(m_leftFileAttris.at(i).relativePath, RC_RIGHT);
			QCoreApplication::processEvents();
		}
		if (isCancel())
		{
			return;
		}
	}

	//再用左边的去同步右边的
	for (int i = 0; i < m_leftFileAttris.count(); ++i)
	{
		//没有找到
		//if (-1 == m_rightFileAttris.indexOf(m_leftFileAttris.at(i)))
		if (!m_rightFileAttrisMap.contains(m_leftFileAttris.at(i).relativePath))
		{
			//如果是文件，则再找其目录
			if (m_leftFileAttris.at(i).type == RC_FILE)
			{
				QString filePath = m_leftFileAttris.at(i).relativePath;

				setItemForeground(m_leftFileAttris.at(i).selfItem, ALONE_COLOR);
				setItemEqualStatus(m_leftFileAttris.at(i).selfItem, ONLY_ONE_SIDE_FILE);

				setAllParentDirForegroundNoEqual(m_leftFileAttris.at(i).selfItem, UN_EQUAL_COLOR);

				//找到其上级目录，则加入一条对齐节点
				QTreeWidgetItem * parentNode = findItemParentByDirName(filePath, RC_RIGHT);
				if (parentNode != nullptr)
				{
					QTreeWidgetItem *newDirItem = new QTreeWidgetSortItem(RC_PAD_FILE);
					newDirItem->setIcon(0, QIcon(":/Resources/img/point.png"));
					newDirItem->setData(0, Item_RelativePath, filePath);
					setItemEqualStatus(newDirItem, LOST_SIDE_FILE);
					parentNode->addChild(newDirItem);

					fileAttriNode node;
					node.relativePath = filePath;
					node.type = RC_PAD_FILE;//是补齐
					node.selfItem = newDirItem;
					node.parent = parentNode;

					m_rightFileAttrisMap.insert(filePath,node);

					setAllParentDirForegroundNoEqual(newDirItem, UN_EQUAL_COLOR);
					//appendAttriNode(node, RC_RIGHT);
				}

			}
		}
		if (isCancel())
		{
			return;
		}
	}

}


QString CompareDirs::filePathToRelativePath(QString filePath, int direction)
{
	QString dir;

	if (RC_LEFT == direction)
	{
		dir = ui.leftPath->text();
	}
	else if (RC_RIGHT == direction)
	{
		dir = ui.rightPath->text();
	}

	return filePath.mid(dir.size() + 1);
}

//将QFileInfo转换为fileAttriNode
void CompareDirs::QFileInfoToAttriNode(QFileInfo &fileInfo, fileAttriNode &fileAttri, QTreeWidgetItem* parentItem, int direction)
{
	QString dir;

	if (RC_LEFT == direction)
	{
		dir = ui.leftPath->text();
	}
	else if (RC_RIGHT == direction)
	{
		dir = ui.rightPath->text();
	}

	fileAttri.parent = parentItem;
	fileAttri.type = fileInfo.isDir() ? RC_DIR : RC_FILE;
	fileAttri.relativePath = fileInfo.absoluteFilePath().mid(dir.size()+1);
}

//创建一个空的对齐文件节点，用于左右对齐时一方文件缺失时的补齐显示
void CompareDirs::alignPathToAttriNode(fileAttriNode &fileAttri, QTreeWidgetItem* parentItem, int direction)
{
	QString dir;

	if (RC_LEFT == direction)
	{
		dir = ui.leftPath->text();
	}
	else if (RC_RIGHT == direction)
	{
		dir = ui.rightPath->text();
	}

	fileAttri.parent = parentItem;
	fileAttri.type = 2;
}

void CompareDirs::appendAttriNode(fileAttriNode &fileAttri, int direction)
{
	if (RC_LEFT == direction)
	{
		m_leftFileAttris.append(fileAttri);
	}
	else if (RC_RIGHT == direction)
	{
		m_rightFileAttris.append(fileAttri);
	}
}

QList<fileAttriNode>& CompareDirs::getFileAttriNodeList(int direction)
{
	if (RC_LEFT == direction)
	{
		return m_leftFileAttris;
	}

	return m_rightFileAttris;
}

//从文件列表创建用于索引快速查找的Map
void CompareDirs::createFileAttriMapFromList()
{
	if (m_leftFileAttrisMap.isEmpty())
	{
		for (int i = 0; i < m_leftFileAttris.count(); ++i)
		{
			m_leftFileAttrisMap.insert(m_leftFileAttris.at(i).relativePath, m_leftFileAttris.at(i));

			if (i % 10 == 0)
			{
				QCoreApplication::processEvents();
		}
	}
	}

	if (m_rightFileAttrisMap.isEmpty())
	{
		for (int i = 0; i < m_rightFileAttris.count(); ++i)
		{
			m_rightFileAttrisMap.insert(m_rightFileAttris.at(i).relativePath, m_rightFileAttris.at(i));

			if (i % 10 == 0)
			{
				QCoreApplication::processEvents();
		}
	}
	}
	QCoreApplication::processEvents();
}

QMap<QString, fileAttriNode>* CompareDirs::getFileAttriNodeMap(int direction)
{
	if (RC_LEFT == direction)
	{
		return &m_leftFileAttrisMap;
	}

	return &m_rightFileAttrisMap;
}

QString CompareDirs::getFileSizeFormat(qint64 size)
{
#if 0
	if (size <= 1000)
	{
		return QString("%1").arg(size);
	}

	QString fileSize = QString("%1").arg(size);

	return QString("%1,%2").arg(fileSize.left(fileSize.count()-3)).arg(fileSize.right(3));
#endif
	return QString::number(size);
}

//参数为主函数中添加的item和路径名//递归版本
#if 0 
QFileInfoList CompareDirs::allfile(QTreeWidgetItem *root, QString path, int direction)
{
	/*添加path路径文件*/
	QDir dir(path);          //遍历各级子目录

	//先获取文件到列表
	QFileInfoList file_list = dir.entryInfoList(QDir::Files | QDir::NoSymLinks);


	//再获取文件夹到列表
	QFileInfoList folder_list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);   //获取当前所有目录

	for (int i = 0; i != folder_list.size(); ++i)         //自动递归添加各目录到上一级目录
	{
		QString namepath = folder_list.at(i).absoluteFilePath();    //获取路径
		QFileInfo folderinfo = folder_list.at(i);
		QString name = folderinfo.fileName();      //获取目录名
		QTreeWidgetItem* childroot = new QTreeWidgetSortItem(QStringList() << name, RC_DIR);
		childroot->setIcon(0, QIcon(":/Resources/img/dir.png"));
		root->addChild(childroot);              //将当前目录添加成path的子项

		fileAttriNode node;
		node.type = RC_DIR;//是目录
		node.selfItem = childroot;
		QFileInfoToAttriNode(folderinfo, node, root, direction);

		//把路径名称保存到tips中，后续需要这个来排序，下同
		childroot->setData(0, Item_RelativePath, node.relativePath);
		appendAttriNode(node, direction);

		QFileInfoList child_file_list = allfile(childroot, namepath, direction);          //进行递归
		file_list.append(folderinfo);
		file_list.append(child_file_list);


	}

	//遍历子目录中所有文件;建立每个文件叶子节点
	QDir dir_file(path);   
	dir_file.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);//获取当前所有文件
	QFileInfoList list_file = dir_file.entryInfoList();
	for (int i = 0; i < list_file.size(); ++i) 
	{  //将当前目录中所有文件添加到treewidget中
		QFileInfo fileInfo = list_file.at(i);

		QString name2 = fileInfo.fileName();
		QTreeWidgetItem* child = new QTreeWidgetSortItem(QStringList() << name2, RC_FILE);
		child->setIcon(0, QIcon(":/Resources/img/point.png"));

		child->setText(1, getFileSizeFormat(fileInfo.size()));
		QString lastModifyTime = fileInfo.lastModified().toString("yy/MM/dd hh:mm:ss");
		child->setText(2, lastModifyTime);
		root->addChild(child);

		fileAttriNode node;
		node.type = RC_FILE;//是文件
		node.selfItem = child;
		QFileInfoToAttriNode(fileInfo,node,root, direction);

		child->setData(0, Item_RelativePath, node.relativePath);
		appendAttriNode(node, direction);
	}

	return file_list;
}
#endif

#if 0
//非递归版本
int CompareDirs::allfile(QTreeWidgetItem* root_, QString path_, int direction_)
{
	QList<WalkFileInfo> dirsList;
	WalkFileInfo oneDir(direction_, root_, path_);
	dirsList.append(oneDir);

	int fileNums = 0;

	//再获取文件夹到列表
	QDir::Filters dirfilter;

	if (s_compareHideDir)
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

	QStringList skipDirList = s_skipDirs.split(":");

	auto dirFilter = [&skipDirList](QString& dirName)->bool {

		//如果在过滤列表中，则过滤返回false;
		if (-1 != skipDirList.indexOf(dirName))
		{
			return false;
		}
		return true;
	};

	QStringList skipFileExtList = s_skipFileExt.split(":");

	auto fileExtFilter = [&skipFileExtList](QString fileExt)->bool {

		//如果在过滤列表中，则过滤返回false;
		if (-1 != skipFileExtList.indexOf(QString(".%1").arg(fileExt)))
		{
			return false;
		}
		return true;
	};

	QStringList skipFileNamePrefix = s_skipNamePrefix.split(":");

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

	if (m_loadFileProcessWin == nullptr)
	{
		m_loadFileProcessWin = new ProgressWin(this);
	}
	m_loadFileProcessWin->setWindowModality(Qt::WindowModal);

	m_loadFileProcessWin->info(tr("load dir file tree in progress\n, please wait ..."));

	if (!s_compareChildDir)
	{
		m_loadFileProcessWin->info(tr("The current rule does not process subdirectories ..."));
	}

	m_loadFileProcessWin->show();

	int dirNums = 0;

	bool firstChildDirs = true;
	int totalStep = 0;

	while (!dirsList.isEmpty())
	{
		WalkFileInfo curDir = dirsList.takeFirst();

		QTreeWidgetItem* root = curDir.root;
		QString path = curDir.path;
		int direction = curDir.direction;

		if (s_compareChildDir)
		{
			/*添加path路径文件*/
			QDir dir(path);          //遍历各级子目录

			QFileInfoList folder_list = dir.entryInfoList(dirfilter);   //获取当前所有目录

			for (int i = 0; i != folder_list.size(); ++i)         //自动递归添加各目录到上一级目录
			{
				QString namepath = folder_list.at(i).absoluteFilePath();    //获取路径
				QFileInfo folderinfo = folder_list.at(i);

				if (!s_compareHideDir && folderinfo.baseName().isEmpty())
				{
					m_loadFileProcessWin->info(tr("skip dir %1").arg(namepath));
					continue;
				}

				QString name = folderinfo.fileName();      //获取目录名

				if (s_isSkipDir && !dirFilter(name))
				{
					m_loadFileProcessWin->info(tr("skip dir %1").arg(namepath));
					continue;
				}

				QTreeWidgetItem* childroot = new QTreeWidgetSortItem(QStringList() << name, RC_DIR);
				childroot->setIcon(0, QIcon(":/Resources/img/dir.png"));
				root->addChild(childroot);              //将当前目录添加成path的子项

				fileAttriNode node;
				node.type = RC_DIR;//是目录
				node.selfItem = childroot;
				QFileInfoToAttriNode(folderinfo, node, root, direction);

				//把路径名称保存到tips中，后续需要这个来排序，下同
				childroot->setData(0, Item_RelativePath, node.relativePath);
				appendAttriNode(node, direction);


				WalkFileInfo oneDir(direction, childroot, namepath);
				dirsList.push_front(oneDir);

				dirNums++;

				m_loadFileProcessWin->info(tr("load %1 dir %2").arg(dirNums).arg(namepath));

				//文件太少，会导致提前出去，外部可能先执行导致崩溃
				if (folder_list.size() > 10)
				{
					QCoreApplication::processEvents(/*QEventLoop::ExcludeUserInputEvents*/);
				}
			}

			if (firstChildDirs)
			{
				totalStep = dirNums;
				m_loadFileProcessWin->setTotalSteps(dirNums);
				firstChildDirs = false;
			}

			if (dirsList.size() < totalStep)
			{
				totalStep = dirsList.size();
				m_loadFileProcessWin->moveStep();
			}


			qint64 maxFileSize = 0;

			QDir dir_file(path);
			dir_file.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);//获取当前所有文件
			QFileInfoList list_file = dir_file.entryInfoList();
			for (int i = 0; i < list_file.size(); ++i)
			{  //将当前目录中所有文件添加到treewidget中
				QFileInfo fileInfo = list_file.at(i);

				//不支持所有文件，仅仅支持支持的类型
				if ((s_compareAllFiles == 2) && !fileFilter(fileInfo))
				{
					continue;
				}

				QString name2 = fileInfo.fileName();

				if (s_isSkipFile && !fileExtFilter(fileInfo.suffix()))
				{
					m_loadFileProcessWin->info(tr("skip file ext %1").arg(name2));
					continue;
				}

				if (s_isSkipNamePrefix && !fileNamePrefixFilter(name2))
				{
					m_loadFileProcessWin->info(tr("skip file prefix %1").arg(name2));
					continue;
				}



				QTreeWidgetItem* child = new QTreeWidgetSortItem(QStringList() << name2, RC_FILE);
				child->setIcon(0, QIcon(":/Resources/img/point.png"));
				qint64 fileSize = fileInfo.size();
				child->setText(1, getFileSizeFormat(fileSize));
				QString lastModifyTime = fileInfo.lastModified().toString("yy/MM/dd hh:mm:ss");
				//修改时间。上次修改的实际。
				child->setText(2, lastModifyTime);
				root->addChild(child);

				//记录当前目录下的最大值
				if (maxFileSize < fileSize)
				{
					maxFileSize = fileSize;
				}

				fileAttriNode node;
				node.type = RC_FILE;//是文件
				node.selfItem = child;
				QFileInfoToAttriNode(fileInfo, node, root, direction);

				child->setData(0, Item_RelativePath, node.relativePath);
				appendAttriNode(node, direction);


				if (m_loadFileProcessWin->isCancel())
				{
					m_loadFileProcessWin->info(tr("compare canceled ..."));
					break;
				}
				QCoreApplication::processEvents();
			}

			//记录最大文件到目录的DIR_ITEM_MAXSIZE_FILE中，后续排序时，文件夹大小按照此来排序
			root->setData(0, DIR_ITEM_MAXSIZE_FILE, maxFileSize);


			fileNums += list_file.size();

			if (m_loadFileProcessWin->isCancel())
			{
				break;
			}

		}

		if (m_loadFileProcessWin->isCancel())
		{
			m_loadDirCancel = true;
		}

		if (m_loadFileProcessWin != nullptr)
		{
			delete m_loadFileProcessWin;
			m_loadFileProcessWin = nullptr;
		}

		if (m_loadDirCancel)
		{
			close();
		}

		return fileNums;
}

#endif

void CompareDirs::on_childThreadMsg(int type, QString msg, qint64 value)
{
	if (m_loadFileProcessWin != nullptr)
	{
		switch (type)
		{
		case PROGRESS_INFO:
			m_loadFileProcessWin->info(msg);
			break;
		case SET_PROGRESS_TOTAL_STEPS:
			m_loadFileProcessWin->setTotalSteps(value);
			break;
		case PROGRESS_STEP:
			m_loadFileProcessWin->moveStep();
			break;
		case ADD_DIR_NODE://增加目录节点
		case ADD_FILE_NODE://增加文件节点
			break;
		case DIR_MAX_FILE_SIZE://文件夹下面最大的文件大小，后续要使用这个排序文件夹。
		{
			//当前文件夹的最大文件的大小，用于排序文件夹。
		}
			break;
		default:
			break;
		}
	}
}

//增加文件或文件夹节点
void CompareDirs::on_addNode(CmpWalkFileInfo* nodeInfo)
{
	switch (nodeInfo->type)
	{
	case RC_DIR:
	{
		if (m_loadDirCancel)
		{
			break;
		}

		//创建1个目录节点
		QTreeWidgetItem* childroot = new QTreeWidgetSortItem(QStringList() << nodeInfo->name, RC_DIR);

		//注意顺序，这里和子线程里面加入的顺序，要保持一致。
		m_dirNodeList.push_front(childroot);

		childroot->setIcon(0, QIcon(":/Resources/img/dir.png"));
		m_curRootDirNode->addChild(childroot);              //将当前目录添加成path的子项

		fileAttriNode node;
		node.type = RC_DIR;//是目录
		node.selfItem = childroot;
		QFileInfoToAttriNode(nodeInfo->fileinfo, node, m_curRootDirNode, nodeInfo->direction);

		//把路径名称保存到tips中，后续需要这个来排序，下同
		childroot->setData(0, Item_RelativePath, node.relativePath);
		appendAttriNode(node, nodeInfo->direction);
	}
	break;

	case RC_FILE:
	{
		if (m_loadDirCancel)
		{
			break;
		}

		QTreeWidgetItem* child = new QTreeWidgetSortItem(QStringList() << nodeInfo->name, RC_FILE);
		child->setIcon(0, QIcon(":/Resources/img/point.png"));
		qint64 fileSize = nodeInfo->fileinfo.size();
		child->setText(1, getFileSizeFormat(fileSize));

		//修改时间。上次修改的实际。
		child->setText(2, nodeInfo->modifyTime);
		m_curRootDirNode->addChild(child);

		fileAttriNode node;
		node.type = RC_FILE;//是文件
		node.selfItem = child;
		QFileInfoToAttriNode(nodeInfo->fileinfo, node, m_curRootDirNode, nodeInfo->direction);

		child->setData(0, Item_RelativePath, node.relativePath);
		appendAttriNode(node, nodeInfo->direction);
	}
	break;

	case CHANGE_CURRENT_DIR_NODE:
	{
		//切换当前的文件夹根目录。要和on_walkFile里面的保存一致的顺序。
		if (!m_dirNodeList.isEmpty())
		{
		m_curRootDirNode = m_dirNodeList.takeFirst();
	}
	}
	break;

	case DIR_MAX_FILE_SIZE:
	{
		if (m_loadDirCancel)
		{
			break;
		}

		//记录最大文件到目录的DIR_ITEM_MAXSIZE_FILE中，后续排序时，文件夹大小按照此来排序
		m_curRootDirNode->setData(0, DIR_ITEM_MAXSIZE_FILE, nodeInfo->fileSize);
	}
	break;

	//最后一个节点槽函数执行完毕后，此时才能认定为加载完毕。
	case DIR_FILE_LOAD_FINISHED:
	{
		m_loadDirFinished = true;
	}
		break;
	}

	delete nodeInfo;
}

//在子线程中执行的版本。
int CompareDirs::walkLoadFile(QString path_, int direction_)
{
	assert(m_cmpDataThread != nullptr);

	m_cmpDataThread->setCancel(false);
	m_cmpDataThread->setIsDone(false);

	m_dirNodeList.clear();

	if (m_loadFileProcessWin == nullptr)
	{
		m_loadFileProcessWin = new ProgressWin(this);
	}

	m_loadFileProcessWin->setWindowModality(Qt::WindowModal);

	m_loadFileProcessWin->info(tr("load dir file tree in progress\n, please wait ..."));

	m_loadFileProcessWin->show();

	m_loadDirCancel = false;

	m_loadDirFinished = false;

	emit s_walkFile(direction_, path_, s_compareHideDir, s_isSkipDir, s_isSkipFile, s_isSkipNamePrefix, s_compareAllFiles, s_skipDirs, s_skipFileExt, s_skipNamePrefix);


	while (/*!m_cmpDataThread->isDone()*/ !m_loadDirFinished)
	{
		QCoreApplication::processEvents();

		if (m_loadFileProcessWin->isCancel())
		{
			m_cmpDataThread->setCancel(true);
			m_loadDirCancel = true;
		}
	}


	if (m_loadFileProcessWin != nullptr)
	{
		delete m_loadFileProcessWin;
		m_loadFileProcessWin = nullptr;
	}

	if (m_loadDirCancel)
	{
		close();
	}

	return m_cmpDataThread->getFileNums();
}

#if 0
//对item进行间隔着色
void CompareDirs::setItemIntervalBackground()
{
	m_curItemIndex = 0;

	QTreeWidgetItemIterator it(ui.leftTreeDir);
	while (*it) {
		if (m_curItemIndex % 2 == 1)
		{
			setItemBackground(*it, QColor(0xf8faf9));
		}
		++it;
		++m_curItemIndex;
	}

	m_curItemIndex = 0;

	QTreeWidgetItemIterator it1(ui.rightTreeDir);
	while (*it1) {
		if (m_curItemIndex % 2 == 1)
		{
			setItemBackground(*it1, QColor(0xf8faf9));
		}
		++it1;
		++m_curItemIndex;
	}
}
#endif

void CompareDirs::setItemBackground(QTreeWidgetItem* item, const QColor& color)
{
	QBrush b(color);
	item->setBackground(0, b);
	item->setBackground(1, b);
	item->setBackground(2, b);
}

void CompareDirs::setItemForeground(QTreeWidgetItem* item, const QColor& color)
{
	QBrush b(color);
	item->setForeground(0, b);
	item->setForeground(1, b);
	item->setForeground(2, b);
}
//0相等 1 不等 2 独有
void CompareDirs::setItemEqualStatus(QTreeWidgetItem* item, ITEM_FILE_STATUS status)
{
	item->setData(0, Item_Eqult_Status, status);


	//20230804做提速，性能分析发现下面切换图标的地方，非常慢。干脆不做这一步了。

#if 0
	switch (status)
	{
	case NO_USER:
		break;
	case EQUAL_FILE:
		if (item->type() != RC_DIR)
		{
			item->setIcon(0, QIcon(":/Resources/img/point.png"));
		}
		else
		{
			item->setIcon(0, QIcon(":/Resources/img/dir.png"));
		}
		break;
	case NO_EQUAL_FILE:
		if (item->type() != RC_DIR)
		{
			item->setIcon(0, QIcon(":/Resources/img/unequalfile.png"));
		}
		else
		{
			item->setIcon(0, QIcon(":/Resources/img/unequaldir.png")); //性能分析发现这里比较慢。
		}
		break;
	case ONLY_ONE_SIDE_FILE:
		item->setIcon(0, QIcon(":/Resources/img/onlyfile.png"));
		break;
	case DELETED_FILE:
		break;
	default:
		break;
	}
#endif
}

ITEM_FILE_STATUS CompareDirs::getItemEqualStatus(QTreeWidgetItem* item)
{
	QVariant v = item->data(0, Item_Eqult_Status);
	if (v.isNull())
	{
		return NO_USER;
	}
	return static_cast<ITEM_FILE_STATUS>(v.toInt());
}

//把所有父节点设置为不等颜色
void CompareDirs::setAllParentDirForegroundNoEqual(QTreeWidgetItem* item, const QColor& color)
{
	QBrush a(color);

	QTreeWidgetItem* parent = item->parent();

	while (parent != nullptr)
	{
		QBrush b = parent->background(0);
		if (b == a)
		{
			break;
		}
		setItemForeground(parent,color);
		setItemEqualStatus(parent, NO_EQUAL_FILE);

		parent = parent->parent();
	}
}

void CompareDirs::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasFormat("text/uri-list")) //只能打开文本文件
	{
		event->accept(); //可以在这个窗口部件上拖放对象
	}
	else
	{
		event->ignore();
	}

}


void CompareDirs::dropEvent(QDropEvent* e)
{
	QList<QUrl> urls = e->mimeData()->urls();
	if (urls.isEmpty())
		return;

	QString dirName = urls.first().toLocalFile();

	if (dirName.isEmpty())
	{
		return;
	}

    //发现mac下面如果目录以/结尾，会导致目录对比不识别名称
#ifdef Q_OS_MAC
    if (dirName.endsWith('/'))
    {
        dirName = dirName.mid(0,dirName.size()-1);
    }
#endif
	QDir dir(dirName);

	if (!dir.exists())
	{
		return;
	}

	//在左边
	if (ui.leftTreeDir->geometry().contains(ui.leftChildWidget->mapFromGlobal(QCursor::pos())))
	{

	/*	m_leftFileNums = loadDir(RC_LEFT, dirName);
		cmpFileTree();*/
		emit delayWork(RC_LEFT, dirName);
		e->accept();

	}
	else if (ui.rightTreeDir->geometry().contains(ui.rightChildWidget->mapFromGlobal(QCursor::pos())))
	{
		//m_rightFileNums = loadDir(RC_RIGHT, dirName);
		//cmpFileTree();
		emit delayWork(RC_RIGHT, dirName);
		e->accept();
	}
	else
	{
		e->ignore();
	}

}

void CompareDirs::closeEvent(QCloseEvent* e)
{
	if (m_leftFileNums > 3000)
	{
		if (QMessageBox::No == QMessageBox::question(this, tr("Close ?"), tr("Are you sure quit ?")))
		{
			e->ignore();
			return;
		}
	}
	emit signCmpDirClose();

	QMainWindow::closeEvent(e);

	e->accept();
}

//重新加载目录
void CompareDirs::slot_reloadLeftDir()
{
	slot_reloadBt();
}

void CompareDirs::slot_reloadRightDir()
{
	slot_reloadBt();
}
