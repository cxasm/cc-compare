#include "comparewin.h"
#include "CmpareMode.h"
#include "Lcs.h"
#include "StrategyCompare.h"
#include "closeDlg.h"
#include "BlockCompare.h"
#include "MediatorDisplay.h"
#include "statuswidget.h"
#include "blockuserdata.h"
#include "rcglobal.h"
#include "Scintilla.h"
#include "compareworker.h"
#include "noequallinemanager.h"
#include "gotolinewin.h"
#include "filecmprulewin.h"
#include "ctipwin.h"
#include "doctypelistview.h"
#include "findcmpwin.h"
#include "alignwin.h"
#include "diffstatuswin.h"
#include "cmpsql.h"

#ifdef OPEN_UNDO_REDO
#include "asynccommand.h"
#include "editcommand.h"
#include "userdatacommand.h"
#include "replacecommand.h"
#endif

#include <QFileDialog>
#include <functional>

#include <QTextDocument>
#include <QTextBlock>
#include <QScrollBar>
#include <QToolBar>
#include <QToolButton>
#include <QFutureWatcher>


#include <QMap>
#include <QMessageBox>
#include <QTextCodec>
#include <QDockWidget>
#include <QInputDialog>

#include <qsciscintillabase.h>
#include <QMimeDatabase>


const int MSG_SHOW_TIME = 3000;
//#define TEST

/* 一定要做到MVC分离，这个规定是一个UI类，制作与显示和交互相关的操作 */

//20211014 一定注意release不要放开_test，因为会在槽函数中中断，去调用下一个槽函数，已经发现问题了。
//Debug是为了调试，有时候迫不得已。今天发现在中文下输入4个字符，qscint会输出4个修改新增信号（可能因为我屏蔽了中间输入状态，见inputMethodEvent函数），
//而在编辑后处理的函数中,会出发4次修改。但是由于编辑是异步事件，而4次修改是直连同步，如果开启postevent，这里就乱了。如果不开启，4次直连会在异步之前完成
//而4次异步消息，只有第一次会处理，因为第一次就把4个修改全部处理完毕了。后面3个是空循环
//如果开启该宏，会导致退出当前处理，去处理下一个消息，导致编辑后同步槽函数多次重入。

//如果要彻底修改这个问题，可以修改qscint中的消息机制，在输入没有完成之前，不要出发修改消息，只在最后输入全部完成后一次性发送一次修改消息。
//#define _test 1



const int STYLE_EQUAL_TEXT_COLOUR = 2;
const int STYLE_COLOUR_RED = 1;

const int STYLE_PAD_BACK_PIXMAP = 3;//补齐用的底纹
const int STYLE_PAD_BACK_PIXMAP_BIT_MASK = 0x8; //不对，还有问题，与我理解的位不一样

const int STYLE_BACK_COLOUR_NO_EQUAL = 4;//不等块的背景颜色
const int STYLE_BACK_COLOUR_NO_EQUAL_BIT_MASK = 0x10;

const int STYLE_BACK_COLOUR_EQUAL = 6;//相同部分的背景颜色
const int STYLE_BACK_COLOUR_EQUAL_BIT_MASK = 0x40;

//默认是在亮色模式下进行配色
//不同部分的背景色
long STYLE_NO_EQUAL_BACK_COLOR_VALUE = 0xe3e3ff;
//不同部分的前景色
long STYLE_NO_EQUAL_FORE_COLOR_VALUE = 0x0000ff;
//相同部分的文字的颜色
long STYLE_EQUAL_TEXT_COLOUR_VALUE = 0;
//相同部分的背景色
long STYLE_EQUAL_BACK_COLOR_VALUE = 0xffffff; //应该设置为主题色

long MARGIN_VER_LINE_COLOR_VALUE = 0x000000;

const int STYLE_LAST_PAD_LINE = 5;
const int STYLE_LAST_PAD_LINE_BIT_MASK = 0x20;

const int STYLE_PAD_OR_LASTPAD_BIT_MASK = 0x28; //复合STYLE_PAD_BACK_PIXMAP_BIT_MASK|STYLE_LAST_PAD_LINE_BIT_MASK

//每使用一次则增加一次计数
int CompareWin::s_useTimes = 0;
int CompareWin::s_defaultFontSize = 10;

static QMimeDatabase s_filetype_db;

static QString NEEDSAVE = ":/Resources/img/needsave.png";
static QString SAVE = ":/Resources/img/save.png";
static QString NEEDSAVEDARK = NEEDSAVE;
static QString SAVEDARK = SAVE;

//static QString NEEDSAVEDARK = ":/Resources/img/needsave.png";
//static QString SAVEDARK = ":/Resources/img/savedark.png";

void initColorModle()
{
	////如果是暗色模式，修改主要颜色
	//if (BLACK_SE == StyleSet::getCurrentSytleId())
	//{
	//	//不同部分的背景色
	//	STYLE_NO_EQUAL_BACK_COLOR_VALUE = 0x555555;
	//	//不同部分的前景色
	//	STYLE_NO_EQUAL_FORE_COLOR_VALUE = 0x00aaff;
	//	//相同部分的文字的颜色
	//	STYLE_EQUAL_TEXT_COLOUR_VALUE = 0xffffff;

	//	STYLE_EQUAL_BACK_COLOR_VALUE = 0x282020;

	//	MARGIN_VER_LINE_COLOR_VALUE = 0xffffff;
	//}
	//else
	//{
	//	//不同部分的背景色
	//	STYLE_NO_EQUAL_BACK_COLOR_VALUE = 0xe3e3ff;
	//	//不同部分的前景色
	//	STYLE_NO_EQUAL_FORE_COLOR_VALUE = 0x0000ff;
	//	//相同部分的文字的颜色
	//	STYLE_EQUAL_TEXT_COLOUR_VALUE = 0;

	//	STYLE_EQUAL_BACK_COLOR_VALUE = (StyleSet::foldbgColor.blue() << 16 | StyleSet::foldbgColor.green() << 8 | StyleSet::foldbgColor.red());

	//	MARGIN_VER_LINE_COLOR_VALUE = 0;
	//}
	}

void setLineStyle(QsciDisplayWindow*  uiSrc, int style, int lineNum)
{
	int startPos = uiSrc->SendScintilla(SCI_POSITIONFROMLINE, lineNum);
	uiSrc->SendScintilla(SCI_STARTSTYLING, startPos);

	int length = uiSrc->SendScintilla(SCI_LINELENGTH, lineNum);
	uiSrc->SendScintilla(SCI_SETSTYLING, length, style);
}

//对比里面的序号，和CODE_ID并不是一一对应的，需要转换一下。
//如果leftCodeBox变化了，则下面的也要保持修改。

int getIndexFromCode(CODE_ID code)
{
	int ret = 12;//UNKONWN
	switch (code)
	{
	case UNKOWN:
		break;
	case ANSI:
		break;
	case UTF8_NOBOM:
		ret = 0;
		break;
	case UTF8_BOM:
		ret = 1;
		break;
	case UNICODE_LE:
		ret = 2;
		break;
	case UNICODE_BE:
		ret = 3;
		break;
	case GBK:
		ret = 4;
		break;
	case EUC_JP:
		ret = 5;
		break;
	case Shift_JIS:
		ret = 6;
		break;
	case EUC_KR:
		ret = 7;
		break;
	case KOI8_R:
		ret = 8;
		break;
	case TSCII:
		ret = 9;
		break;
	case TIS_620:
		ret = 10;
		break;
	case BIG5:
		ret = 11;
		break;
	case UNICODE_LE_NOBOM:
		break;
	case WINDOWS1250:
		ret = 12;
		break;
	case IBM866:
		break;
	case CODE_END:
		break;
	default:
		break;
	}
	return ret;
}

//把对比框的index序号转换为code
static inline  CODE_ID getCodeFromIndex(int index)
{
	CODE_ID ret = UNKOWN;
	switch (index)
	{
	case 0:
		ret = CODE_ID::UTF8_NOBOM;
		break;
	case 1:
		ret = CODE_ID::UTF8_BOM;
		break;
	case 2:
		ret = CODE_ID::UNICODE_LE;
		break;
	case 3:
		ret = CODE_ID::UNICODE_BE;
		break;
	case 4:
		ret = CODE_ID::GBK;
		break;
	case 5:
		ret = CODE_ID::EUC_JP;
		break;
	case 6:
		ret = CODE_ID::Shift_JIS;
		break;
	case 7:
		ret = CODE_ID::EUC_KR;
		break;
	case 8:
		ret = CODE_ID::KOI8_R;
		break;
	case 9:
		ret = CODE_ID::TSCII;
		break;
	case 10:
		ret = CODE_ID::TIS_620;
		break;
	case 11:
		ret = CODE_ID::BIG5;
		break;
	case 12:
		ret = CODE_ID::WINDOWS1250;
		break;
	case 13:
		ret = CODE_ID::UNKOWN;
		break;
	default:
		break;
	}
	return ret;
}


CompareWin::CompareWin(QWidget *parent) :QMainWindow(parent), m_pCmpMode(nullptr), m_commandIndex(0), m_enableTextChangeSignal(false),m_cmpMode(2),
m_isBlankLineDoMatch(true), m_lineMatchEqualRata(50), m_workStatus(FREE_STATUS),m_workMode(1), m_isTextChangeSignalOnMemoryMode(false), m_leftExternBlockInfo(nullptr), m_rightExternBlockInfo(nullptr),\
m_isCancel(false), m_isCmpFinished(true), m_dockDiffStatusWin(nullptr), m_diffStatusWin(nullptr), m_leftSaveCodeChange(false), m_rightSaveCodeChange(false)
{
	ui.setupUi(this);

	init();
}

/* 用于从其它地方直接传递文件过来进行对比 ，不需要手动选择文件路径 */
CompareWin::CompareWin(QString leftFile, QString rightFile, QWidget *parent) :QMainWindow(parent), m_pCmpMode(nullptr), m_enableTextChangeSignal(false), m_cmpMode(2),
m_isBlankLineDoMatch(true), m_lineMatchEqualRata(50), m_workStatus(FREE_STATUS), m_workMode(0), m_isTextChangeSignalOnMemoryMode(false), m_leftExternBlockInfo(nullptr), m_rightExternBlockInfo(nullptr), \
m_isCancel(false), m_isCmpFinished(true), m_dockDiffStatusWin(nullptr), m_diffStatusWin(nullptr), m_leftSaveCodeChange(false), m_rightSaveCodeChange(false)
{
	ui.setupUi(this);

	ui.leftPath->setText(leftFile);
	ui.rightPath->setText(rightFile);

	//要注意顺序，这里肯定是文件对比
	init();

	ui.leftSrc->setFilePath(leftFile);
	ui.rightSrc->setFilePath(rightFile);

	m_leftExternBlockInfo = nullptr;
	m_rightExternBlockInfo = nullptr;
}

void CompareWin::clearInterStatus()
{
#ifdef OPEN_UNDO_REDO

	for (auto var : m_undoList)
	{
		delete var;
	}
	m_undoList.clear();
	m_commandIndex = 0;
#endif

	m_leftNoEqualBlocks.clear();
	m_rightNoEqualBlocks.clear();

	m_curShowBlockNums = -1;

	m_isLeftDirty = false;
	m_isRightDirty = false;

	syncNeedSaveIcon();

	//这里因为放空了用户手动指定编码的过程，所以不能每次清空
	//如果用户就是指定了编码，就按照用户编码来
	//m_leftCode = CODE_ID::UNKOWN;
	//m_rightCode = CODE_ID::UNKOWN;


	if (m_pCmpMode != nullptr)
	{
		delete m_pCmpMode;
		m_pCmpMode = nullptr;
	}

	m_leftExternBlockInfo = nullptr;
	m_rightExternBlockInfo = nullptr;


	m_leftModifyRecord.clear();
	m_rightModifyRecord.clear();
}

void CompareWin::clearStatus()
{
	clearInterStatus();

	if (m_workMode == 1)
	{
		//如果当前是内存模式
		enableSignTextChange(false);
		disableTextChangeSignal();

		ui.leftSrc->clear();
		ui.rightSrc->clear();

		enableSignTextChange(true);
	}
	else if (m_workMode == 0)
	{
		//当前是文件模式
		enableSignTextChange(false);
		disableTextChangeSignal();

		ui.leftSrc->clear();
		ui.rightSrc->clear();

		//enableTextChangeSignal();

		//默认进入内存对比模式
		m_workMode = 1;
		enableSignTextChange(true);
	}

	blockCompare->setEmptyFileStatus(0);

	m_isCmpFinished = true;
	m_isCancel = false;
}

void CompareWin::init()
{

	ui.leftDiffView->setWrapMode(QsciScintilla::WrapWord);
	ui.rightDiffView->setWrapMode(QsciScintilla::WrapWord);

	ui.splitter_left->setStretchFactor(0, 6);
	ui.splitter_left->setStretchFactor(1, 1);

	ui.splitter_right->setStretchFactor(0, 6);
	ui.splitter_right->setStretchFactor(1, 1);

	m_splitterLeftPos = -1;
	m_splitterRightPos = -1;

	//千万不能使用队列连接，否则可能导致来回震荡。为保险起见，最多3次。
	connect(ui.splitter_left, &QSplitter::splitterMoved, this, [this](int pos, int index) {

		if (m_splitterLeftPos == pos)
		{
			return;
		}
		m_splitterLeftPos = pos;

		qDebug() << "left" << pos << index;
		if (m_splitterRightPos != pos)
		{
			ui.splitter_right->moveToSplitter(pos, index);
		}

		});

	connect(ui.splitter_right, &QSplitter::splitterMoved, this, [this](int pos, int index) {

		if (m_splitterRightPos == pos)
		{
			return;
		}

		m_splitterRightPos = pos;

		qDebug() << "right" << pos << index;
		if (m_splitterLeftPos != pos)
		{
			ui.splitter_left->moveToSplitter(pos, index);
		}


		});

	initColorModle();

	//禁止编辑器本身的undo和redo。只能处理文本，而不能处理marker。
	//需要自己来管理undo和redo。这里会崩溃在debug模式下，所有qscintilla也做了修改
	ui.leftSrc->SendScintilla(SCI_EMPTYUNDOBUFFER);
	ui.rightSrc->SendScintilla(SCI_EMPTYUNDOBUFFER);

	//启用拖动
	setAcceptDrops(true);

	//监控回车
	connect(ui.leftPath, &QLineEdit::returnPressed, this, &CompareWin::on_leftPathReturnPressed);
	connect(ui.rightPath, &QLineEdit::returnPressed, this, &CompareWin::on_rightPathReturnPressed);

	m_leftRightOrder = 0;

	blockCompare = new BlockCompare();
	blockCompare->m_isCancel = &m_isCancel;

	ui.leftSrc->setDirection(RC_LEFT);
	ui.rightSrc->setDirection(RC_RIGHT);

	m_curShowBlockNums = -1;

	m_leftCode = CODE_ID::UNKOWN;
	m_rightCode = CODE_ID::UNKOWN;

	m_leftOperStatus = OperStatus::RC_NONE;
	m_rightOperStatus = OperStatus::RC_NONE;

#if 1

	//选中的左右行特殊显示
	//这里如果直接让左右互相同步，互相影响，可能导致混乱。需要一个中间调停者模式，作为中间人去控制同步消息
	//中介者模式意图：用一个中介对象来封装一系列的对象交互，中介者使各对象不需要显式地相互引用，从而使其耦合松散，而且可以独立地改变它们之间的交互。
	//主要解决：对象与对象之间存在大量的关联关系，这样势必会导致系统的结构变得很复杂，同时若一个对象发生改变，我们也需要跟踪与之相关联的对象，同时做出相应的处理。
	//connect(ui.leftSrc, &DisplayWindow::cursorChangeY, ui.rightSrc, &DisplayWindow::cursorChangeLine);
	//connect(ui.rightSrc, &DisplayWindow::cursorChangeY, ui.leftSrc, &DisplayWindow::cursorChangeLine);

	m_mediator = new MediatorDisplay();

	ui.leftSrc->setMediator(m_mediator);
	ui.rightSrc->setMediator(m_mediator);


#endif
	QToolButton* showWhiteChar = new QToolButton(ui.mainToolBar);

	showWhiteChar->setCheckable(true);
	connect(showWhiteChar, &QAbstractButton::toggled, this, &CompareWin::slot_whiteCharClick);

	showWhiteChar->setFixedSize(48, 48);
	showWhiteChar->setIcon(QIcon(":/Resources/img/hidechar.png"));
	showWhiteChar->setText(tr("white"));
	showWhiteChar->setToolTip(tr("white"));
	showWhiteChar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(showWhiteChar);

	QToolButton* ruleBt = new QToolButton(ui.mainToolBar);
	ruleBt->setFixedSize(48, 48);
	ruleBt->setIcon(QIcon(":/Resources/img/rule.png"));
	ruleBt->setText(tr("rule"));
	ruleBt->setToolTip(tr("rule"));
	ruleBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(ruleBt);
	connect(ruleBt, &QToolButton::clicked, this, &CompareWin::slot_rultBt);

	QToolButton* breakBt = new QToolButton(ui.mainToolBar);
	breakBt->setFixedSize(48, 48);
	breakBt->setCheckable(true);
	breakBt->setIcon(QIcon(":/Resources/img/break.png"));
	breakBt->setText(tr("break"));
	breakBt->setToolTip(tr("break"));
	breakBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(breakBt);
	connect(breakBt, &QToolButton::clicked, this, &CompareWin::slot_breakBt);

	QToolButton* pullBt = new QToolButton(ui.mainToolBar);
	pullBt->setFixedSize(48, 48);
	//aglinBt->setCheckable(true);
	pullBt->setIcon(QIcon(":/Resources/img/pullopen.png"));
	pullBt->setText(tr("pull"));
	pullBt->setToolTip(tr("pull open"));
	pullBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(pullBt);
	connect(pullBt, &QToolButton::clicked, this, &CompareWin::slot_pullopenBt);

	ui.mainToolBar->addSeparator();

	QString key("cmpmode");
	m_cmpMode = CmpSql::getKeyValueFromNumSets(key);

	//退出时，对比该值，看看是否有修改。
	m_srcCmpMode = m_cmpMode;

	m_strictBt = new QToolButton(ui.mainToolBar);
	m_strictBt->setFixedSize(48, 48);
	m_strictBt->setCheckable(true);
	m_strictBt->setChecked((m_cmpMode == 1));
	m_strictBt->setIcon(QIcon(":/Resources/img/strict.png"));
	m_strictBt->setText(tr("strict"));
	m_strictBt->setToolTip(tr("strict"));
	m_strictBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(m_strictBt);
	connect(m_strictBt, &QToolButton::clicked, this, &CompareWin::slot_strictBt);

	//宽容模式。忽略所有空白符号。默认开启忽略。
	m_tolerantBt = new QToolButton(ui.mainToolBar);
	m_tolerantBt->setFixedSize(48, 48);
	m_tolerantBt->setCheckable(true);
	m_tolerantBt->setChecked((m_cmpMode == 2));
	m_tolerantBt->setIcon(QIcon(":/Resources/img/tolerant.png"));
	m_tolerantBt->setText(tr("ignore"));
	m_tolerantBt->setToolTip(tr("ignore"));
	m_tolerantBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(m_tolerantBt);
	connect(m_tolerantBt, &QToolButton::clicked, this, &CompareWin::slot_tolerantBt);

	ui.mainToolBar->addSeparator();

	QToolButton* undoBt = new QToolButton(ui.mainToolBar);
	undoBt->setFixedSize(48, 48);
	undoBt->setIcon(QIcon(":/Resources/img/undo.png"));
	undoBt->setText(tr("undo"));
	undoBt->setToolTip(tr("undo Alt+Z"));
	undoBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	undoBt->setShortcut(QKeySequence("Alt+Z")); //CTRL+Z存在冲突
	ui.mainToolBar->addWidget(undoBt);
	connect(undoBt, &QToolButton::clicked, this, &CompareWin::slot_undoBt);

	ui.mainToolBar->addSeparator();

	QToolButton* preBt = new QToolButton(ui.mainToolBar);
	preBt->setFixedSize(56, 48);
	preBt->setIcon(QIcon(":/Resources/img/pre2.png"));
	preBt->setText(tr("pre"));
	preBt->setToolTip(tr("pre (F3)"));
	preBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	preBt->setShortcut(QKeySequence(Qt::Key_F3));
	ui.mainToolBar->addWidget(preBt);
	connect(preBt, &QToolButton::clicked, this, &CompareWin::slot_preBt);

	QToolButton* nextBt = new QToolButton(ui.mainToolBar);
	nextBt->setFixedSize(56, 48);
	nextBt->setIcon(QIcon(":/Resources/img/next2.png"));
	nextBt->setText(tr("next"));
	nextBt->setToolTip(tr("next (F4)"));
	nextBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	nextBt->setShortcut(QKeySequence(Qt::Key_F4));
	ui.mainToolBar->addWidget(nextBt);
	connect(nextBt, &QToolButton::clicked, this, &CompareWin::slot_nextBt);

	ui.mainToolBar->addSeparator();

	QToolButton* zoominBt = new QToolButton(ui.mainToolBar);
	connect(zoominBt, &QAbstractButton::clicked, this, &CompareWin::slot_zoomin);
	zoominBt->setFixedSize(48, 48);
	zoominBt->setIcon(QIcon(":/Resources/img/zoomin.png"));
	zoominBt->setText(tr("zoomin"));
	zoominBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(zoominBt);


	QToolButton* zoomoutBt = new QToolButton(ui.mainToolBar);
	connect(zoomoutBt, &QAbstractButton::clicked, this, &CompareWin::slot_zoomout);
	zoomoutBt->setFixedSize(48, 48);
	zoomoutBt->setIcon(QIcon(":/Resources/img/zoomout.png"));
	zoomoutBt->setText(tr("zoomout"));
	zoomoutBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(zoomoutBt);


	ui.mainToolBar->addSeparator();

	QToolButton* clearBt = new QToolButton(ui.mainToolBar);
	clearBt->setFixedSize(48, 48);
	clearBt->setIcon(QIcon(":/Resources/img/clear.png"));
	clearBt->setText(tr("clear"));
	clearBt->setToolTip(tr("clear current compare"));
	clearBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(clearBt);
	connect(clearBt, &QToolButton::clicked, this, &CompareWin::slot_clearBt);

	QToolButton* swapBt = new QToolButton(ui.mainToolBar);
	swapBt->setFixedSize(48, 48);
	swapBt->setIcon(QIcon(":/Resources/img/swap.png"));
	swapBt->setText(tr("swap"));
	swapBt->setToolTip(tr("swap left Right windows"));
	swapBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(swapBt);
	connect(swapBt, &QToolButton::clicked, this, &CompareWin::slot_swapBt);

	QToolButton* reloadBt = new QToolButton(ui.mainToolBar);
	reloadBt->setFixedSize(56, 48);
	reloadBt->setIcon(QIcon(":/Resources/img/reload.png"));
	reloadBt->setText(tr("refresh"));
	reloadBt->setToolTip(tr("compare again (F5)"));
	reloadBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	reloadBt->setShortcut(QKeySequence(Qt::Key_F5));
	ui.mainToolBar->addWidget(reloadBt);
	connect(reloadBt, &QToolButton::clicked, this, &CompareWin::slot_reloadBt);

	ui.mainToolBar->addSeparator();

	QToolButton* diffViewBt = new QToolButton(ui.mainToolBar);
	diffViewBt->setFixedSize(48, 48);
	diffViewBt->setIcon(QIcon(":/Resources/img/diffall.png"));
	diffViewBt->setText(tr("status"));
	diffViewBt->setToolTip(tr("show diff views"));
	diffViewBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(diffViewBt);
	connect(diffViewBt, &QToolButton::clicked, this, &CompareWin::slot_diffViewBt);

	m_leftNoEqualBlocks.clear();
	m_rightNoEqualBlocks.clear();

	m_isLeftDirty = false;
	m_isRightDirty = false;

	//设置字体
#if defined (Q_OS_WIN)
	QFont font("Courier New", CompareWin::s_defaultFontSize, QFont::Normal);
#elif defined(Q_OS_MAC)
    QFont font("Courier New", 12, QFont::Normal);
#else
    QFont font("Courier", 12, QFont::Normal);
#endif
	ui.leftSrc->setMarginsFont(font);
	ui.rightSrc->setMarginsFont(font);

	ui.leftSrc->setFont(font);
	ui.rightSrc->setFont(font);

	QFontMetrics fontmetrics = QFontMetrics(font);

	//设置左侧行号栏宽度等
	ui.leftSrc->setMarginWidth(MARGIN_LINE_NUM, fontmetrics.width("0000"));
	ui.leftSrc->setMarginLineNumbers(MARGIN_LINE_NUM, true);
	ui.leftSrc->setBraceMatching(QsciScintilla::SloppyBraceMatch);
	ui.leftSrc->setTabWidth(4);

	ui.rightSrc->setMarginWidth(MARGIN_LINE_NUM, fontmetrics.width("0000"));
	ui.rightSrc->setMarginLineNumbers(MARGIN_LINE_NUM, true);
	ui.rightSrc->setBraceMatching(QsciScintilla::SloppyBraceMatch);
	ui.rightSrc->setTabWidth(4);

	ui.leftSrc->SendScintilla(STYLE_LINENUMBER);
	ui.rightSrc->SendScintilla(STYLE_LINENUMBER);

	ui.leftSrc->setMarginWidth(MARGIN_SYNC_BT, fontmetrics.width("000"));
	ui.rightSrc->setMarginWidth(MARGIN_SYNC_BT, fontmetrics.width("000"));
	ui.leftSrc->setMarginType(MARGIN_SYNC_BT, QsciScintilla::SymbolMargin);
	ui.rightSrc->setMarginType(MARGIN_SYNC_BT, QsciScintilla::SymbolMargin);


	m_leftPix = new QPixmap(QString(":/Resources/img/right3.png"));
	m_rightPix = new QPixmap(QString(":/Resources/img/left3.png"));

	ui.leftSrc->markerDefine(*m_leftPix, MARGIN_SYNC_BT);
	ui.rightSrc->markerDefine(*m_rightPix, MARGIN_SYNC_BT);

	ui.leftSrc->setMarginSensitivity(MARGIN_SYNC_BT, true);
	ui.rightSrc->setMarginSensitivity(MARGIN_SYNC_BT, true);

	ui.leftSrc->markerDefine(QsciScintilla::VerticalLine, MARGIN_VER_LINE);
	ui.rightSrc->markerDefine(QsciScintilla::VerticalLine, MARGIN_VER_LINE);


	ui.leftSrc->SendScintilla(SCI_MARKERSETBACK, MARGIN_VER_LINE, MARGIN_VER_LINE_COLOR_VALUE);
	ui.rightSrc->SendScintilla(SCI_MARKERSETBACK, MARGIN_VER_LINE, MARGIN_VER_LINE_COLOR_VALUE);


	//ui.leftSrc->setMarginsBackgroundColor(QColor(0xf0f0f0));
	//ui.rightSrc->setMarginsBackgroundColor(QColor(0xf0f0f0));

	ui.leftSrc->markerDefine(SC_MARK_AVAILABLE, STYLE_PAD_BACK_PIXMAP);
	ui.rightSrc->markerDefine(SC_MARK_AVAILABLE, STYLE_PAD_BACK_PIXMAP);

	ui.leftSrc->SendScintilla(SCI_MARKERDEFINEPIXMAP_USER, STYLE_PAD_BACK_PIXMAP,  Qt::BDiagPattern);
	ui.rightSrc->SendScintilla(SCI_MARKERDEFINEPIXMAP_USER, STYLE_PAD_BACK_PIXMAP, Qt::BDiagPattern);

	ui.leftSrc->setMarginBackgroundColor(STYLE_PAD_BACK_PIXMAP, QColor(0xffffff));
	ui.rightSrc->setMarginBackgroundColor(STYLE_PAD_BACK_PIXMAP, QColor(0xffffff));

	ui.leftSrc->markerDefine(SC_MARK_AVAILABLE, STYLE_LAST_PAD_LINE);
	ui.rightSrc->markerDefine(SC_MARK_AVAILABLE, STYLE_LAST_PAD_LINE);

	ui.leftSrc->SendScintilla(SCI_MARKERDEFINEPIXMAP_USER, STYLE_LAST_PAD_LINE, Qt::DiagCrossPattern);
	ui.rightSrc->SendScintilla(SCI_MARKERDEFINEPIXMAP_USER, STYLE_LAST_PAD_LINE, Qt::DiagCrossPattern);


	ui.leftSrc->markerDefine(QsciScintilla::Background, STYLE_BACK_COLOUR_NO_EQUAL);
	ui.rightSrc->markerDefine(QsciScintilla::Background, STYLE_BACK_COLOUR_NO_EQUAL);

	//ui.leftSrc->SendScintilla(SCI_MARKERSETBACK, STYLE_BACK_COLOUR_NO_EQUAL, STYLE_NO_EQUAL_BACK_COLOR_VALUE);
	//ui.rightSrc->SendScintilla(SCI_MARKERSETBACK, STYLE_BACK_COLOUR_NO_EQUAL, STYLE_NO_EQUAL_BACK_COLOR_VALUE);

	//20240426 发现一个坑，如果不给STYLE_NO_EQUAL_BACK_COLOR_VALUE_QCOLOR 的最后一个Alpha值，则默认是255，有时候会漏掉背景值的绘制
	//QColor STYLE_NO_EQUAL_BACK_COLOR_VALUE_QCOLOR(0xff,0xe3,0xe3,0x40);255,160,122
	QColor STYLE_NO_EQUAL_BACK_COLOR_VALUE_QCOLOR(255, 160, 122, 0x20);

	ui.leftSrc->setMarkerBackgroundColor(STYLE_NO_EQUAL_BACK_COLOR_VALUE_QCOLOR, STYLE_BACK_COLOUR_NO_EQUAL);
	ui.rightSrc->setMarkerBackgroundColor(STYLE_NO_EQUAL_BACK_COLOR_VALUE_QCOLOR, STYLE_BACK_COLOUR_NO_EQUAL);


	ui.leftSrc->markerDefine(QsciScintilla::Background, STYLE_BACK_COLOUR_EQUAL);
	ui.rightSrc->markerDefine(QsciScintilla::Background, STYLE_BACK_COLOUR_EQUAL);

	ui.leftSrc->SendScintilla(SCI_MARKERSETBACK, STYLE_BACK_COLOUR_EQUAL, STYLE_EQUAL_BACK_COLOR_VALUE);
	ui.rightSrc->SendScintilla(SCI_MARKERSETBACK, STYLE_BACK_COLOUR_EQUAL, STYLE_EQUAL_BACK_COLOR_VALUE);


	connect(ui.leftSrc, &QsciScintilla::marginClicked, this, &CompareWin::slot_leftMarginClicked);
	connect(ui.rightSrc, &QsciScintilla::marginClicked, this, &CompareWin::slot_rightMarginClicked);

	//设置不同部分的前景颜色
	ui.leftSrc->SendScintilla(SCI_STYLESETFORE, STYLE_COLOUR_RED, STYLE_NO_EQUAL_FORE_COLOR_VALUE);
	ui.rightSrc->SendScintilla(SCI_STYLESETFORE, STYLE_COLOUR_RED, STYLE_NO_EQUAL_FORE_COLOR_VALUE);


	ui.leftDiffView->SendScintilla(SCI_STYLESETFORE, STYLE_COLOUR_RED, STYLE_NO_EQUAL_FORE_COLOR_VALUE);
	ui.rightDiffView->SendScintilla(SCI_STYLESETFORE, STYLE_COLOUR_RED, STYLE_NO_EQUAL_FORE_COLOR_VALUE); 

	//下面单行换行的对比窗口，对于不同的字符，设置一个背景风格。
	ui.leftDiffView->SendScintilla(SCI_STYLESETBACK, STYLE_COLOUR_RED, 0xbfffff);
	ui.rightDiffView->SendScintilla(SCI_STYLESETBACK, STYLE_COLOUR_RED, 0xbfffff);

	ui.leftSrc->SendScintilla(SCI_STYLESETFORE, STYLE_EQUAL_TEXT_COLOUR, STYLE_EQUAL_TEXT_COLOUR_VALUE);
	ui.rightSrc->SendScintilla(SCI_STYLESETFORE, STYLE_EQUAL_TEXT_COLOUR, STYLE_EQUAL_TEXT_COLOUR_VALUE);

	//先禁止重做,要崩溃。重做禁止后，redou也没有了。这里禁止的是编辑器的undo。
	//因为左右联动，所以undo只能由菜单同一管理。但是crtl+z是使用不了了。只能换其它快捷键
	int unDoKeyDefinition0 = 'Z' + (SCMOD_CTRL << 16);

	int unDoKeyDefinition1 = SCK_BACK + (SCMOD_ALT << 16);

	ui.leftSrc->SendScintilla(SCI_ASSIGNCMDKEY, unDoKeyDefinition0, SCI_NULL);
	ui.rightSrc->SendScintilla(SCI_ASSIGNCMDKEY, unDoKeyDefinition0, SCI_NULL);

	ui.leftSrc->SendScintilla(SCI_ASSIGNCMDKEY, unDoKeyDefinition1, SCI_NULL);
	ui.rightSrc->SendScintilla(SCI_ASSIGNCMDKEY, unDoKeyDefinition1, SCI_NULL);


	ui.leftSrc->setIsShowFindItem(false);
	ui.rightSrc->setIsShowFindItem(false);

	connect(ui.leftSrc, &QsciDisplayWindow::sign_find, this, &CompareWin::slot_findFile, Qt::QueuedConnection);
	connect(ui.rightSrc, &QsciDisplayWindow::sign_find, this, &CompareWin::slot_findFile, Qt::QueuedConnection);

	connect(ui.leftSrc, &QsciDisplayWindow::sign_saveAsFile, this, &CompareWin::slot_saveAsFile, Qt::QueuedConnection);
	connect(ui.rightSrc, &QsciDisplayWindow::sign_saveAsFile, this, &CompareWin::slot_saveAsFile, Qt::QueuedConnection);

	connect(ui.leftSrc, &QsciDisplayWindow::sign_pullOpen, this, &CompareWin::on_pullOpenLeft, Qt::QueuedConnection);
	connect(ui.rightSrc, &QsciDisplayWindow::sign_pullOpen, this, &CompareWin::on_pullOpenRight, Qt::QueuedConnection);


	//设置查找的快捷键
	QAction* findAction = new QAction(this);
	QKeySequence findKey = QKeySequence(Qt::CTRL | Qt::Key_F);
	findAction->setShortcut(findKey);

	connect(findAction, &QAction::triggered, this, &CompareWin::slot_findFile);
	this->addAction(findAction);

	QAction* gotoLineAction = new QAction(this);
	QKeySequence goKey = QKeySequence(Qt::CTRL | Qt::Key_G);
	gotoLineAction->setShortcut(goKey);

	connect(gotoLineAction, &QAction::triggered, this, &CompareWin::slot_gotoLine);
	this->addAction(gotoLineAction);


	//////设置光标所在行背景色
	ui.leftSrc->setCaretLineVisible(true);
	//ui.leftSrc->setCaretLineBackgroundColor(QColor(0xFAF9DE));

	ui.rightSrc->setCaretLineVisible(true);
	//ui.rightSrc->setCaretLineBackgroundColor(QColor(0xFAF9DE));


	//设置查找的快捷键
	QAction* saveAction = new QAction(this);
	QKeySequence saveKey = QKeySequence(Qt::CTRL | Qt::Key_S);
	saveAction->setShortcut(saveKey);

	connect(saveAction, &QAction::triggered, this, &CompareWin::slot_saveFile);
	this->addAction(saveAction);

	m_compareWorker = new CompareWorker;

	m_compareWorker->moveToThread(&m_workerThread);

	//m_compareWorker已经到新线程中去了，不应该再放在主线程销毁。所以这样教科书的销毁是比较好的。
	connect(&m_workerThread, &QThread::finished, m_compareWorker, &QObject::deleteLater);

	connect(this, &CompareWin::sendModify, m_compareWorker, &CompareWorker::doCompareWork);

	connect(m_compareWorker, &CompareWorker::startCompare, this, &CompareWin::slot_doCmpAfterEdit);

	m_workerThread.start();

	//只有在内存模式才监控
	if (m_workMode == 1)
	{
		enableSignTextChange(true);
	}

	//设置当前视图的放大值。
	m_zoomValue = CmpSql::getKeyValueFromNumSets(CMP_ZOOM_VALUE, 100);

	if (m_zoomValue != 100)
	{
		//100 对应 0 ；300 对应 20。因为要减去100的基数
		if (m_zoomValue > 300)
		{
			m_zoomValue = 300;
		}
		if (m_zoomValue < 80)
		{
			m_zoomValue = 100;
		}

		int curValue = (m_zoomValue - 100) / 10;

		ui.leftSrc->zoomTo(curValue);
		ui.rightSrc->zoomTo(curValue);

		ui.leftSrc->updateLineNumberWidth();
		ui.rightSrc->updateLineNumberWidth();
	}


	ui.statusBar->showMessage(tr("Drag file support ..."));

	ui.leftSrc->setCopyNeedReplace(true);
	ui.rightSrc->setCopyNeedReplace(true);
}


CompareWin::~CompareWin()
{
	if (!m_isCmpFinished)
	{
		m_isCancel = true;
	}

#ifdef OPEN_UNDO_REDO

	for (auto var : m_undoList)
	{
		delete var;
	}
	m_undoList.clear();
#endif

	while (!m_isCmpFinished)
	{
		QCoreApplication::processEvents();
	}

	if (blockCompare != nullptr)
	{
		delete blockCompare;
		blockCompare = nullptr;
	}

	if (m_mediator != nullptr)
	{
		delete m_mediator;
		m_mediator = nullptr;
	}

	if (m_pCmpMode != nullptr)
	{
		delete m_pCmpMode;
	}

	m_workerThread.quit();
	m_workerThread.wait();

	m_isCmpFinished = true;

	if (m_srcCmpMode != m_cmpMode)
	{
		//修改了m_cmpMode，重新更新一下。
		CmpSql::updataKeyValueFromNumSets("cmpmode", m_cmpMode);
	}
}

void CompareWin::on_leftPathReturnPressed()
{
	//有可能手动修改了文件名，见gitee用户反馈，此时要更新，否则保存到原来文件中去了。
	QString leftPath = ui.leftPath->text();

	QFileInfo fi(leftPath);

	if (fi.exists())
	{
		if (ui.leftSrc->getFilePath() != leftPath)
		{
			ui.leftSrc->setFilePath(leftPath);
		}
	}

	slot_doCmp();
}

void CompareWin::on_rightPathReturnPressed()
{
	QString rightPath = ui.rightPath->text();

	QFileInfo fi(rightPath);

	if (fi.exists())
	{
		if (ui.rightSrc->getFilePath() != rightPath)
		{
			ui.rightSrc->setFilePath(rightPath);
		}
	}

	slot_doCmp();
}


void CompareWin::syncNeedSaveIcon()
{
	if (m_isLeftDirty)
	{
			ui.leftSaveBt->setIcon(QIcon(NEEDSAVEDARK));
	}
	else
	{
			ui.leftSaveBt->setIcon(QIcon(SAVEDARK));
	}

	if (m_isRightDirty)
	{
			ui.rightSaveBt->setIcon(QIcon(NEEDSAVEDARK));
	}
	else
	{
			ui.rightSaveBt->setIcon(QIcon(SAVEDARK));
	}
}



//自动根据鼠标当前在那边进行保存
void CompareWin::slot_saveFile()
{
	//在左边
	if (!ui.leftSrc->geometry().contains(ui.leftChildWidget->mapFromGlobal(QCursor::pos())))
	{
		slot_saveRightFile();
	}
	else
	{
		slot_saveLeftFile();
	}
}

//isOn true:开启信号，进入内存模式 false 关闭文本改变信号检测
//返回原来的模式。外面可以根原来模式和isOn是否相同，判断是否发生了改变
bool CompareWin::enableSignTextChange(bool isOn)
{
	bool oldStaus = m_isTextChangeSignalOnMemoryMode;

	//开启，内存模式才能开启
	if (isOn && !m_isTextChangeSignalOnMemoryMode)
	{
		//这里必须是队列模式，否则slot_textChanged触发时，文本压根没有进入到编辑框中。
		//注意：这里的QueuedConnection是必须要加的，不能不加，否则会出现编辑框无法删除干净，拷贝内容后文本丢失现象。
		//20220425是因为在qscint中的槽函数中会出发textChanged，如果不是队列连接，则直接执行slot_textChanged导致状态不一样，删除不干净
		//而队列连接则不在qscint中执行，延后执行没有问题
		connect(ui.leftSrc, &QsciDisplayWindow::textChanged, this, &CompareWin::slot_textChanged, Qt::QueuedConnection);
		connect(ui.rightSrc, &QsciDisplayWindow::textChanged, this, &CompareWin::slot_textChanged, Qt::QueuedConnection);

		m_isTextChangeSignalOnMemoryMode = true;
	}

	//关闭模式。
	if (!isOn && m_isTextChangeSignalOnMemoryMode)
	{
		//m_workMode = 0;//必须进入文件模式
		/*ui.leftSrc->disconnect(SIGNAL(textChanged()));
		ui.rightSrc->disconnect(SIGNAL(textChanged()));*/

		disconnect(ui.leftSrc, &QsciDisplayWindow::textChanged, this, &CompareWin::slot_textChanged/*, Qt::QueuedConnection*/);
		disconnect(ui.rightSrc, &QsciDisplayWindow::textChanged, this, &CompareWin::slot_textChanged/*, Qt::QueuedConnection*/);

		m_isTextChangeSignalOnMemoryMode = false;
	}

	return oldStaus;
}

bool CompareWin::isNeedMemoryCmp()
{
	//至少有1个文件是空，或者都空
	if (ui.leftPath->text().isEmpty() || ui.rightPath->text().isEmpty())
	{
		if (!ui.leftSrc->text().isEmpty() && !ui.rightSrc->text().isEmpty())
		{
			return true;
		}
	}
	return false;
}

//只在非文件对比下才使用。
void CompareWin::slot_textChanged()
{
	//至少有1个文件是空，或者都空
	//if (ui.leftPath->text().isEmpty() || ui.rightPath->text().isEmpty())
	//{
	//	if (!ui.leftSrc->text().isEmpty() && !ui.rightSrc->text().isEmpty())
	//	{
	//		slot_doMemoryCmp();
	//	}
	//}
	if (isNeedMemoryCmp())
	{
		slot_doMemoryCmp();
	}
}

//开始做内存比较
void CompareWin::slot_doMemoryCmp()
{
	QString leftPath = ui.leftPath->text();
	QString rightPath = ui.rightPath->text();

	if ((!leftPath.isEmpty() && !rightPath.isEmpty()) || (m_workMode != 1))
	{
		return;
	}

	if (m_workStatus != FREE_STATUS)
	{
		//已经在比较中，不能重入
		ui.statusBar->showMessage(tr("Comparison in progress, please wait ..."));
		return;
	}

	QString leftText = ui.leftSrc->text();
	QString rightText = ui.rightSrc->text();

	if (leftText.isEmpty() || rightText.isEmpty())
	{
		return;
	}
	if (m_workStatus == FREE_STATUS)
	{
		m_workStatus = CMP_WORKING;
	}

	//这里必须关闭，因为已经进入对比模式，如果不关闭，将引发连锁改变触发
	enableSignTextChange(false);
	disableTextChangeSignal();

	std::string leftStr = leftText.toStdString();
	std::string rightStr = rightText.toStdString();

	++s_useTimes;

	//此时务必要清空文本。否则会干扰对比结果的输出
	ui.leftSrc->clear();
	ui.rightSrc->clear();

	setAtEditStatus(true);


	char* pLeft = new char[leftStr.length()+1];
	char* pRight = new char[rightStr.length()+1];

	memcpy(pLeft, leftStr.data(), leftStr.length());
	memcpy(pRight, rightStr.data(), rightStr.length());

	//如果左右文件都存在，而且都是文件，则开始比较

	QFutureWatcher<ThreadFileCmpParameter*> *futureWatcher = new QFutureWatcher<ThreadFileCmpParameter*>();

	QObject::connect(futureWatcher, &QFutureWatcher<ThreadFileCmpParameter*>::finished, this, &CompareWin::slot_fileCompareFinish);

	//使用异步的线程是做耗时的比较工作。就是把之前下面的work比较工作，移动到异步线程中去处理

	ThreadFileCmpParameter * threadParater = new ThreadFileCmpParameter((uchar*)pLeft, leftStr.length(), (uchar*)pRight, rightStr.length());

	futureWatcher->setFuture(CmpareMode::commitAsyncTask([&](ThreadFileCmpParameter *parameter)->ThreadFileCmpParameter *
		{
			CmpareMode * pCmpMode = new CmpareMode(parameter->leftText, parameter->leftLens, parameter->rightText, parameter->rightLens);
			pCmpMode->setCmpMode(m_cmpMode);
			pCmpMode->setCmpTextCode(m_leftCode, m_rightCode);

			//这里pCmpMode的释放，是在后续clearStatus中释放的
			parameter->resultCmpObj = pCmpMode;
			pCmpMode->work(blockCompare);

			return parameter;
		}, threadParater));
}

//文件另存为
void CompareWin::slot_saveAsFile()
{
	QsciDisplayWindow* src = dynamic_cast<QsciDisplayWindow*>(sender());
	if (src == nullptr || m_leftExternBlockInfo == nullptr || m_rightExternBlockInfo == nullptr)
	{
		ui.statusBar->showMessage(tr("Can not save file !"),8000);
		return;
	}

	RC_DIRECTION direction = ((src == ui.leftSrc) ? RC_LEFT : RC_RIGHT);

	CODE_ID code = UNKOWN;

	QList<BlockUserData*>* externLinesInfo = nullptr;

	QString filter("Text files (*.txt);;All types(*.*)");
	QString filePath = QFileDialog::getSaveFileName(this, tr("Save File As ..."), QString(), filter);

	if (filePath.isEmpty())
	{
		//这里点击了取消，不进行保存
		return;
	}

	if (direction == RC_LEFT)
	{
		externLinesInfo = m_leftExternBlockInfo;
		src = ui.leftSrc;
		code = m_leftCode;
	}
	else
	{
		externLinesInfo = m_rightExternBlockInfo;
		src = ui.rightSrc;
		code = m_rightCode;
	}

	auto clearFileContents = [](QFile& file, QString& filepath)
	{
		if (file.isOpen())
		{
			if (file.size() > 0)
			{
				file.close();
				file.remove();
				file.setFileName(filepath);
				file.open(QIODevice::ReadWrite);
			}
		}
		else
		{
			file.remove();
			file.setFileName(filepath);
			file.open(QIODevice::ReadWrite);
		}
	};

	src->setReadOnly(true);

	QFile file(filePath);

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		QMessageBox::warning(this, tr("Error"), tr("open file %1 failed").arg(filePath));
		return;
	}

	bool isFirstLine = true;

	int firstLineEndType = UNKNOWN_LINE;

	QString text;
	QTextCodec* pTranCode = nullptr;

	if (externLinesInfo != nullptr)
	{
		src->travEveryBlockToSave([&](QString &src, int tailStatus) {

			text.append(src);

			//如果是第一行，则要考虑文本字符编码问题，处理是否是UTF-BOM等格式
			if (isFirstLine && (code != UNKOWN))
			{
				isFirstLine = false;

				QString codecDstName = Encode::getQtCodecNameById(code);
				if (codecDstName.isEmpty() || codecDstName == "unknown")
				{
					//这里是不严谨的，但是目前只能这样处理
					pTranCode = QTextCodec::codecForName("UTF-8");
				}
				else
				{
					pTranCode = QTextCodec::codecForName(codecDstName.toStdString().c_str());
				}


				if (code == CODE_ID::UTF8_BOM)
				{
					//UTF_BOM的不会自动在前面加上头，所以要单独写
					QByteArray codeFlag = Encode::getEncodeStartFlagByte(code);
					if (!codeFlag.isEmpty())
					{
						//先写入标识头
						file.write(codeFlag);
					}
				}

				firstLineEndType = tailStatus;
			}

			/* 如果是未知行尾，则以第一行的行号为准;当编辑后目前行尾可能丢失，则使用第一行的行号; */
			if (tailStatus == UNKNOWN_LINE)
			{
				tailStatus = firstLineEndType;
			}

			if (tailStatus == UNIX_LINE)
			{
				text.append("\n");
			}
			else if (tailStatus == DOS_LINE)
			{
				text.append("\r\n");
			}
			else if (tailStatus == MAC_LINE)
			{
				text.append("\r");
			}
			else if (tailStatus == UNKNOWN_LINE)
			{
				//最后如果实在不知道行尾换行号码，直接使用默认值
				text.append("\r\n");
			}
		}, externLinesInfo);
	}
	else
	{
		//这种就发生在编辑框中直接对比的时候
		pTranCode = QTextCodec::codecForName("UTF-8");
		text = src->text();
	}

	if (text.length() > 0)
	{
		//保存时注意编码问题。在调用toLocal8Bit时，除了UTF-BOM以外，其他都会自动在前面加上头。
		QByteArray t = pTranCode->fromUnicode(text);
		file.write(t);
	}

	file.close();

	src->setReadOnly(false);

	ui.statusBar->showMessage(tr("save file finished !"), MSG_SHOW_TIME);
}

void CompareWin::slot_findFile(bool)
{
	RC_DIRECTION dir = RC_LEFT;

	//在左边
	if (!ui.leftSrc->geometry().contains(ui.leftChildWidget->mapFromGlobal(QCursor::pos())))
	{
		dir = RC_RIGHT;
	}
	initFindWindow(dir);
	FindCmpWin* pFind = dynamic_cast<FindCmpWin*>(m_pFindWin.data());
	pFind->setCurrentTab(FIND_TAB);
	pFind->activateWindow();
    pFind->raise();
	pFind->show();
}

void CompareWin::slot_searchDireChange(RC_DIRECTION dir)
{
	if (!m_pFindWin.isNull())
	{
		FindCmpWin* pFind = dynamic_cast<FindCmpWin*>(m_pFindWin.data());

		QsciDisplayWindow *pEdit = nullptr;

		if (m_leftRightOrder == 0)
		{
			pEdit = ((dir == RC_LEFT) ? ui.leftSrc : ui.rightSrc);
		}
		else
		{
			pEdit = ((dir == RC_LEFT) ? ui.rightSrc : ui.leftSrc);
		}
		pFind->setWorkEdit(pEdit);
	}
}

void CompareWin::initFindWindow(RC_DIRECTION dir)
{
	FindCmpWin* pFind = nullptr;

	if (m_pFindWin.isNull())
	{
		m_pFindWin = new FindCmpWin(dir, this);
		pFind = dynamic_cast<FindCmpWin*>(m_pFindWin.data());
		pFind->setAttribute(Qt::WA_DeleteOnClose);
		pFind->setFindHistory(&m_findHistroy);
		connect(pFind, &FindCmpWin::sgin_searchDirectionChange, this, &CompareWin::slot_searchDireChange);
	}
	else
	{
		pFind = dynamic_cast<FindCmpWin*>(m_pFindWin.data());
	}

	QsciDisplayWindow *pEdit = ((dir == RC_LEFT) ? ui.leftSrc : ui.rightSrc);
	pFind->setWorkEdit(pEdit);
	if (pEdit->hasSelectedText())
	{
		QString text = pEdit->selectedText();
		pFind->setFindText(text);
	}
}

void CompareWin::slot_gotoLine()
{
	GoToLineWin* p = new GoToLineWin(nullptr);
	p->setAttribute(Qt::WA_DeleteOnClose);
	connect(p,&GoToLineWin::sign_gotoLine,this,&CompareWin::slot_gotoEditLine);
	p->show();
}

void CompareWin::slot_gotoEditLine(int dire, int lineNum)
{

	if (((dire == 0)&&(m_leftRightOrder == 0))||((dire == 1) && (m_leftRightOrder == 1)))
	{
			ui.leftSrc->setCursorPosition(lineNum, 1);
			ui.leftSrc->setFocus();
	}
	else
	{

		ui.rightSrc->setCursorPosition(lineNum, 1);
		ui.rightSrc->setFocus();
	}

	gotoViewLine(lineNum);

	updateDiffWinCurView();

	ui.statusBar->showMessage(tr("goto line nusm %1 finish!").arg(lineNum+1), 5000);
}

RC_DIRECTION CompareWin::getUiRealDirection(RC_DIRECTION type)
{
	if (m_leftRightOrder == 0)
	{
		return type;
	}
	else if (m_leftRightOrder == 1)
	{
		if (type == RC_LEFT)
		{
			return RC_RIGHT;
		}
		else
		{
			return RC_LEFT;
		}
	}

	return type;
}

//对齐操作。这个不是对齐，而是拉开。发现拉开要比对齐更实用。名称不方便修改了。
void CompareWin::slot_pullopenBt()
{
	AlignWin* p = new AlignWin();

	//获取光标所在行的行号
	int line = -1;
	int index = 0;
	ui.leftSrc->getCursorPosition(&line, &index);

	if (line != -1)
	{
		p->setStartLine(line + 1);
	}

	p->setAttribute(Qt::WA_DeleteOnClose);
	connect(p, &AlignWin::alignLine, this, &CompareWin::slot_alignWork);
	p->show();
}

void CompareWin::on_pullOpenLeft()
{
	doPullOpen(0);
}

void CompareWin::doPullOpen(int dire)
{
	QsciDisplayWindow* srcDisWin = ((0 == dire) ? ui.leftSrc : ui.rightSrc);

	int lineFrom;
	int indexFrom;
	int lineTo;
	int indexTo;

	srcDisWin->getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);

	if (lineFrom == -1)
	{
		srcDisWin->getCursorPosition(&lineFrom, &indexFrom);
		lineTo = lineFrom;
	}

	slot_alignWork(dire, lineFrom, lineTo);
}

void CompareWin::on_pullOpenRight()
{
	doPullOpen(1);
}

//获取某一行的实际内容。//返回的是长度
int getOneLineText(QsciDisplayWindow * src, int lineNum, LineFileInfo& leftInfo, QByteArray & outputBytes)
{
	//如果行是有内容的，而不是对齐行那种，则实际取其值
	int curLineLength = src->SendScintilla(SCI_LINELENGTH, lineNum);

	char* contens = new char[curLineLength + 1];
	memset(contens, 0, curLineLength + 1);

	src->SendScintilla(SCI_GETLINE, lineNum, contens);

	leftInfo.lineNums = lineNum;
	leftInfo.unicodeStr = QString(contens);
	leftInfo.isEmptyLine = (strlen(contens) == 0);
	leftInfo.lineEndFormat = getLineEndType(leftInfo.unicodeStr);

	outputBytes.append(contens, curLineLength);

	delete[]contens;

	return curLineLength;
}

//把老的拉开逻辑备份一下，下面的新函数，进行了重构。
#if 0
//进行对齐操作。拉开操作，拉开也是对齐的一种方式。
void CompareWin::slot_alignWork(int type, int leftStart, int leftEnd)
{
    int lineNum = ui.leftSrc->lines();
    if(leftEnd >= lineNum)
    {
        leftEnd = lineNum - 1;
    }

	//type == 0 。简单将内容拎出来，右边使用对齐处理，不和任何行对比
	bool old = enableSignTextChange(false);
	disableTextChangeSignal();

	QList<LineFileInfo> leftLineInfo;
	QList<LineFileInfo> rightLineInfo;

#ifdef OPEN_UNDO_REDO
	UserDataCommand* uc = new UserDataCommand(this);

	//单独拿出来，是因为要控制他们加入uc里面的顺序
	UserDataOperRecords* leftDelUserDataRecord = nullptr;
	UserDataOperRecords* leftAddUserDataRecord = nullptr;
	UserDataOperRecords* rightDelUserDataRecord = nullptr;
	UserDataOperRecords* rightAddUserDataRecord = nullptr;
#endif

	QByteArray leftDelContents;
	leftDelContents.reserve(5120);

	int leftDelTextLength = 0;

	//获取左边行的内容
	for (int i = leftStart; i <= leftEnd; ++i)
	{
		LineFileInfo lineText;
		leftDelTextLength += getOneLineText(ui.leftSrc, i, lineText, leftDelContents);

		int markMask = ui.leftSrc->SendScintilla(SCI_MARKERGET, i);
		if ((markMask & STYLE_PAD_OR_LASTPAD_BIT_MASK) != 0)
		{
			// 对齐行，直接continue;
			continue;
		}
		leftLineInfo.append(lineText);
	}

	//全部是对齐行，已经是错开的了，直接返回
	if (leftLineInfo.size() == 0)
	{
		delete uc;
		return;
	}

	int rightDelTextLength = 0;
	QByteArray rightDelContents;
	rightDelContents.reserve(5120);

	//获取右边行的内容
	for (int i = leftStart; i <= leftEnd; ++i)
	{
		LineFileInfo lineText;
		rightDelTextLength += getOneLineText(ui.rightSrc, i, lineText, rightDelContents);

		int markMask = ui.rightSrc->SendScintilla(SCI_MARKERGET, i);
		if ((markMask & STYLE_PAD_OR_LASTPAD_BIT_MASK) != 0)
		{
			// 对齐行，直接continue;
			continue;
		}

		rightLineInfo.append(lineText);
	}

	//全部是对齐行，已经是错开的了，直接返回
	if (rightLineInfo.size() == 0)
	{
		delete uc;
		return;
	}

	{
	//1)删除行的格式配置。倒着来，因为要删除，只能从尾部删除
	for (int start = leftEnd; start >= leftStart; --start)
	{
		//只删除格式
		deleteLinesMarker(RC_LEFT, start, 1);

		setLineStyle(ui.leftSrc, STYLE_EQUAL_TEXT_COLOUR, start);
		//setLineStyle(ui.leftSrc, STYLE_BACK_COLOUR_EQUAL, start);
	/*	int startPos = ui.leftSrc->SendScintilla(SCI_POSITIONFROMLINE, start);
		ui.leftSrc->SendScintilla(SCI_STARTSTYLING, startPos);

		int length = ui.leftSrc->SendScintilla(SCI_LINELENGTH, start);
		ui.leftSrc->SendScintilla(SCI_SETSTYLING, length, STYLE_EQUAL_TEXT_COLOUR ); 
		ui.leftSrc->SendScintilla(SCI_SETSTYLING, length, STYLE_BACK_COLOUR_EQUAL);*/
	}


	//4)再插入新的内容。把左边的实际内容收集在一起

	QByteArray leftBytes;
	leftBytes.reserve(1024 * 512);

	for (int i = 0; i < leftLineInfo.size(); ++i)
	{
		const LineFileInfo& data = leftLineInfo.at(i);
		leftBytes.append(data.unicodeStr.toUtf8());
	}
	//再插入换行表示对齐行
	for (int i = 0; i < rightLineInfo.size(); ++i)
	{
		//先不区分换行符号的类型，直接加入\n，后续要区分win mac linux
		leftBytes.append("\n");
	}
	//leftBytes.append('\0');

		//这里其实就是一个内容的替换，先删除、后增加，直接就是一个替换。使用新内容替换原来就内容
#ifdef OPEN_UNDO_REDO
		//把原来的内容收集起来。这里的原来，也是代表2阶段的文本，及修改后当前的文本，而不是原来修改前文本，也不是校正后文本。
		ReplaceOperRecords* replaceRecord = new ReplaceOperRecords();
		replaceRecord->dir = RC_LEFT;

		int startPos = ui.leftSrc->SendScintilla(SCI_POSITIONFROMLINE, leftStart);
		int endPos = ui.leftSrc->SendScintilla(SCI_POSITIONFROMLINE, leftEnd + 1);

		replaceRecord->startPos = startPos;
		replaceRecord->srcEndPos = endPos;

		struct Sci_TextRange delTextRange;

		delTextRange.chrg.cpMin = startPos;
		delTextRange.chrg.cpMax = endPos;

		delTextRange.lpstrText = new char[endPos - startPos + 1];
		memset(delTextRange.lpstrText, 0, endPos - startPos + 1);
		//拷贝原始的内容
		ui.leftSrc->SendScintilla(SCI_GETTEXTRANGE, 0, &delTextRange);
		replaceRecord->srcContents = delTextRange.lpstrText;


		//把替换文本记录在replaceContents，后续撤销需要使用
		replaceRecord->replaceLens = leftBytes.length();

		char* replaceContens = new char[replaceRecord->replaceLens + 1];
		memset(replaceContens, 0, replaceRecord->replaceLens + 1);
		memcpy(replaceContens, leftBytes.data(), replaceRecord->replaceLens);

		replaceRecord->replaceContents = replaceContens;

		ReplaceCommand* ec = new ReplaceCommand(this);
		ec->setChangeStatus(NONE_CHANGE);
		ec->addRecord(replaceRecord);
		ec->setOperIndex(m_commandIndex);

		m_undoList.append(ec);

		replaceText(RC_LEFT, startPos, endPos, leftBytes.length(), leftBytes.data());
#endif


#ifdef OPEN_UNDO_REDO

		//要注意左右任何时候的userdatablock都是要相等的。
		//这里要调整加删的顺序。左边要先删，右边也要先删。
		//后面左边加，右边也要加。不要穿插进行。比如左边先删后加。
		//右边再进行删、加。那么在左边删后，如果不是右边删，就会导致左右不等。20221019。
		//UserDataCommand* uc = new UserDataCommand(this);
		uc->setChangeStatus(NONE_CHANGE);
		uc->setOperIndex(m_commandIndex);

		//巨坑;20210804 这里一定要注意顺序。不能直接append。一定是先记录所有的修改内容后，在记录userdata变化
		//而且userdata在还原时，也是需要最后做的，而不能在前面做。否则乱序导致文本混乱

		//往前面插入,找到该相同index的最前面位置插入
		int insertPos = 0;
		for (insertPos = m_undoList.size() - 1; insertPos >= 0; --insertPos)
		{
			if (m_undoList.at(insertPos)->getOperIndex() != m_commandIndex)
			{
				break;
			}
		}

		m_undoList.insert(insertPos + 1, uc);
		leftDelUserDataRecord = new UserDataOperRecords(RC_LEFT);

		leftDelUserDataRecord->operType = DEL_USER_DATA;

		//uc->addRecord(userDataRecord);

#endif

		//2)再删除行对应的userdata信息，
		for (int start = leftEnd; start >= leftStart; --start)
		{
#ifdef OPEN_UNDO_REDO
			//把删除的格式保存起来
			BlockUserData* data = m_leftExternBlockInfo->at(start);
			m_leftExternBlockInfo->removeAt(start);

			leftDelUserDataRecord->userData.push_front(data);
#else
			//把格式也删除
			BlockUserData* data = m_leftExternBlockInfo->at(start);
			m_leftExternBlockInfo->removeAt(start);
			delete data;
#endif
		}

#ifdef OPEN_UNDO_REDO

		leftAddUserDataRecord = new UserDataOperRecords(RC_LEFT);
		leftAddUserDataRecord->operType = ADD_USER_DATA;
#endif

	//5）再插入新的userdata，主要更新其中的m_srcBlockNum值
	for (int index = 0, size = leftLineInfo.size(); index < size; ++index)
	{
		BlockUserData* v = new BlockUserData(UNEQUAL_BLOCK);
		v->m_srcBlockNum = leftStart + index;
		v->m_blockLen = 1;
		m_leftExternBlockInfo->insert(v->m_srcBlockNum, v);

#ifdef OPEN_UNDO_REDO
		leftAddUserDataRecord->userData.append(new BlockUserData(*v));
#endif
		ui.leftSrc->markerDelete(v->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
		ui.leftSrc->markerAdd(v->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
	}

	for (int index = 0, size = rightLineInfo.size(); index < size; ++index)
	{
		BlockUserData* v = new BlockUserData(PAD_BLOCK);
		v->m_srcBlockNum = leftStart + index + leftLineInfo.size();
		v->m_blockLen = 1;
		m_leftExternBlockInfo->insert(v->m_srcBlockNum, v);

#ifdef OPEN_UNDO_REDO
		leftAddUserDataRecord->userData.append(new BlockUserData(*v));
#endif

		ui.leftSrc->markerAdd(v->m_srcBlockNum, STYLE_PAD_BACK_PIXMAP);
#ifdef _test
		QCoreApplication::processEvents();
#endif
	}

	int oldBlockNum = leftEnd - leftStart + 1;
	int newBlockNum = leftLineInfo.size() + rightLineInfo.size();

	//整体调整后续不等块的偏移行号
	if (oldBlockNum != newBlockNum)
	{
		int interNum = newBlockNum - oldBlockNum;

		for (int start = leftStart + newBlockNum, size = m_leftExternBlockInfo->size(); start < size; ++start)
		{
			m_leftExternBlockInfo->at(start)->m_srcBlockNum += interNum;
		}
	}

		ui.leftSrc->SendScintilla(SCI_GOTOPOS, startPos);
	}

	//现在处理右边

	{
		//删除行的格式配置。倒着来，因为要删除，只能从尾部删除
		for (int start = leftEnd; start >= leftStart; --start)
		{
			//只删除格式
			deleteLinesMarker(RC_RIGHT, start, 1);

		setLineStyle(ui.rightSrc, STYLE_EQUAL_TEXT_COLOUR, start);
		//setLineStyle(ui.rightSrc, STYLE_BACK_COLOUR_EQUAL, start);

		/*int startPos = ui.rightSrc->SendScintilla(SCI_POSITIONFROMLINE, start);
			ui.rightSrc->SendScintilla(SCI_STARTSTYLING, startPos);

			int length = ui.rightSrc->SendScintilla(SCI_LINELENGTH, start);
		ui.rightSrc->SendScintilla(SCI_SETSTYLING, length, STYLE_EQUAL_TEXT_COLOUR );
		ui.rightSrc->SendScintilla(SCI_SETSTYLING, length, STYLE_BACK_COLOUR_EQUAL);*/
		}

		//再插入新的内容。把右边的实际内容收集在一起

		QByteArray rightBytes;
		rightBytes.reserve(1024 * 512);

		//先插入换行表示对齐行
		for (int i = 0; i < leftLineInfo.size(); ++i)
		{
			//先不区分换行符号的类型，直接加入\n，后续要区分win mac linux
			rightBytes.append("\n");
		}
		//在插入原来右边行的内容
		for (int i = 0; i < rightLineInfo.size(); ++i)
		{
			const LineFileInfo& data = rightLineInfo.at(i);
			rightBytes.append(data.unicodeStr.toUtf8());
		}

		//rightBytes.append('\0');

		//这里其实就是一个内容的替换，先删除、后增加，直接就是一个替换。使用新内容替换原来就内容
#ifdef OPEN_UNDO_REDO
	//把原来的内容收集起来。这里的原来，也是代表2阶段的文本，及修改后当前的文本，而不是原来修改前文本，也不是校正后文本。
		ReplaceOperRecords* replaceRecord = new ReplaceOperRecords();
		replaceRecord->dir = RC_RIGHT;

		int startPos = ui.rightSrc->SendScintilla(SCI_POSITIONFROMLINE, leftStart);
		int endPos = ui.rightSrc->SendScintilla(SCI_POSITIONFROMLINE, leftEnd + 1);

		replaceRecord->startPos = startPos;
		replaceRecord->srcEndPos = endPos;

		struct Sci_TextRange delTextRange;

		delTextRange.chrg.cpMin = startPos;
		delTextRange.chrg.cpMax = endPos;

		delTextRange.lpstrText = new char[endPos - startPos + 1];
		memset(delTextRange.lpstrText, 0, endPos - startPos + 1);
		//拷贝原始的内容
		ui.rightSrc->SendScintilla(SCI_GETTEXTRANGE, 0, &delTextRange);
		replaceRecord->srcContents = delTextRange.lpstrText;


		//把替换文本记录在replaceContents，后续撤销需要使用
		replaceRecord->replaceLens = rightBytes.length();

		char* replaceContens = new char[replaceRecord->replaceLens + 1];
		memset(replaceContens, 0, replaceRecord->replaceLens + 1);
		memcpy(replaceContens, rightBytes.data(), replaceRecord->replaceLens);

		replaceRecord->replaceContents = replaceContens;

		ReplaceCommand* ec = new ReplaceCommand(this);
		ec->setChangeStatus(NONE_CHANGE);
		ec->addRecord(replaceRecord);
		ec->setOperIndex(m_commandIndex);

		m_undoList.append(ec);

		replaceText(RC_RIGHT, startPos, endPos, rightBytes.length(), rightBytes.data());
#endif


#ifdef OPEN_UNDO_REDO
		rightDelUserDataRecord = new UserDataOperRecords(RC_RIGHT);
		rightDelUserDataRecord->operType = DEL_USER_DATA;

#endif

		//再删除行对应的userdata信息，
		for (int start = leftEnd; start >= leftStart; --start)
		{
#ifdef OPEN_UNDO_REDO
			//把删除的格式保存起来
			BlockUserData* data = m_rightExternBlockInfo->at(start);
			m_rightExternBlockInfo->removeAt(start);

			rightDelUserDataRecord->userData.push_front(data);
#else
			//把格式也删除
			BlockUserData* data = m_rightExternBlockInfo->at(start);
			m_rightExternBlockInfo->removeAt(start);
			delete data;
#endif
		}

#ifdef OPEN_UNDO_REDO

		rightAddUserDataRecord = new UserDataOperRecords(RC_RIGHT);
		rightAddUserDataRecord->operType = ADD_USER_DATA;

#endif

		//5）再插入新的userdata，主要更新其中的m_srcBlockNum值
		for (int index = 0, size = leftLineInfo.size(); index < size; ++index)
		{
			BlockUserData* v = new BlockUserData(PAD_BLOCK);
			v->m_srcBlockNum = leftStart + index;
			v->m_blockLen = 1;
			m_rightExternBlockInfo->insert(leftStart + index, v);


#ifdef OPEN_UNDO_REDO
			rightAddUserDataRecord->userData.append(new BlockUserData(*v));
#endif

			ui.rightSrc->markerAdd(v->m_srcBlockNum, STYLE_PAD_BACK_PIXMAP);
		}

		for (int index = 0, size = rightLineInfo.size(); index < size; ++index)
		{
			BlockUserData* v = new BlockUserData(UNEQUAL_BLOCK);
			v->m_srcBlockNum = leftStart + index + leftLineInfo.size();
			v->m_blockLen = 1;
			m_rightExternBlockInfo->insert(v->m_srcBlockNum, v);

#ifdef OPEN_UNDO_REDO
			rightAddUserDataRecord->userData.append(new BlockUserData(*v));
#endif
		ui.rightSrc->markerDelete(v->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
			ui.rightSrc->markerAdd(v->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
#ifdef _test
			QCoreApplication::processEvents();
#endif
		}

		int oldBlockNum = leftEnd - leftStart + 1;
		int newBlockNum = leftLineInfo.size() + rightLineInfo.size();

		//整体调整后续不等块的偏移行号
		if (oldBlockNum != newBlockNum)
		{
			int interNum = newBlockNum - oldBlockNum;

			for (int start = leftStart + newBlockNum, size = m_rightExternBlockInfo->size(); start < size; ++start)
			{
				m_rightExternBlockInfo->at(start)->m_srcBlockNum += interNum;
			}
		}

		ui.rightSrc->SendScintilla(SCI_GOTOPOS, startPos);
	}

	//自己控制uc的顺序。然左右时刻平衡
	uc->addRecord(leftDelUserDataRecord);
	uc->addRecord(rightDelUserDataRecord);
	uc->addRecord(leftAddUserDataRecord);
	uc->addRecord(rightAddUserDataRecord);


	setLastBlockFormat(m_leftExternBlockInfo, m_rightExternBlockInfo);

	createNonEqualBlock(m_leftExternBlockInfo, m_rightExternBlockInfo, m_leftNoEqualBlocks, m_rightNoEqualBlocks);
	megreUnEqualBlock();

	cleanMargins(RC_LEFT);
	cleanMargins(RC_RIGHT);

	createMargins(m_leftExternBlockInfo, m_rightExternBlockInfo);

	++m_commandIndex;



	enableTextChangeSignal();
	if (old != false)//原来不是关闭，则处理完成后开启
	{
		enableSignTextChange(true);
	}

	updateDiffStatusWin(ui.leftSrc->lines(), m_leftNoEqualBlocks);
}
#endif
//进行对齐操作。拉开操作，拉开也是对齐的一种方式。
void CompareWin::slot_alignWork(int type, int leftStart, int leftEnd)
{

	//type = 0, 左边内容拉到上面，右边内容拉到下面。type = 1 反之。
	QsciDisplayWindow* srcDisWin = ((0 == type) ? ui.leftSrc : ui.rightSrc);
	QsciDisplayWindow* otherDisWin = ((0 == type) ? ui.rightSrc : ui.leftSrc);

	QList<BlockUserData*>* srcExternBlockInfo = ((0 == type) ? m_leftExternBlockInfo : m_rightExternBlockInfo);
	QList<BlockUserData*>* otherExternBlockInfo = ((0 == type) ? m_rightExternBlockInfo  : m_leftExternBlockInfo);

	RC_DIRECTION srcDire = ((0 == type) ? RC_LEFT : RC_RIGHT);
	RC_DIRECTION otherDire = ((0 == type) ? RC_RIGHT : RC_LEFT);

	int lineNum = srcDisWin->lines();
	if (leftEnd >= lineNum)
	{
		leftEnd = lineNum - 1;
	}

	//type == 0 。简单将内容拎出来，右边使用对齐处理，不和任何行对比
	bool old = enableSignTextChange(false);
	disableTextChangeSignal();

	QList<LineFileInfo> leftLineInfo;
	QList<LineFileInfo> rightLineInfo;

#ifdef OPEN_UNDO_REDO
	UserDataCommand* uc = new UserDataCommand(this);

	//单独拿出来，是因为要控制他们加入uc里面的顺序
	UserDataOperRecords* leftDelUserDataRecord = nullptr;
	UserDataOperRecords* leftAddUserDataRecord = nullptr;
	UserDataOperRecords* rightDelUserDataRecord = nullptr;
	UserDataOperRecords* rightAddUserDataRecord = nullptr;
#endif

	QByteArray leftDelContents;
	leftDelContents.reserve(5120);

	int leftDelTextLength = 0;

	//获取左边行的内容
	for (int i = leftStart; i <= leftEnd; ++i)
	{
		LineFileInfo lineText;
		leftDelTextLength += getOneLineText(srcDisWin, i, lineText, leftDelContents);

		int markMask = srcDisWin->SendScintilla(SCI_MARKERGET, i);
		if ((markMask & STYLE_PAD_OR_LASTPAD_BIT_MASK) != 0)
		{
			// 对齐行，直接continue;
			continue;
		}
		leftLineInfo.append(lineText);
	}

	//全部是对齐行，已经是错开的了，直接返回
	if (leftLineInfo.size() == 0)
	{
		delete uc;
		enableTextChangeSignal();
		return;
	}

	int rightDelTextLength = 0;
	QByteArray rightDelContents;
	rightDelContents.reserve(5120);

	//获取右边行的内容
	for (int i = leftStart; i <= leftEnd; ++i)
	{
		LineFileInfo lineText;
		rightDelTextLength += getOneLineText(otherDisWin, i, lineText, rightDelContents);

		int markMask = otherDisWin->SendScintilla(SCI_MARKERGET, i);
		if ((markMask & STYLE_PAD_OR_LASTPAD_BIT_MASK) != 0)
		{
			// 对齐行，直接continue;
			continue;
		}

		rightLineInfo.append(lineText);
	}

	//全部是对齐行，已经是错开的了，直接返回
	if (rightLineInfo.size() == 0)
	{
		delete uc;
		enableTextChangeSignal();
		return;
	}

	{
		//1)删除行的格式配置。倒着来，因为要删除，只能从尾部删除
		for (int start = leftEnd; start >= leftStart; --start)
		{
			//只删除格式
			deleteLinesMarker(srcDire, start, 1);
			setLineStyle(srcDisWin, STYLE_EQUAL_TEXT_COLOUR, start);
		}


		//4)再插入新的内容。把左边的实际内容收集在一起

		QByteArray leftBytes;
		leftBytes.reserve(1024 * 512);

		for (int i = 0; i < leftLineInfo.size(); ++i)
		{
			const LineFileInfo& data = leftLineInfo.at(i);
			leftBytes.append(data.unicodeStr.toUtf8());
		}
		//再插入换行表示对齐行
		for (int i = 0; i < rightLineInfo.size(); ++i)
		{
			//先不区分换行符号的类型，直接加入\n，后续要区分win mac linux
			leftBytes.append("\n");
		}

		//这里其实就是一个内容的替换，先删除、后增加，直接就是一个替换。使用新内容替换原来就内容
#ifdef OPEN_UNDO_REDO
		//把原来的内容收集起来。这里的原来，也是代表2阶段的文本，及修改后当前的文本，而不是原来修改前文本，也不是校正后文本。
		ReplaceOperRecords* replaceRecord = new ReplaceOperRecords();
		replaceRecord->dir = srcDire;

		int startPos = srcDisWin->SendScintilla(SCI_POSITIONFROMLINE, leftStart);
		int endPos = srcDisWin->SendScintilla(SCI_POSITIONFROMLINE, leftEnd + 1);

		replaceRecord->startPos = startPos;
		replaceRecord->srcEndPos = endPos;

		struct Sci_TextRange delTextRange;

		delTextRange.chrg.cpMin = startPos;
		delTextRange.chrg.cpMax = endPos;

		delTextRange.lpstrText = new char[endPos - startPos + 1];
		memset(delTextRange.lpstrText, 0, endPos - startPos + 1);
		//拷贝原始的内容
		srcDisWin->SendScintilla(SCI_GETTEXTRANGE, 0, &delTextRange);
		replaceRecord->srcContents = delTextRange.lpstrText;


		//把替换文本记录在replaceContents，后续撤销需要使用
		replaceRecord->replaceLens = leftBytes.length();

		char* replaceContens = new char[replaceRecord->replaceLens + 1];
		memset(replaceContens, 0, replaceRecord->replaceLens + 1);
		memcpy(replaceContens, leftBytes.data(), replaceRecord->replaceLens);

		replaceRecord->replaceContents = replaceContens;

		ReplaceCommand* ec = new ReplaceCommand(this);
		ec->setChangeStatus(NONE_CHANGE);
		ec->addRecord(replaceRecord);
		ec->setOperIndex(m_commandIndex);

		m_undoList.append(ec);

		replaceText(srcDire, startPos, endPos, leftBytes.length(), leftBytes.data());
#endif


#ifdef OPEN_UNDO_REDO

		//要注意左右任何时候的userdatablock都是要相等的。
		//这里要调整加删的顺序。左边要先删，右边也要先删。
		//后面左边加，右边也要加。不要穿插进行。比如左边先删后加。
		//右边再进行删、加。那么在左边删后，如果不是右边删，就会导致左右不等。20221019。
		//UserDataCommand* uc = new UserDataCommand(this);
		uc->setChangeStatus(NONE_CHANGE);
		uc->setOperIndex(m_commandIndex);

		//巨坑;20210804 这里一定要注意顺序。不能直接append。一定是先记录所有的修改内容后，在记录userdata变化
		//而且userdata在还原时，也是需要最后做的，而不能在前面做。否则乱序导致文本混乱

		//往前面插入,找到该相同index的最前面位置插入
		int insertPos = 0;
		for (insertPos = m_undoList.size() - 1; insertPos >= 0; --insertPos)
		{
			if (m_undoList.at(insertPos)->getOperIndex() != m_commandIndex)
			{
				break;
			}
		}

		m_undoList.insert(insertPos + 1, uc);
		leftDelUserDataRecord = new UserDataOperRecords(srcDire);
		leftDelUserDataRecord->operType = DEL_USER_DATA;

#endif

		//2)再删除行对应的userdata信息，
		for (int start = leftEnd; start >= leftStart; --start)
		{
#ifdef OPEN_UNDO_REDO
			//20230126 发现在尾巴拉开，可能导致崩溃。
			if (start < srcExternBlockInfo->size())
			{
			//把删除的格式保存起来
			BlockUserData* data = srcExternBlockInfo->at(start);
			srcExternBlockInfo->removeAt(start);

			leftDelUserDataRecord->userData.push_front(data);
			}
#else
			//把格式也删除
			BlockUserData* data = srcExternBlockInfo->at(start);
			srcExternBlockInfo->removeAt(start);
			delete data;
#endif
		}

#ifdef OPEN_UNDO_REDO

		leftAddUserDataRecord = new UserDataOperRecords(srcDire);
		leftAddUserDataRecord->operType = ADD_USER_DATA;
#endif

		//5）再插入新的userdata，主要更新其中的m_srcBlockNum值
		for (int index = 0, size = leftLineInfo.size(); index < size; ++index)
		{
			BlockUserData* v = new BlockUserData(UNEQUAL_BLOCK);
			v->m_srcBlockNum = leftStart + index;
			v->m_blockLen = 1;
			srcExternBlockInfo->insert(v->m_srcBlockNum, v);

#ifdef OPEN_UNDO_REDO
			leftAddUserDataRecord->userData.append(new BlockUserData(*v));
#endif
			srcDisWin->markerDelete(v->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
			srcDisWin->markerAdd(v->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
		}

		for (int index = 0, size = rightLineInfo.size(); index < size; ++index)
		{
			BlockUserData* v = new BlockUserData(PAD_BLOCK);
			v->m_srcBlockNum = leftStart + index + leftLineInfo.size();
			v->m_blockLen = 1;
			srcExternBlockInfo->insert(v->m_srcBlockNum, v);

#ifdef OPEN_UNDO_REDO
			leftAddUserDataRecord->userData.append(new BlockUserData(*v));
#endif

			srcDisWin->markerAdd(v->m_srcBlockNum, STYLE_PAD_BACK_PIXMAP);
#ifdef _test
			QCoreApplication::processEvents();
#endif
		}

		int oldBlockNum = leftEnd - leftStart + 1;
		int newBlockNum = leftLineInfo.size() + rightLineInfo.size();

		//整体调整后续不等块的偏移行号
		if (oldBlockNum != newBlockNum)
		{
			int interNum = newBlockNum - oldBlockNum;

			for (int start = leftStart + newBlockNum, size = srcExternBlockInfo->size(); start < size; ++start)
			{
				srcExternBlockInfo->at(start)->m_srcBlockNum += interNum;
			}
		}

		srcDisWin->SendScintilla(SCI_GOTOPOS, startPos);
	}

	//现在处理右边

	{
		//删除行的格式配置。倒着来，因为要删除，只能从尾部删除
		for (int start = leftEnd; start >= leftStart; --start)
		{
			//只删除格式
			deleteLinesMarker(otherDire, start, 1);
			setLineStyle(otherDisWin, STYLE_EQUAL_TEXT_COLOUR, start);
		}

		//再插入新的内容。把右边的实际内容收集在一起

		QByteArray rightBytes;
		rightBytes.reserve(1024 * 512);

		//先插入换行表示对齐行
		for (int i = 0; i < leftLineInfo.size(); ++i)
		{
			//先不区分换行符号的类型，直接加入\n，后续要区分win mac linux
			rightBytes.append("\n");
		}
		//在插入原来右边行的内容
		for (int i = 0; i < rightLineInfo.size(); ++i)
		{
			const LineFileInfo& data = rightLineInfo.at(i);
			rightBytes.append(data.unicodeStr.toUtf8());
		}


	//这里其实就是一个内容的替换，先删除、后增加，直接就是一个替换。使用新内容替换原来就内容
#ifdef OPEN_UNDO_REDO
	//把原来的内容收集起来。这里的原来，也是代表2阶段的文本，及修改后当前的文本，而不是原来修改前文本，也不是校正后文本。
		ReplaceOperRecords* replaceRecord = new ReplaceOperRecords();
		replaceRecord->dir = otherDire;

		int startPos = otherDisWin->SendScintilla(SCI_POSITIONFROMLINE, leftStart);
		int endPos = otherDisWin->SendScintilla(SCI_POSITIONFROMLINE, leftEnd + 1);

		replaceRecord->startPos = startPos;
		replaceRecord->srcEndPos = endPos;

		struct Sci_TextRange delTextRange;

		delTextRange.chrg.cpMin = startPos;
		delTextRange.chrg.cpMax = endPos;

		delTextRange.lpstrText = new char[endPos - startPos + 1];
		memset(delTextRange.lpstrText, 0, endPos - startPos + 1);
		//拷贝原始的内容
		otherDisWin->SendScintilla(SCI_GETTEXTRANGE, 0, &delTextRange);
		replaceRecord->srcContents = delTextRange.lpstrText;


		//把替换文本记录在replaceContents，后续撤销需要使用
		replaceRecord->replaceLens = rightBytes.length();

		char* replaceContens = new char[replaceRecord->replaceLens + 1];
		memset(replaceContens, 0, replaceRecord->replaceLens + 1);
		memcpy(replaceContens, rightBytes.data(), replaceRecord->replaceLens);

		replaceRecord->replaceContents = replaceContens;

		ReplaceCommand* ec = new ReplaceCommand(this);
		ec->setChangeStatus(NONE_CHANGE);
		ec->addRecord(replaceRecord);
		ec->setOperIndex(m_commandIndex);

		m_undoList.append(ec);

		replaceText(otherDire, startPos, endPos, rightBytes.length(), rightBytes.data());
#endif


#ifdef OPEN_UNDO_REDO
		rightDelUserDataRecord = new UserDataOperRecords(otherDire);
		rightDelUserDataRecord->operType = DEL_USER_DATA;

#endif

		//再删除行对应的userdata信息，
		for (int start = leftEnd; start >= leftStart; --start)
		{
#ifdef OPEN_UNDO_REDO
			//20230126 发现在尾巴拉开，可能导致崩溃。
			if (start < otherExternBlockInfo->size())
			{
			//把删除的格式保存起来
			BlockUserData* data = otherExternBlockInfo->at(start);
			otherExternBlockInfo->removeAt(start);

			rightDelUserDataRecord->userData.push_front(data);
			}
#else
			//把格式也删除
			BlockUserData* data = otherExternBlockInfo->at(start);
			otherExternBlockInfo->removeAt(start);
			delete data;
#endif
		}

#ifdef OPEN_UNDO_REDO

		rightAddUserDataRecord = new UserDataOperRecords(otherDire);
		rightAddUserDataRecord->operType = ADD_USER_DATA;

#endif

		//5）再插入新的userdata，主要更新其中的m_srcBlockNum值
		for (int index = 0, size = leftLineInfo.size(); index < size; ++index)
		{
			BlockUserData* v = new BlockUserData(PAD_BLOCK);
			v->m_srcBlockNum = leftStart + index;
			v->m_blockLen = 1;
			otherExternBlockInfo->insert(leftStart + index, v);


#ifdef OPEN_UNDO_REDO
			rightAddUserDataRecord->userData.append(new BlockUserData(*v));
#endif

			otherDisWin->markerAdd(v->m_srcBlockNum, STYLE_PAD_BACK_PIXMAP);
		}

		for (int index = 0, size = rightLineInfo.size(); index < size; ++index)
		{
			BlockUserData* v = new BlockUserData(UNEQUAL_BLOCK);
			v->m_srcBlockNum = leftStart + index + leftLineInfo.size();
			v->m_blockLen = 1;
			otherExternBlockInfo->insert(v->m_srcBlockNum, v);

#ifdef OPEN_UNDO_REDO
			rightAddUserDataRecord->userData.append(new BlockUserData(*v));
#endif
			otherDisWin->markerDelete(v->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
			otherDisWin->markerAdd(v->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
#ifdef _test
			QCoreApplication::processEvents();
#endif
		}

		int oldBlockNum = leftEnd - leftStart + 1;
		int newBlockNum = leftLineInfo.size() + rightLineInfo.size();

		//整体调整后续不等块的偏移行号
		if (oldBlockNum != newBlockNum)
		{
			int interNum = newBlockNum - oldBlockNum;

			for (int start = leftStart + newBlockNum, size = otherExternBlockInfo->size(); start < size; ++start)
			{
				otherExternBlockInfo->at(start)->m_srcBlockNum += interNum;
			}
		}

		otherDisWin->SendScintilla(SCI_GOTOPOS, startPos);
	}

	//自己控制uc的顺序。然左右时刻平衡
	uc->addRecord(leftDelUserDataRecord);
	uc->addRecord(rightDelUserDataRecord);
	uc->addRecord(leftAddUserDataRecord);
	uc->addRecord(rightAddUserDataRecord);


	setLastBlockFormat(m_leftExternBlockInfo, m_rightExternBlockInfo);

	createNonEqualBlock(m_leftExternBlockInfo, m_rightExternBlockInfo, m_leftNoEqualBlocks, m_rightNoEqualBlocks);
	megreUnEqualBlock();

	cleanMargins(RC_LEFT);
	cleanMargins(RC_RIGHT);

	createMargins(m_leftExternBlockInfo, m_rightExternBlockInfo);

	++m_commandIndex;



	enableTextChangeSignal();
	if (old != false)//原来不是关闭，则处理完成后开启
	{
		enableSignTextChange(true);
	}

	updateDiffStatusWin(ui.leftSrc->lines(), m_leftNoEqualBlocks);
}

void  CompareWin::slot_rultBt()
{
	FileCmpRuleWin* p = new FileCmpRuleWin(m_cmpMode, m_isBlankLineDoMatch, m_lineMatchEqualRata);
	p->setAttribute(Qt::WA_DeleteOnClose);
	p->setWindowModality(Qt::ApplicationModal);
	connect(p, &FileCmpRuleWin::sign_cmpModeChange, this, &CompareWin::slot_cmpModeChange);
	p->show();
}

//每个不同的单独一个箭头。裂开
void  CompareWin::slot_breakBt(bool check)
{
	if (m_leftExternBlockInfo == nullptr || m_rightExternBlockInfo == nullptr)
	{
		return;
	}

	//保存并恢复打断前后的curNoEqualLine，避免后续跳转上下时序号错误而失效
	int curNoEqualLine = 0;

	if (m_curShowBlockNums < 0)
	{
		m_curShowBlockNums = 0;
	}

	if (m_curShowBlockNums < m_leftNoEqualBlocks.size())
	{
		curNoEqualLine = m_leftNoEqualBlocks.at(m_curShowBlockNums).startBlockNums;
	}

		createNonEqualBlock(m_leftExternBlockInfo, m_rightExternBlockInfo, m_leftNoEqualBlocks, m_rightNoEqualBlocks);

		if (!check)
		{
			megreUnEqualBlock();
		}

	for (int i = 0; i < m_leftNoEqualBlocks.size(); ++i)
	{
		const NoEqualBlock & t = m_leftNoEqualBlocks.at(i);
		if (curNoEqualLine <= (t.startBlockNums + t.blockLens))
		{
			m_curShowBlockNums = i;
			break;
		}
	}

		cleanMargins(RC_LEFT);
		cleanMargins(RC_RIGHT);

		createMargins(m_leftExternBlockInfo, m_rightExternBlockInfo);

	ui.statusBar->showMessage(tr("current has %1 differents").arg(m_leftNoEqualBlocks.count()));

	if (m_leftNoEqualBlocks.size() < 200)
	{
		updateDiffStatusWin(ui.leftSrc->lines(), m_leftNoEqualBlocks);
}
}

//严格规则。严格时要匹配前面的空白行，而且m_lineMatchEqualRata为90
void  CompareWin::slot_strictBt(bool isStrict)
{
	if (m_tolerantBt->isChecked())
	{
		m_tolerantBt->setChecked(false);
	}

	int newMode = (isStrict ? 1 : 0);

	int newEqualRata = (isStrict ? 90 : 50);

	if (m_lineMatchEqualRata != newEqualRata)
	{
		m_lineMatchEqualRata = newEqualRata;

		blockCompare->setMatchParameter(m_isBlankLineDoMatch, m_lineMatchEqualRata);
	}

	if (m_cmpMode != newMode)
	{
		m_cmpMode = newMode;


		slot_doCmp();
	}
}

//宽容规则。行中所有空白字符都不参与比较。
void  CompareWin::slot_tolerantBt(bool isTolerant)
{
	if (m_strictBt->isChecked())
	{
		m_strictBt->setChecked(false);
	}

	int newMode = (isTolerant ? 2 : 0);

	int newEqualRata = 50;

	if (m_lineMatchEqualRata != newEqualRata)
	{
		m_lineMatchEqualRata = newEqualRata;

		blockCompare->setMatchParameter(m_isBlankLineDoMatch, m_lineMatchEqualRata);
	}

	if (m_cmpMode != newMode)
	{
		m_cmpMode = newMode;

		slot_doCmp();
	}
}

//修改了对比方式
void  CompareWin::slot_cmpModeChange(int mode, bool blankMatch, int equalRata)
{
	bool isChange = false;

	if (m_cmpMode != mode)
	{
		m_cmpMode = mode;
		isChange = true;

		if (m_cmpMode != 2)
		{
			m_tolerantBt->setChecked(false);
	}
		if (m_cmpMode != 1)
		{
			m_strictBt->setChecked(false);
		}
	}

	if (m_isBlankLineDoMatch != blankMatch)
	{
		m_isBlankLineDoMatch = blankMatch;
		blockCompare->setMatchParameter(blankMatch, equalRata);
		isChange = true;
	}

	if (m_lineMatchEqualRata != equalRata)
	{
		m_lineMatchEqualRata = equalRata;
		blockCompare->setMatchParameter(blankMatch, equalRata);
		isChange = true;
	}

	if (isChange)
	{
		slot_doCmp();
	}
}

void CompareWin::slot_whiteCharClick(bool checked)
{
	QToolButton* showWhiteChar = dynamic_cast<QToolButton*>(sender());
	if (checked)
	{
		showWhiteChar->setIcon(QIcon(":/Resources/img/showchar.png"));

#if 1 //设置显示行尾和空白字符
		ui.leftSrc->SendScintilla(SCI_SETVIEWWS, SCWS_VISIBLEALWAYS);
		ui.rightSrc->SendScintilla(SCI_SETVIEWWS, SCWS_VISIBLEALWAYS);
		ui.leftSrc->SendScintilla(SCI_SETVIEWEOL, true);
		ui.rightSrc->SendScintilla(SCI_SETVIEWEOL, true);

		ui.leftDiffView->SendScintilla(SCI_SETVIEWWS, SCWS_VISIBLEALWAYS);
		ui.rightDiffView->SendScintilla(SCI_SETVIEWWS, SCWS_VISIBLEALWAYS);
		ui.leftDiffView->SendScintilla(SCI_SETVIEWEOL, true);
		ui.rightDiffView->SendScintilla(SCI_SETVIEWEOL, true);
#endif

	}
	else
	{
		showWhiteChar->setIcon(QIcon(":/Resources/img/hidechar.png"));

#if 1 //设置显示行尾和空白字符
		ui.leftSrc->SendScintilla(SCI_SETVIEWWS, SCWS_INVISIBLE);
		ui.rightSrc->SendScintilla(SCI_SETVIEWWS, SCWS_INVISIBLE);
		ui.leftSrc->SendScintilla(SCI_SETVIEWEOL, false);
		ui.rightSrc->SendScintilla(SCI_SETVIEWEOL, false);

		ui.leftDiffView->SendScintilla(SCI_SETVIEWWS, SCWS_INVISIBLE);
		ui.rightDiffView->SendScintilla(SCI_SETVIEWWS, SCWS_INVISIBLE);
		ui.leftDiffView->SendScintilla(SCI_SETVIEWEOL, false);
		ui.rightDiffView->SendScintilla(SCI_SETVIEWEOL, false);
#endif
	}
}

/* 开闭通知，因为经常在修改过程中，会触发新的修改通知，为避免麻烦，直接开闭通知 */
void CompareWin::enableTextChangeSignal()
{
	if (!m_enableTextChangeSignal)
	{
		m_enableTextChangeSignal = true;
		connect(m_mediator, &MediatorDisplay::syncCurScrollValue, this, &CompareWin::slot_syncCurScrollValue, Qt::QueuedConnection);
		connect(m_mediator, &MediatorDisplay::syncCurScrollXValue, this, &CompareWin::slot_syncCurScrollXValue, Qt::QueuedConnection);

		connect(ui.leftSrc, SIGNAL(SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)), this, SLOT(slot_left_SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)));
		connect(ui.rightSrc, SIGNAL(SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)), this, SLOT(slot_right_SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)));

		connect(ui.leftSrc, &QsciScintilla::cursorPosChange, this, &CompareWin::on_updateDiffView, Qt::QueuedConnection);
		connect(ui.rightSrc, &QsciScintilla::cursorPosChange, this, &CompareWin::on_updateDiffView, Qt::QueuedConnection);
	}
}


void CompareWin::on_updateDiffView(int line, int pos)
{
	auto showLineText = [&](QsciDisplayWindow* src, QsciDisplayWindow* diffView) {
		if (line >= src->lines())
		{
			return;
		}

		int curLineLength = src->SendScintilla(SCI_LINELENGTH, line);

		char* contens = new char[curLineLength + 1];
		memset(contens, 0, curLineLength + 1);
		src->SendScintilla(SCI_GETLINE, line, contens);

		QString text(contens);
		diffView->setText(text);
		delete[] contens;
	};
	showLineText(ui.leftSrc,ui.leftDiffView);
	showLineText(ui.rightSrc, ui.rightDiffView);

	if (line < m_leftExternBlockInfo->size())
	{
		//如果是相等，啥也不需要做了
		if (m_leftExternBlockInfo->at(line)->m_blockType == EQUAL_BLOCK)
		{
			return;
		}

		//修改文字颜色
		QVector<BlockNode> leftBlocks;
		QVector<BlockNode> rightBlocks;

		QVector<UnequalCharsPosInfo> leftUnqealPosList;
		QVector<UnequalCharsPosInfo> rightUnqealPosList;

		//20210724 这里发现可能崩溃，在界面上的最后一行添加字符，最后一行其实不是文本的内容，不过因为换行导致界面显示下面有一个空的行。
		//但是这个空行其实没有externBlockInfo中对应的元素

		cmpLineOneByOne(1, 0, 0, \
			ui.leftDiffView, ui.rightDiffView, \
			nullptr, nullptr, \
			leftBlocks, rightBlocks);

		QStringList leftContent;
		QStringList rightContent;

		QList<BlockUserData*> leftExternBlockInfo;
		QList<BlockUserData*> rightExternBlockInfo;

		QVector<int> leftLineStatus;
		QVector<int> rightLineStatus;

		getCompareUnqealPos(leftBlocks, leftContent, leftExternBlockInfo, leftUnqealPosList, leftLineStatus);
		getCompareUnqealPos(rightBlocks, rightContent, rightExternBlockInfo, rightUnqealPosList, rightLineStatus);


		int leftStartPos = ui.leftDiffView->SendScintilla(SCI_POSITIONFROMLINE, 0);

		for (auto it = leftUnqealPosList.begin(); it != leftUnqealPosList.end(); ++it)
		{
			ui.leftDiffView->SendScintilla(SCI_STARTSTYLING, leftStartPos + it->start);
			ui.leftDiffView->SendScintilla(SCI_SETSTYLING, it->length, (long)STYLE_COLOUR_RED);
		}

		int rightStartPos = ui.rightDiffView->SendScintilla(SCI_POSITIONFROMLINE, 0);

		for (auto it = rightUnqealPosList.begin(); it != rightUnqealPosList.end(); ++it)
		{
			ui.rightDiffView->SendScintilla(SCI_STARTSTYLING, rightStartPos + it->start);
			ui.rightDiffView->SendScintilla(SCI_SETSTYLING, it->length, (long)STYLE_COLOUR_RED);
		}
	}

}


//建议，disconnect不要偷懒，connetc怎么写的,disconnect就怎么写，不要扩大影响面
void CompareWin::disableTextChangeSignal()
{
	if (m_enableTextChangeSignal)
	{
		m_enableTextChangeSignal = false;
		m_mediator->disconnect(SIGNAL(syncCurScrollValue(int)));
		m_mediator->disconnect(SIGNAL(syncCurScrollXValue(int)));

		//发现如果没有SCN_MODIFIED，就没有textChanged消息了。所以该消息还是要发，但是对象不接受而已。但是不能下面这样写？有点说不通。
		//看了qscint代码，textChanged是在信号SCN_MODIFIED发生后的槽函数中触发的。即如果没有SCN_MODIFIED信号，也就不会有textChanged信号。
		//disconnect(this,SLOT(slot_left_SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)));
		//disconnect(this,SLOT(slot_right_SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)));

		//这样就可以。虽然CompareWin不接受slot_left_SCN_MODIFIED，但是并没有屏蔽SCN_MODIFIED信号的发生，进而textChanged还是可以检测到。
		disconnect(ui.leftSrc, SIGNAL(SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)),this, SLOT(slot_left_SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)));
		disconnect(ui.rightSrc, SIGNAL(SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)), this, SLOT(slot_right_SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)));


		disconnect(ui.leftSrc, &QsciScintilla::cursorPosChange, this, &CompareWin::on_updateDiffView);
		disconnect(ui.rightSrc, &QsciScintilla::cursorPosChange, this, &CompareWin::on_updateDiffView);

		//下面这样比较暴力，就是说不发送SCN_MODIFIED，这样一来textChanged也没有了。最开始这样写的，替换掉了。
		//ui.leftSrc->disconnect(SIGNAL(SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)));
		//ui.rightSrc->disconnect(SIGNAL(SCN_MODIFIED(int, int, const char*, int, int, int, int, int, int, int)));
	}
}


//左边的发生了文字的修改。2021/6/20号：可能发生同时删除和增加的情况，就会短期出现2次。比如拷贝一段内容，去替换旧的内容。会先发生删除，立刻发生增加。
//要能将这一次修改，合并识别出来才行。否则分两次来处理的话，也可以，但是不太好，是否有合理的方法？
//20211013 对拷贝进行了识别，如果是拷贝时删除，则等待后面的增加，合并在一起处理。如果是直接的拷贝和删除，不是拷贝替换那种，则“直接”执行，这个直接不是立即
//因为该函数在槽函数中，还是要sendModify队列方式，发生到其他线程中；否则qscint中间逻辑还没有完毕，就执行同步后编辑，会导致问题。
void CompareWin::slot_left_SCN_MODIFIED(int position, int modificationType, const char* text, int length, int linesAdded, int /*line*/, int /*foldLevelNow*/, int /*foldLevelPrev*/, int /*token*/, int /*annotationLinesAdded*/)
{
	//1是增加，2是删除
	modificationType &= (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);

	//不是关于增加与修改文字的修改
	if (modificationType == 0 || (text == nullptr))
	{
		return;
	}

	if (m_leftOperStatus != OperStatus::RC_NONE)
	{
		return;
	}

	ModifyRecords* record = new ModifyRecords(position, modificationType, length, linesAdded);

	record->isInPaste = ui.leftSrc->isInPasteStatus();
	m_leftModifyRecord.append(record);

#ifdef OPEN_UNDO_REDO
	ModifyOperRecords* operRecord = new ModifyOperRecords(position, modificationType, length, linesAdded);
	operRecord->dir = RC_LEFT;
	operRecord->contents = new char[length + 1];
	memset(operRecord->contents, 0, length + 1);
	memcpy(operRecord->contents, text, length);

	EditCommand *ec = new EditCommand(this);
	ec->setChangeStatus(LEFT_CHANGE);
	ec->addRecord(operRecord);

	ec->setOperIndex(m_commandIndex);

	m_undoList.append(ec);
#endif

	//20210707 可能增加行，可能减少行，也会同时出现增加和删除行的情况
	//但是可能会出现先删除字符，有增加字符的情况，这种情况出现在拷贝替换的过程。

	//注意，替换的过程就是先删除、再添加。如果是多行替换少行，则是先删除几行，删除后再添加，但是两次的pos位置是一样的
	//如果是少替换多，也是先删除行，删除后再添加。记住：可以尝试最多只处理2个触发合并为一的情况。
	//因为替换就是先删除、再添加，这两次是一个动作的触发，需要合并一起处理。而且这两次的pos一定相等

	//20210722 如果第一个修改是删除行，则需要延迟处理。因为替换就是先删除/后添加，二者合一处理比较好
	//而如果是纯粹增加行，则直接处理。
	//这样的想法是好的，发现一个bug，左边的内容在删除时，无法生效！可能时直接执行，还没有退出当前的执行流程导致。必须要走延迟sendModify处理
	//这里先留着，等于是给自己留一个教训，避免后面又放开这里，犯了同样的错误。
	//20210729 发现上面的无法删除marker的问题了，引入了markerall删除的函数。因为qscint的机制导致删除当前行，会把下行的marker合并，而合并又没有做
	//去重出来，导致一个行上面很多marker，光删除一次是不够的，需要删除多次，故修改qscint的中删除一行所有marker的功能
#if 0
	if (m_leftModifyRecord.size() == 1 && record->linesAdded > 0)
	{
		//直接处理，第一次纯粹增加的情况
		QCoreApplication::processEvents();
		slot_doCmpAfterEdit(0);
		return;
	}
#endif


	emit sendModify(0, record);


	if (!m_isLeftDirty)
	{
		m_isLeftDirty = true;

		syncNeedSaveIcon();
	}

	//如果是删除内容，则要注意，后面很可能短期来一个新增的消息。比如替换时，总是先删除，再增加。
	//而如果是增加，则往往直接就是增加。对于删除，延时半秒的时间。没有更好的办法，还是只能严格的来一个修改，做一次修正。
}

enum LEFT_RIGHT_LINE_STATUS {
	STATUS_UNKNOWN = -1,
	BOTH_EQUAL = 0,//左右相等
	BOTH_UNEQUAL,
	LEFT_UNEQUAL_RIGHT_PAD,
	LEFT_PAD_RIGHT_UNEQUAL
};

//根据左右目前的状态，考虑如何去做下一步的对比
int CompareWin::getLineCompareStatus(BlockUserData* leftBlockData, BlockUserData* rightBlockData)
{
	int leftStatus = leftBlockData->m_blockType;
	int rightStatus = rightBlockData->m_blockType;

	int ret = STATUS_UNKNOWN;

	if (EQUAL_BLOCK == leftStatus)
	{
		if (EQUAL_BLOCK == rightStatus)
		{
			//左右原来是相同的
			ret = BOTH_EQUAL;
		}
		else
		{
			assert(false);
		}
	}
	else if (UNEQUAL_BLOCK == leftStatus)
	{
		if (UNEQUAL_BLOCK == rightStatus)
		{
			//右边是不相同
			ret = BOTH_UNEQUAL;
		}
		else if (PAD_BLOCK == rightStatus)
		{
			//右边是对齐行
			ret = LEFT_UNEQUAL_RIGHT_PAD;
		}
		else
		{
			assert(false);
		}
	}
	else if (PAD_BLOCK == leftStatus)
	{
		if (UNEQUAL_BLOCK == rightStatus)
		{
			//右边是不相同
			ret = LEFT_PAD_RIGHT_UNEQUAL;
		}
		else
		{
			assert(false);
		}
	}

	return ret;
}

#ifdef OPEN_UNDO_REDO
//lineNum：对比行的数量
//leftStartLineNum：左边开始的行号
//rightStartLineNum：右边开始的行号
//左右两边，各取一定的行，每行直接进行对比
void CompareWin::cmpLineOneByOne(int lineNum, int leftStartLineNum, int rightStartLineNum, \
	QsciDisplayWindow* leftSrc, QsciDisplayWindow* rightSrc, \
	QList<BlockUserData*>* /*leftExternBlockInfo*/, QList<BlockUserData*>* /*rightExternBlockInfo*/, \
	QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks)
{
	QList<LineFileInfo> leftLineInfo;
	QList<LineFileInfo> rightLineInfo;

	BlocksInfo leftBlockInfo(false, 0, lineNum, 1);
	BlocksInfo rightBlockInfo(false, 0, lineNum, 1);

	int leftLineNum = 0;
	for (int i = leftStartLineNum, lens = 0; lens < lineNum; ++i, ++leftLineNum, ++lens)
	{
		int leftCurLineLength = leftSrc->SendScintilla(SCI_LINELENGTH, i);

		char* leftContens = new char[leftCurLineLength + 1];
		memset(leftContens, 0, leftCurLineLength + 1);

		leftSrc->SendScintilla(SCI_GETLINE, i, leftContens);

		LineFileInfo leftInfo;
		leftInfo.lineNums = leftLineNum;
		leftInfo.unicodeStr = QString(leftContens);
		leftInfo.isEmptyLine = (strlen(leftContens) == 0);
		leftLineInfo.append(leftInfo);

		delete[]leftContens;
	}

	leftBlockInfo.endLine = leftLineNum;
	leftBlockInfo.actualNums = leftLineNum;

	int rightLineNum = 0;
	for (int i = rightStartLineNum, lens = 0; lens < lineNum; ++i, ++lens)
	{
	/*	BlockUserData* rightLineData = rightExternBlockInfo->at(i);

		if ((rightLineData->m_blockType == EQUAL_BLOCK) || (rightLineData->m_blockType == UNEQUAL_BLOCK))
		{*/
			int rightCurLineLength = rightSrc->SendScintilla(SCI_LINELENGTH, i);

			char* rightContens = new char[rightCurLineLength + 1];
			memset(rightContens, 0, rightCurLineLength + 1);

			rightSrc->SendScintilla(SCI_GETLINE, i, rightContens);

			LineFileInfo rightInfo;
			rightInfo.lineNums = rightLineNum;
			rightInfo.unicodeStr = QString(rightContens);
			rightInfo.isEmptyLine = (strlen(rightContens) == 0);
			rightInfo.lineEndFormat = getLineEndType(rightInfo.unicodeStr);
			rightLineInfo.append(rightInfo);

			delete[]rightContens;

			++rightLineNum;
//		}
//		else
//		{
//			//这里是对齐块，如果是对齐，直接填充\r\n。不能这么做。20211015发现bug。最后一行本来是空，而且没有\r\n的那种对齐行
//			//结果填充后导致超过了外面的实际长度，进而崩溃
//			LineFileInfo rightInfo;
//			rightInfo.lineNums = rightLineNum;
//#ifdef _WIN32
//			rightInfo.unicodeStr = QString("\r\n");
//#else
//			rightInfo.unicodeStr = QString("\n");
//#endif
//			rightInfo.isEmptyLine = true;
//			rightInfo.lineEndFormat = getLineEndType(rightInfo.unicodeStr);
//			rightLineInfo.append(rightInfo);
//
//			++rightLineNum;
//
//		}
	}

	rightBlockInfo.endLine = rightLineNum;
	rightBlockInfo.actualNums = rightLineNum;


	for (int i = 0; i < lineNum; ++i)
	{
		blockCompare->createBlockWithOneLine(i, i, leftLineInfo, rightLineInfo, leftBlocks, rightBlocks);
	}
}
#endif

//在当前编辑视图中，获取左右多行，对比后得出结果
void CompareWin::outputMultiLineCompareResult(int leftStartLineNum, int leftLineLength, int externLeftLineLength, \
	int rightStartLineNum, int rightLineLength, \
	QsciDisplayWindow* leftSrc, QsciDisplayWindow* rightSrc, \
	QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks)
{
	QList<LineFileInfo> leftLineInfo;
	QList<LineFileInfo> rightLineInfo;

	BlocksInfo leftBlockInfo(false, 0, leftLineLength, 1);
	BlocksInfo rightBlockInfo(false, 0, rightLineLength, 1);

	int leftLineNum = 0;
	bool isPadLine = false;

	for (int i = leftStartLineNum, lens = 0; lens < leftLineLength; ++i, ++lens)
	{
		//从界面去获取mark是否是对齐行。20211014这里不能这样，如果修改的就是对齐行，
		//比如在对齐行上增加文字，如果不获取，会导致更新失败。这里有个两难问题，不然不检测
		//则在对齐行出键入换行，会导致多了一行，因为把对齐行也认定为内容了。做个兼容：
		//如果是对齐行，但是内容不是空，则拷贝；如果是对齐行，内容也为空，则不拷贝。
		isPadLine = false;

		int markMask = leftSrc->SendScintilla(SCI_MARKERGET, i);
		if ((markMask & STYLE_PAD_BACK_PIXMAP_BIT_MASK) != 0)
		{
			isPadLine = true;
		}

		int leftCurLineLength = leftSrc->SendScintilla(SCI_LINELENGTH, i);

		char* leftContens = new char[leftCurLineLength + 1];
		memset(leftContens, 0, leftCurLineLength + 1);
		leftSrc->SendScintilla(SCI_GETLINE, i, leftContens);

		LineFileInfo leftInfo;
		leftInfo.lineNums = leftLineNum;
		leftInfo.unicodeStr = QString(leftContens);


		//如果即是对齐行，又只有换行，则不加入统计
		if (isPadLine && ((leftInfo.unicodeStr == "\r\n")|| (leftInfo.unicodeStr == "\n")|| (leftInfo.unicodeStr == "\r")))
		{
			delete[]leftContens;
			continue;
		}

		leftInfo.isEmptyLine = (strlen(leftContens) == 0);
		leftLineInfo.append(leftInfo);

		delete[]leftContens;

		++leftLineNum;
	}

	for (int i = leftStartLineNum + leftLineLength, lens = 0; lens < externLeftLineLength; ++i, ++lens)
	{
		//从界面去获取mark是否是对齐行;否则不是实际内容行，则不取其内容
		int markMask = leftSrc->SendScintilla(SCI_MARKERGET, i);
		if ((markMask & STYLE_PAD_BACK_PIXMAP_BIT_MASK) != 0)
		{
			continue;
		}
		//如果行是有内容的，而不是对齐行那种，则实际取其值
		int leftCurLineLength = leftSrc->SendScintilla(SCI_LINELENGTH, i);

		char* leftContens = new char[leftCurLineLength + 1];
		memset(leftContens, 0, leftCurLineLength + 1);

		leftSrc->SendScintilla(SCI_GETLINE, i, leftContens);

		LineFileInfo leftInfo;
		leftInfo.lineNums = leftLineNum;
		leftInfo.unicodeStr = QString(leftContens);
		leftInfo.isEmptyLine = (strlen(leftContens) == 0);
		leftInfo.lineEndFormat = getLineEndType(leftInfo.unicodeStr);
		leftLineInfo.append(leftInfo);

		delete[]leftContens;

		++leftLineNum;
	}

	leftBlockInfo.endLine = leftLineNum;
	leftBlockInfo.actualNums = leftLineNum;

	int rightLineNum = 0;
	for (int i = rightStartLineNum, lens = 0; lens < rightLineLength; ++i, ++lens)
	{
		int markMask = rightSrc->SendScintilla(SCI_MARKERGET, i);
		if ((markMask & STYLE_PAD_BACK_PIXMAP_BIT_MASK) != 0)
		{
			continue;
		}

		int rightCurLineLength = rightSrc->SendScintilla(SCI_LINELENGTH, i);

		char* rightContens = new char[rightCurLineLength + 1];
		memset(rightContens, 0, rightCurLineLength + 1);

		rightSrc->SendScintilla(SCI_GETLINE, i, rightContens);

		LineFileInfo rightInfo;
		rightInfo.lineNums = rightLineNum;
		rightInfo.unicodeStr = QString(rightContens);
		rightInfo.isEmptyLine = (strlen(rightContens) == 0);
		rightInfo.lineEndFormat = getLineEndType(rightInfo.unicodeStr);
		rightLineInfo.append(rightInfo);

		delete[]rightContens;

		++rightLineNum;

	}

	rightBlockInfo.endLine = rightLineNum;
	rightBlockInfo.actualNums = rightLineNum;

	blockCompare->blockCmpLcs(leftBlockInfo, leftLineInfo, rightBlockInfo, rightLineInfo, leftBlocks, rightBlocks);

}

//这里的BlockNode其实只有1行，后续还需要修正
void CompareWin::getCompareUnqealPos(BlockNode & blocks, QVector<UnequalCharsPosInfo>& m_unqealPosList, int& blockType)
{
	blockType = UNKNOWN_BLOCK;

	int curCharPos = 0;

	const BlockNode& t = blocks;

	if (t.type == BlockType::REAL_DATA)
	{
		//实际数据块
		for (int j = 0; j < t.lineNodes.count(); ++j)
		{
			const LineNode& lines = t.lineNodes.at(j);

			//虽然是不相等块，但是行却完全相同，还是按照相等块对待
			//这种情况大概率出现在左右两边都是空白行的情况
			//相等块或者左右行完全相同，均作为相等块输出
			if (lines.totalEqual)
			{
				blockType = EQUAL_BLOCK;

				//已经是相等了，不再进行比较
				return;
			}
			//不相等的块需要设置红色背景块
			else
			{
				blockType = UNEQUAL_BLOCK;
			}


			QString oneLineContens;

			for (int k = 0; k < lines.lineText.count(); ++k)
			{
				const SectionNode& section = lines.lineText.at(k);

				/* 2020-6-14 发现：\r\n行尾在textCursor里面只算一个QChar字符8233 ；而且每次插入一行（带换行符）后，块就会自动加1块
				* 这里有个问题，上面如果设置setNoEqualBackColor颜色后，再下面调用insertPlainText插入时，如果插入的带有换行，肯定会新
				* 加一块，这个块的颜色也会被之前设置setNoEqualBackColor后颜色所覆盖。但是由于下次调用会重新设置，所以之前没有暴露问题。
				*/
				if (section.equal)
				{
					//oneLineContens.append(section.text);
				}
				else
				{
					//oneLineContens.append(section.text);

					UnequalCharsPosInfo pos;
					pos.start = curCharPos;
					pos.length = section.text.toUtf8().size();

					m_unqealPosList.append(pos);
				}

				/*2021/6/12发现必须要把qstring转换为uft8,然后统计字节长度才行；如果直接用qstring.size则会在中文下对比错误；
				*我们是把不管什么字符编码原始文件，都转换为unicode这样一个统一的编码后，再去对比的。
				*/
				curCharPos += section.text.toUtf8().size();
			}
		}
	}
	else if (t.type == BlockType::PAD_DATA)
	{
		//什么都不用做
	}
}

//这里的BlockNode有多行，从只有一行扩展过来的
void CompareWin::getCompareUnqealPos(QVector<BlockNode>& blocks, QStringList& content, QList<BlockUserData*>& externBlockInfo, QVector<UnequalCharsPosInfo>& m_unqealPosList, QVector<int> & blockTypes)
{
	int blockType = UNKNOWN_BLOCK;

	int curCharPos = 0;

	for (int i = 0; i < blocks.size(); ++i)
	{
		const BlockNode& t = blocks.at(i);

		if (t.type == BlockType::REAL_DATA)
		{
			//实际数据块
			for (int j = 0; j < t.lineNodes.count(); ++j)
			{
				const LineNode& lines = t.lineNodes.at(j);

				//虽然是不相等块，但是行却完全相同，还是按照相等块对待
				//这种情况大概率出现在左右两边都是空白行的情况
				//相等块或者左右行完全相同，均作为相等块输出
				if (lines.totalEqual)
				{
					blockType = EQUAL_BLOCK;
					//已经是相等了，不再进行比较
					blockTypes.append(blockType);
				}
				//不相等的块需要设置红色背景块
				else
				{
					blockType = UNEQUAL_BLOCK;
					blockTypes.append(blockType);
				}

				BlockUserData* userData = new BlockUserData(blockType);
				userData->setSrcBlockNum(content.size());
				userData->m_lineEndType = (int)getLineEndType(lines);

				externBlockInfo.append(userData);

				QString oneLineContens;

				for (int k = 0; k < lines.lineText.count(); ++k)
				{
					const SectionNode& section = lines.lineText.at(k);

					/* 2020-6-14 发现：\r\n行尾在textCursor里面只算一个QChar字符8233 ；而且每次插入一行（带换行符）后，块就会自动加1块
					* 这里有个问题，上面如果设置setNoEqualBackColor颜色后，再下面调用insertPlainText插入时，如果插入的带有换行，肯定会新
					* 加一块，这个块的颜色也会被之前设置setNoEqualBackColor后颜色所覆盖。但是由于下次调用会重新设置，所以之前没有暴露问题。
					*/

					if (section.equal)
					{
						oneLineContens.append(section.text);

						//20230404 新增：比如尾巴是不等，左边是\r，右边是\r\n,公共是\r，其实右边尾巴\n是不等的。但是如果尾巴之前\r是相等
					//由于会把换行尾巴附加在起一个节上面。所以如果前一个节相等，但是尾巴其实不等。比如3\r 和3\r\n这种，会认定为相等。
					//此时要单独判断section.tailStatus不等的情况。把最后一个尾巴字符，标定为不等。
						//对比时，默认是无条件忽略尾部空白字符的，所以左右换行符号不等的情况，默认其实是不能识别出来的。
						//但是修改一行后，确实可以精确识别出换行字符的。这是20230404加的，虽然越来越复杂，但是精度更高了。
						if (section.tailStatus == 2)
						{

							UnequalCharsPosInfo pos;

							pos.length = section.text.toUtf8().size();

							pos.start = curCharPos + pos.length - section.tailLens;
							pos.length = section.tailLens;
							m_unqealPosList.append(pos);

					}
					}
					else
					{
						oneLineContens.append(section.text);
						UnequalCharsPosInfo pos;
						pos.start = curCharPos;
						pos.length = section.text.toUtf8().size();

						//是相等的换行尾巴。但是附在了不同的前一个节上面。扣除后面的尾巴字段长度。
						//这样后面尾巴换行字符，则是相等的颜色，不会使用红色异常标定。
						//如果本身也是不等的尾巴，及tailStatus=2，则还是要标定。
						if (section.tailStatus == 1)
						{
							pos.length -= section.tailLens;
						}

						m_unqealPosList.append(pos);
					}

					/*2021/6/12发现必须要把qstring转换为uft8,然后统计字节长度才行；如果直接用qstring.size则会在中文下对比错误；
					*我们是把不管什么字符编码原始文件，都转换为unicode这样一个统一的编码后，再去对比的。
					*/
					curCharPos += section.text.toUtf8().size();
				}
				content.append(oneLineContens);
			}
		}
		else if (t.type == BlockType::PAD_DATA)
		{
			for (int i = 0; i < t.alignRowCount; ++i)
			{
				BlockUserData* userData = new BlockUserData(PAD_BLOCK);
				userData->setSrcBlockNum(content.size());
				userData->m_lineEndType = UNKNOWN_LINE;//其实应该是PAD_LINE，为了和未知行位保持一致，只能写UNKNOWN_LINE
				externBlockInfo.append(userData);

				//还要加一个苹果的才好
#ifdef _WIN32
				content.append(QString("\r\n"));
#else
				content.append(QString("\n"));
#endif
			}
#ifdef _WIN32
			curCharPos += 2 * t.alignRowCount;
#else
			curCharPos += t.alignRowCount;
#endif

		}
	}
}

//在删除行之前（行没有删除掉前），先清除掉行现在的mark。20210722添加，坑太他妈的多了
//这个函数执行时，要删除的行还没有消失，还在界面中。该函数中不能调用QCoreApplication::processEvents()
//否则一旦调用后，行就立刻被删除了，导致没有删除行的marker之前，行就被删除掉了。
//后续在继续删除行的marker时，发现无法删除掉marker导致残留。

//复现步骤如下：1）前面一行A有标签 
//2）下面一行B有标签
//3）从B的行首开始删除，会导致删除是从A开始，而B直接变到A原来的位置去。
//4）后续删除B的时候，也要删除B的marker，会发现不知道咋地，B的marker就是删除不掉。严重怀疑是B覆盖掉了
//A，A的如果不调用deleteLinesMarker进行删除，后续会残留在B上面，无论如何都删除不掉
//虽然比较难描述，但真的是一个问题。
//所以我们要遵循一个原则：在删除行之前，截获删除消息，在行还没有消失之前，就执行行的marker操作。
//这个原则如果遵循，则不会出错。但是【！！！注意该函数中千万不能执行QCoreApplication::processEvents()】】。因为一旦执行，行就立刻被删除
//界面也立刻被更新，真不是一般的坑
//每删除一个Marker也会触发一次slot_left_SCN_MODIFIED 巨坑，要避免这样循环触发 20210725

//20210729 前几天已经发现删除不掉mareker的故障，也修改了qscint源码，可以一次性删除所有marker
void CompareWin::deleteLinesMarker(int dire, int startLine, int delNum)
{
	QsciDisplayWindow* uiSrc = ((dire == 0) ? ui.leftSrc : ui.rightSrc);

	for (int start = startLine + delNum - 1; start >= startLine; --start)
	{
		int markMask = uiSrc->SendScintilla(SCI_MARKERGET, start);
		if (markMask != 0)
		{
			//这个接口是我新增，删除一行中全部的marker
			//20210727 qscint有个问题，删除一行时，下一行的marker自动合并到上一行中去
			//但是它里面没有做去重处理，可能一行有多个同样重复的marker，导致删除时需要删很多次
			uiSrc->markerDelete(start);
		}
	}
}

//对对比结果进行过滤处理，去除一些明显其实就是相等的情况
void CompareWin::filterCompareResult(QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks)
{
	//return;
	assert(leftBlocks.size() == rightBlocks.size());

	//目前线去掉左边对齐、右边空白；左边空白、右边对齐这种明显相等的情况
	if (leftBlocks.size() <= 2)
	{
		return;
	}

	QVector<int> delIndex;

	for (int i = 0; i < leftBlocks.size() - 1; ++i)
	{
		BlockNode& left = leftBlocks[i];
		BlockNode& right = rightBlocks[i];

		BlockNode& leftNext = leftBlocks[i + 1];
		BlockNode& rightNext = rightBlocks[i + 1];

		//其实就是相等
		if ((left.type == BlockType::PAD_DATA && left.alignRowCount == 1)\
			&& (right.type == BlockType::REAL_DATA && right.lineNodes.size() == 1)\
			&& (right.lineNodes.first().lineText.size() == 1)\

			&& (leftNext.type == BlockType::REAL_DATA && leftNext.lineNodes.size() == 1)\
			&& (leftNext.lineNodes.first().lineText.size() == 1)\
			&& (rightNext.type == BlockType::PAD_DATA && rightNext.alignRowCount == 1) \

			&& (right.lineNodes.first().lineText.first().text == leftNext.lineNodes.first().lineText.first().text))
		{
			//现在还不能删除，先标记为删除。
			//把坐标的对齐删除
			left.type = BlockType::REAL_DATA;
			LineNode lineNode;
			lineNode.totalEqual = true;
			SectionNode secNode;
			secNode.text = right.lineNodes.first().lineText.first().text;
			lineNode.lineText.append(secNode);

			left.lineNodes.append(lineNode);

			right.lineNodes.first().totalEqual = true;

			delIndex.append(i + 1);

			//必须跳过一行
			++i;
		}
		//其实就是相等
		else if ((right.type == BlockType::PAD_DATA && right.alignRowCount == 1)\
			&& (left.type == BlockType::REAL_DATA && left.lineNodes.size() == 1)\
			&& (left.lineNodes.first().lineText.size() == 1)\

			&& (rightNext.type == BlockType::REAL_DATA && rightNext.lineNodes.size() == 1)\
			&& (rightNext.lineNodes.first().lineText.size() == 1)\
			&& (leftNext.type == BlockType::PAD_DATA && leftNext.alignRowCount == 1) \

			&& (left.lineNodes.first().lineText.first().text == rightNext.lineNodes.first().lineText.first().text))
		{
			//现在还不能删除，先标记为删除。
			//把坐标的对齐删除
			right.type = BlockType::REAL_DATA;
			LineNode lineNode;
			lineNode.totalEqual = true;
			SectionNode secNode;
			secNode.text = left.lineNodes.first().lineText.first().text;
			lineNode.lineText.append(secNode);

			right.lineNodes.append(lineNode);

			left.lineNodes.first().totalEqual = true;

			delIndex.append(i + 1);

			//必须跳过一行
			++i;
		}
	}

	for (int i = delIndex.size() - 1; i >= 0; --i)
	{
		leftBlocks.removeAt(delIndex[i]);
		rightBlocks.removeAt(delIndex[i]);
	}

}

void CompareWin::doCmpAfterEdit(RC_DIRECTION dir)
{
	RC_DIRECTION otherDir = ((dir == RC_LEFT) ? RC_RIGHT : RC_LEFT);
	QList<ModifyRecords*>& modifyRecord = ((dir == RC_LEFT) ? m_leftModifyRecord : m_rightModifyRecord);

	QsciDisplayWindow* uiSrc = ((dir == RC_LEFT) ? ui.leftSrc : ui.rightSrc);
	QsciDisplayWindow* uiOtherSrc = ((dir == RC_LEFT) ? ui.rightSrc : ui.leftSrc);

	QList<BlockUserData*>* externBlockInfo = ((dir == RC_LEFT) ? m_leftExternBlockInfo : m_rightExternBlockInfo);
	QList<BlockUserData*>* otherExternBlockInfo = ((dir == RC_LEFT) ? m_rightExternBlockInfo : m_leftExternBlockInfo);

	if (!modifyRecord.isEmpty())
	{
		//左边编辑了，去同步右边
		int addLineNums = 0;
		int delLineNums = 0;
		int addOrDelText = 0; //1 纯增加 2:减少

		for (auto it = modifyRecord.begin(); it != modifyRecord.end(); ++it)
		{
			int adds = (*it)->linesAdded;
			if (adds > 0)
			{
				addLineNums += adds;
			}
			else if (adds < 0)
			{
				delLineNums -= adds;
			}

			if ((*it)->modificationType == SC_MOD_INSERTTEXT)
			{
				addOrDelText |= 0x1;
			}
			else if ((*it)->modificationType == SC_MOD_DELETETEXT)
			{
				addOrDelText |= 0x2;
			}
		}

		//这里就是发生了行数的加删的。20210719采用新思路
		//1 获取修改块
		//2 扩展到周围完整的一个不等块
		//3 对比内容，删除旧内容，插入新内容
		//4 把没有发生行增加删除的情况也统一进去20210721
		ModifyRecords* record = nullptr;

		if (addOrDelText == 1)
		{
			//纯增加。取第一个修改记录为开始修改位置
			record = modifyRecord.first();
		}
		else if (addOrDelText == 2)
		{
			//纯删除。取最后一个修改记录为开始修改位置
			record = modifyRecord.last();
		}
		else if (addOrDelText == 3)
		{
			//这里有加有删，肯定是先删后加。发生在替换的情况
			record = modifyRecord.last();
		}
		else
		{
			return;
		}

		int startPos = record->position;
		int firstStartLine = uiSrc->SendScintilla(SCI_LINEFROMPOSITION, startPos);

		//有必要但是目前没用上，注释掉，但是别删除，后面还是有用
		int secondStartLine = firstStartLine;
		int secondEndLine = 0;

		int firstCmpLines = 0;
		int externFirstCmpLines = 0;

		int secondCmpLines = 0;
		//编辑完毕后的位置
		int firstEditFinishPos = 0;
		int firstLineNums = externBlockInfo->size();

		bool isLineAddOrDel = true;

		//没有行的增加和删除
		if (addLineNums == 0 && delLineNums == 0)
		{
			//leftEndLine = firstStartLine;
			firstCmpLines = 1;

			secondEndLine = firstStartLine;
			secondCmpLines = 1;

			//针对最后最后一行修改的情况做处理
			//修改的行号码，已经超过了当前具有的m_leftExternBlockInfo的最后一行
			if (firstStartLine >= firstLineNums)
			{
				//这种情况右边是没有内容的，所以不需要获取对比
				secondCmpLines = 0;
			}

			isLineAddOrDel = false;
		}
		else if (addLineNums > 0 && delLineNums == 0)
		{
			if (firstStartLine <= (firstLineNums - 1))
			{
				//处理参与比较的1+addLineNums外，还有先加入的addLineNums行
				//leftEndLine = firstStartLine + addLineNums + addLineNums;

				firstCmpLines = 1 + addLineNums;
				externFirstCmpLines = addLineNums;


				//考虑在最后行操作越界的情况
				if (firstStartLine + addLineNums > (firstLineNums - 1))
				{
					//超过了现有行，只能取有限行
					externFirstCmpLines = firstLineNums - firstStartLine - 1;

					if (externFirstCmpLines < 0)
					{
						externFirstCmpLines = 0;
					}
				}

				//对比左边，取出1+addLineNums参与比较。第1行就是firstStartLine
				secondEndLine = firstStartLine + addLineNums;
				secondCmpLines = 1 + externFirstCmpLines;
			}
			else
			{
				//assert(firstStartLine == firstLineNums);
				//在当前行的下一行进行了操作，即在有效行的最后一行的后一行进行了操作
				//处理参与比较的1+addLineNums外，还有先加入的addLineNums行
				//leftEndLine = firstStartLine + addLineNums;
				firstCmpLines = 1 + addLineNums;

				secondEndLine = firstStartLine + addLineNums;
				secondCmpLines = 0;

			}


		}
		else if (addLineNums == 0 && delLineNums > 0)
		{
			//leftEndLine = firstStartLine;
			firstCmpLines = 1;

			secondEndLine = firstStartLine + delLineNums;
			secondCmpLines = 1 + delLineNums;

			//删除的情况不一样，如果在最后一行删除，开始值不会是最后一行，之需考虑尾巴即可
			//此时右边对比的值需要减少1行，因为最后那个其实不是有效的行。仔细想一想
			if (secondEndLine > (firstLineNums - 1))
			{
				secondCmpLines = firstLineNums - firstStartLine;
			}

		}
		else if (addLineNums > 0 && delLineNums > 0)
		{
			assert(modifyRecord.size() <= 2);

			//加的比删的多
			if (addLineNums >= delLineNums)
			{
				//leftEndLine = firstStartLine + addLineNums;
				firstCmpLines = 1 + addLineNums;

				secondEndLine = firstStartLine + delLineNums;
				secondCmpLines = 1 + delLineNums;

				if (secondEndLine > (firstLineNums - 1))
				{
					secondCmpLines = firstLineNums - firstStartLine;
				}
			}
			else
			{
				//删比加多
				//leftEndLine = firstStartLine + addLineNums;
				firstCmpLines = 1 + addLineNums;

				secondEndLine = firstStartLine + delLineNums;
				secondCmpLines = 1 + delLineNums;

				if (secondEndLine > (firstLineNums - 1))
				{
					secondCmpLines = firstLineNums - firstStartLine;
				}
			}
		}


		//确定位置
		//增加
		ModifyRecords* lastRecord = modifyRecord.last();

		if ((lastRecord->modificationType & SC_MOD_INSERTTEXT) != 0)
		{
			firstEditFinishPos = lastRecord->position + lastRecord->length;
		}
		else if ((lastRecord->modificationType & SC_MOD_DELETETEXT) != 0)
		{
			//删除
			firstEditFinishPos = lastRecord->position;
		}

		//取出范围内的文本

		QVector<BlockNode> firstBlocks;
		QVector<BlockNode> secondBlocks;

		QVector<UnequalCharsPosInfo> firstUnqealPosList;
		QVector<UnequalCharsPosInfo> secondUnqealPosList;

		//20210724 这里发现可能崩溃，在界面上的最后一行添加字符，最后一行其实不是文本的内容，不过因为换行导致界面显示下面有一个空的行。
		//但是这个空行其实没有m_leftExternBlockInfo中对应的元素

		outputMultiLineCompareResult(firstStartLine, firstCmpLines, externFirstCmpLines, \
			secondStartLine, secondCmpLines, \
			uiSrc, uiOtherSrc, \
			firstBlocks, secondBlocks);

		filterCompareResult(firstBlocks, secondBlocks);

		QStringList firstContent;
		QStringList secondContent;

		QList<BlockUserData*> firstExternBlockInfo;
		QList<BlockUserData*> secondExternBlockInfo;

		QVector<int> firstLineStatus;
		QVector<int> secondLineStatus;

		getCompareUnqealPos(firstBlocks, firstContent, firstExternBlockInfo, firstUnqealPosList, firstLineStatus);
		getCompareUnqealPos(secondBlocks, secondContent, secondExternBlockInfo, secondUnqealPosList, secondLineStatus);

		//1）第一步，先把左右修改的行获取出来，在内存中进行对比完毕
		//2)删除一边对应修改的行。先删除了行上面的marker

		//删除左边的这么多行：从修改行开始，新增加的行，还有后面额外的externLeftCmpLines行，全部删除掉
		for (int start = firstStartLine + firstCmpLines + externFirstCmpLines - 1; start >= firstStartLine; --start)
		{
			//先删除marker和纹理后，再删除行，顺序不能乱

			//删除行目前所有的marken，发现删除一行后，下行会上前到前面一行
			//但是他们的market居然会叠加，所有和增加不一样，删除必须采用删除所有marker的方法
			int markMask = uiSrc->SendScintilla(SCI_MARKERGET, start);
			if (markMask != 0)
			{
				//这个接口是我新增，删除一行中全部的marker
				//20210727 qscint有个问题，删除一行时，下一行的marker自动合并到上一行中去
				//但是它里面没有做去重处理，可能一行有多个同样重复的marker，导致删除时需要删很多次
				uiSrc->markerDelete(start);
			}

#if 0
			//没有行增加和删除，则不删除现有行
			if (isLineAddOrDel)
			{
				//先删除marker和纹理后，再删除行，顺序不能乱
				uiSrc->SendScintilla(SCI_GOTOLINE, start);
				uiSrc->SendScintilla(SCI_LINEDELETE, start);
			}
#endif
		}

		//删除掉需要删除的m_leftExternBlockInfo中的内容
		//从firstStartLine开始，而需要删除的长度是rightCmpLines，仔细思考，长度的确是rightCmpLines
		//右边其实也是一样
#ifdef OPEN_UNDO_REDO
		UserDataCommand* uc = new UserDataCommand(this);
		uc->setChangeStatus(NONE_CHANGE);
		uc->setOperIndex(m_commandIndex);

		//巨坑;20210804 这里一定要注意顺序。不能直接append。一定是先记录所有的修改内容后，在记录userdata变化
		//而且userdata在还原时，也时需要最后做的，而不能在前面做。否则乱序导致文本混乱

		//往前面插入,找到该相同index的最前面位置插入
		int insertPos = 0;
		for (insertPos = m_undoList.size() - 1; insertPos >= 0; --insertPos)
		{
			if (m_undoList.at(insertPos)->getOperIndex() != m_commandIndex)
			{
				break;
			}
		}

		m_undoList.insert(insertPos + 1, uc);


		UserDataOperRecords* userDataRecord = new UserDataOperRecords(dir);
		userDataRecord->operType = DEL_USER_DATA;

		uc->addRecord(userDataRecord);

#endif
		//3)再删除行对应的userdata信息，由于有撤销功能，故没有删除userdata，而是放到撤销的记录中去。注意：这里记录时
		//userdata中的记录还是旧的信息，及没有进行编辑之前的信息。这里一定要配合后面恢复时的时序，否则会崩溃。20211015记录
		//文本三个阶段：1）最开始文本 2）编辑后文本 3）编辑后重新进行对比后文本。下面会使用这个文本阶段的概念。这个userdata还是1）阶段的记录

		for (int start = firstStartLine + secondCmpLines - 1; start >= firstStartLine; --start)
		{

#ifdef OPEN_UNDO_REDO
			//把删除的格式保存起来
			BlockUserData* data = externBlockInfo->at(start);
			externBlockInfo->removeAt(start);

			userDataRecord->userData.push_front(data);
#else
			//把格式也删除
			BlockUserData* data = externBlockInfo->at(start);
			externBlockInfo->removeAt(start);
			delete data;
#endif
		}

		//左边新插入对比后的行。这里其实也会有isModifyFromLineFirstChar面临同样的问题。
		//由于肯定是从第一个字符插入，当前行被下移，而其marker肯定也跟随下移行动，所以与目前期望的结果是一样的，但是要明白有这个问题。
		startPos = uiSrc->SendScintilla(SCI_POSITIONFROMLINE, firstStartLine);

		//4）进行文本的替换。把修改后的记录，和重新对比校正过的文本，进行替换处理
		if (isLineAddOrDel)
		{
			int endPos = uiSrc->SendScintilla(SCI_POSITIONFROMLINE, firstStartLine + firstCmpLines + externFirstCmpLines);

			//如果超过了，则返回最大文本长度
			if (endPos == -1)
			{
				endPos = uiSrc->length();
			}

#ifdef OPEN_UNDO_REDO
			//把原来的内容收集起来。这里的原来，也是代表2阶段的文本，及修改后当前的文本，而不是原来修改前文本，也不是校正后文本。
			ReplaceOperRecords* replaceRecord = new ReplaceOperRecords();
			replaceRecord->dir = dir;
			replaceRecord->startPos = startPos;
			replaceRecord->srcEndPos = endPos;

			struct Sci_TextRange delTextRange;

			delTextRange.chrg.cpMin = startPos;
			delTextRange.chrg.cpMax = endPos;

			delTextRange.lpstrText = new char[endPos - startPos + 1];
			memset(delTextRange.lpstrText, 0, endPos - startPos + 1);
			//拷贝原始的内容
			uiSrc->SendScintilla(SCI_GETTEXTRANGE, 0, &delTextRange);
			replaceRecord->srcContents = delTextRange.lpstrText;


			//把替换文本记录在replaceContents，后续撤销需要使用
			std::string text = firstContent.join("").toStdString();
			replaceRecord->replaceLens = text.length();

			char* replaceContens = new char[replaceRecord->replaceLens + 1];
			memset(replaceContens, 0, replaceRecord->replaceLens + 1);
			memcpy(replaceContens, text.data(), replaceRecord->replaceLens);

			replaceRecord->replaceContents = replaceContens;

			ReplaceCommand* ec = new ReplaceCommand(this);
			ec->setChangeStatus((ChangeDire)dir);
			ec->addRecord(replaceRecord);
			ec->setOperIndex(m_commandIndex);

			m_undoList.append(ec);

			replaceText(dir, startPos, endPos, text.length(), text.data());

#endif
		}
		else
		{
			//没有加删行，则文字全部变黑

			setLineStyle(uiSrc, STYLE_EQUAL_TEXT_COLOUR, firstStartLine);
			//setLineStyle(uiSrc, STYLE_BACK_COLOUR_EQUAL, firstStartLine);

			//int length = uiSrc->SendScintilla(SCI_LINELENGTH, firstStartLine);

			//uiSrc->SendScintilla(SCI_STARTSTYLING, startPos);
			//uiSrc->SendScintilla(SCI_SETSTYLING, length, (long)STYLE_EQUAL_TEXT_COLOUR );
			//uiSrc->SendScintilla(SCI_SETSTYLING, length, (long)STYLE_BACK_COLOUR_EQUAL);
		}

#ifdef _test
		QCoreApplication::processEvents();
#endif

		//5）根据最新对比信息，设置不同文字的颜色为红色
		for (auto it = firstUnqealPosList.begin(); it != firstUnqealPosList.end(); ++it)
		{
			uiSrc->SendScintilla(SCI_STARTSTYLING, startPos + it->start);
			uiSrc->SendScintilla(SCI_SETSTYLING, it->length, (long)STYLE_COLOUR_RED);
#ifdef _test
			QCoreApplication::processEvents();
#endif
		}


#ifdef OPEN_UNDO_REDO

		userDataRecord = new UserDataOperRecords(dir);
		userDataRecord->operType = ADD_USER_DATA;

		uc->addRecord(userDataRecord);
#endif

		//6）再插入新的userdata，主要更新其中的m_srcBlockNum值
		for (int index = 0, size = firstExternBlockInfo.size(); index < size; ++index)
		{
			BlockUserData* v = firstExternBlockInfo.at(index);
			v->m_srcBlockNum = firstStartLine + index;

			externBlockInfo->insert(firstStartLine + index, v);

#ifdef OPEN_UNDO_REDO
			userDataRecord->userData.append(new BlockUserData(*v));
#endif

			if (v->m_blockType == UNEQUAL_BLOCK)
			{
				uiSrc->markerDelete(firstStartLine + index, STYLE_BACK_COLOUR_EQUAL);
				uiSrc->markerAdd(firstStartLine + index, STYLE_BACK_COLOUR_NO_EQUAL);
			}
			else if (v->m_blockType == EQUAL_BLOCK)
			{
				uiSrc->markerDelete(firstStartLine + index, STYLE_BACK_COLOUR_NO_EQUAL);
				uiSrc->markerAdd(firstStartLine + index, STYLE_BACK_COLOUR_EQUAL);
			}
			else if (v->m_blockType == PAD_BLOCK)
			{
				uiSrc->markerAdd(firstStartLine + index, STYLE_PAD_BACK_PIXMAP);
			}
#ifdef _test
			QCoreApplication::processEvents();
#endif
		}

		//7)因为插入修改了整体的userdata，可能里面偏移错乱，调整userdata中的偏移
		//因为m_leftExternBlockInfo中插入了leftExternBlockInfo.size()行新内容，更新现有m_leftExternBlockInfo中后续的行号内容
		//m_leftExternBlockInfo先删除了修改的行，后插入了修改后对齐的行
		int oldBlockNum = 0;
		int newBlockNum = 0;

		//20211015这里最近就发现过一个问题，如果在编辑框的行长度超过externBlockInfo不止1行，比如超过2行，当在超过的第2行编辑的时候
		//这里条件不会满足，也不会调整。所以后续在setlast中规定了，编辑框最多不能超过externBlockInfo1行，这样的话，尾部从firstStartLine
		//开始新增的userdata都是新增，其偏移肯定都是大于externBlockInfo现有偏移的，现有externBlockInfo偏移不需要调整。而编辑框最多不超过
		//externBlockInfo 1行的保证，也确保了firstStartLine最大只会是externBlockInfo.size,这样保证了偏移都是连续的，不会出现externBlockInfo最后一个
		//与firstStartLine中间还有一个差异的问题。不要小看这个问题，20211014就测试出来，导致后续不同块的生成错误。
		if (firstStartLine + firstExternBlockInfo.size() < externBlockInfo->size())
		{
			oldBlockNum = externBlockInfo->at(firstStartLine + firstExternBlockInfo.size())->m_srcBlockNum;
			newBlockNum = firstStartLine + firstExternBlockInfo.size();

			if (oldBlockNum != newBlockNum)
			{
				int interNum = newBlockNum - oldBlockNum;

				for (int start = firstStartLine + firstExternBlockInfo.size(), size = externBlockInfo->size(); start < size; ++start)
				{
					externBlockInfo->at(start)->m_srcBlockNum += interNum;
				}
			}
		}

		///左边的处理完毕，现在轮到右边
#if 0
#ifdef OPEN_UNDO_REDO
//把删除的内容记录下来
		if (isLineAddOrDel)
		{
			int startPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, secondStartLine);
			int endPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, secondStartLine + secondCmpLines);


			//超过了最大行。则取前面1行的开始，加上其长度，因为后面有换行符
			//不能使用SCI_GETLINEENDPOSITION，因为SCI_GETLINEENDPOSITION没有包含换行符号
			if (endPos == -1)
			{
				int lastLineNum = uiOtherSrc->SendScintilla(SCI_GETLINECOUNT) - 1;

				endPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, lastLineNum);
				int lineLens = uiOtherSrc->SendScintilla(SCI_LINELENGTH, lastLineNum);
				endPos += lineLens;
			}

			struct Sci_TextRange delTextRange;

			delTextRange.chrg.cpMin = startPos;
			delTextRange.chrg.cpMax = endPos;

			int length_ = endPos - startPos;
			int linesAdded_ = (firstCmpLines + externFirstCmpLines);

			delTextRange.lpstrText = new char[endPos - startPos + 1];
			memset(delTextRange.lpstrText, 0, endPos - startPos + 1);
			//拷贝左边的内容
			uiOtherSrc->SendScintilla(SCI_GETTEXTRANGE, 0, &delTextRange);

			ModifyOperRecords* delRecord = new ModifyOperRecords(startPos, SC_MOD_DELETETEXT, length_, linesAdded_);
			delRecord->contents = delTextRange.lpstrText;
			delRecord->dir = otherDir;

			EditCommand* ec = new EditCommand(this);
			ec->addRecord(delRecord);
			ec->setOperIndex(m_commandIndex);

			m_undoList.append(ec);

		}
#endif
#endif

#ifdef OPEN_UNDO_REDO

		userDataRecord = new UserDataOperRecords(otherDir);
		userDataRecord->operType = DEL_USER_DATA;

		uc->addRecord(userDataRecord);

#endif

		//替换右边新对比后的内容，再把格式重新设定
		for (int start = secondStartLine + secondCmpLines - 1; start >= secondStartLine; --start)
		{

			int markMask = uiOtherSrc->SendScintilla(SCI_MARKERGET, start);
			if (markMask != 0)
			{
				//这个接口是我新增，删除一行中全部的marker
				//20210727 qscint有个问题，删除一行时，下一行的marker自动合并到上一行中去
				//但是它里面没有做去重处理，可能一行有多个同样重复的marker，导致删除时需要删很多次
				uiOtherSrc->markerDelete(start);
			}

#ifdef OPEN_UNDO_REDO
			BlockUserData* data = otherExternBlockInfo->takeAt(start);
			userDataRecord->userData.push_front(data);
#else
			//把格式也删除
			BlockUserData* data = otherExternBlockInfo->takeAt(start);
			delete data;
#endif


#ifdef _test
			QCoreApplication::processEvents();
#endif
		}

		//右边新插入对比后的行
		int otherStartPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, secondStartLine);

		if (isLineAddOrDel)
		{

			int endPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, secondStartLine + secondCmpLines);

			//如果超过了，则返回最大文本长度
			if (endPos == -1)
			{
				endPos = uiOtherSrc->length();
			}

			ReplaceOperRecords* replaceRecord = new ReplaceOperRecords();
			replaceRecord->dir = otherDir;
			replaceRecord->startPos = otherStartPos;
			replaceRecord->srcEndPos = endPos;

			struct Sci_TextRange textRange;

			textRange.chrg.cpMin = otherStartPos;
			textRange.chrg.cpMax = endPos;

			textRange.lpstrText = new char[endPos - otherStartPos + 1];
			memset(textRange.lpstrText, 0, endPos - otherStartPos + 1);
			//拷贝原始的内容
			uiOtherSrc->SendScintilla(SCI_GETTEXTRANGE, 0, &textRange);

			replaceRecord->srcContents = textRange.lpstrText;

			std::string text = secondContent.join("").toStdString();
			replaceRecord->replaceLens = text.length();

			char* replaceContens = new char[replaceRecord->replaceLens + 1];
			memset(replaceContens, 0, replaceRecord->replaceLens + 1);
			memcpy(replaceContens, text.data(), replaceRecord->replaceLens);

			replaceRecord->replaceContents = replaceContens;

			ReplaceCommand* ec = new ReplaceCommand(this);
			ec->setChangeStatus((ChangeDire)dir);
			ec->addRecord(replaceRecord);
			ec->setOperIndex(m_commandIndex);

			m_undoList.append(ec);

			replaceText(otherDir, otherStartPos, endPos, text.length(), text.data());

		}
		else
		{
			//没有加删行，则文字全部变黑

			setLineStyle(uiOtherSrc, STYLE_EQUAL_TEXT_COLOUR, secondStartLine);
			//setLineStyle(uiOtherSrc, STYLE_BACK_COLOUR_EQUAL, secondStartLine);

			//int length = uiOtherSrc->SendScintilla(SCI_LINELENGTH, secondStartLine);

			//uiOtherSrc->SendScintilla(SCI_STARTSTYLING, otherStartPos);
			//uiOtherSrc->SendScintilla(SCI_SETSTYLING, length, (long)STYLE_EQUAL_TEXT_COLOUR);
			//uiOtherSrc->SendScintilla(SCI_SETSTYLING, length, (long)STYLE_BACK_COLOUR_EQUAL);
		}
#ifdef _test
		QCoreApplication::processEvents();
#endif

		for (auto it = secondUnqealPosList.begin(); it != secondUnqealPosList.end(); ++it)
		{
			uiOtherSrc->SendScintilla(SCI_STARTSTYLING, otherStartPos + it->start);
			uiOtherSrc->SendScintilla(SCI_SETSTYLING, it->length, (long)STYLE_COLOUR_RED);
#ifdef _test
			QCoreApplication::processEvents();
#endif
		}

#ifdef OPEN_UNDO_REDO

		userDataRecord = new UserDataOperRecords(otherDir);
		userDataRecord->operType = ADD_USER_DATA;

		uc->addRecord(userDataRecord);

#endif

		//再插入新的userdata
		for (int index = 0, size = secondExternBlockInfo.size(); index < size; ++index)
		{
			BlockUserData* v = secondExternBlockInfo.at(index);
			v->m_srcBlockNum = secondStartLine + index;

			otherExternBlockInfo->insert(secondStartLine + index, v);

#ifdef OPEN_UNDO_REDO
			userDataRecord->userData.append(new BlockUserData(*v));
#endif

			if (v->m_blockType == UNEQUAL_BLOCK)
			{
				uiOtherSrc->markerDelete(secondStartLine + index, STYLE_BACK_COLOUR_EQUAL);
				uiOtherSrc->markerAdd(secondStartLine + index, STYLE_BACK_COLOUR_NO_EQUAL);
			}
			else if (v->m_blockType == EQUAL_BLOCK)
			{
				uiOtherSrc->markerDelete(secondStartLine + index, STYLE_BACK_COLOUR_NO_EQUAL);
				uiOtherSrc->markerAdd(secondStartLine + index, STYLE_BACK_COLOUR_EQUAL);
			}
			else if (v->m_blockType == PAD_BLOCK)
			{
				uiOtherSrc->markerAdd(secondStartLine + index, STYLE_PAD_BACK_PIXMAP);
			}
#ifdef _test
			QCoreApplication::processEvents();
#endif
		}

		//更新现有leftExternBlockInfo中的行号内容
		if (oldBlockNum != newBlockNum)
		{
			int interNum = newBlockNum - oldBlockNum;

			for (int start = secondStartLine + secondExternBlockInfo.size(), size = otherExternBlockInfo->size(); start < size; ++start)
			{
				otherExternBlockInfo->at(start)->m_srcBlockNum += interNum;
			}
		}

#ifdef _test
		QCoreApplication::processEvents();
#endif

		setLastBlockFormat(m_leftExternBlockInfo, m_rightExternBlockInfo);

		createNonEqualBlock(m_leftExternBlockInfo, m_rightExternBlockInfo, m_leftNoEqualBlocks, m_rightNoEqualBlocks);
		megreUnEqualBlock();

		cleanMargins(dir);
		cleanMargins(otherDir);

		createMargins(m_leftExternBlockInfo, m_rightExternBlockInfo);



		for (auto it = modifyRecord.begin(); it != modifyRecord.end(); ++it)
		{
			delete[] * it;
		}

		modifyRecord.clear();

		++m_commandIndex;

		//20210808 这里做一下当前行的光标调整

		uiSrc->SendScintilla(SCI_GOTOPOS, firstEditFinishPos);
	}
}


//替换文本
void CompareWin::replaceText(RC_DIRECTION dir, int start, int end, int textLen, const char* text)
{
	QsciDisplayWindow* uisrc = ((dir == RC_LEFT) ? ui.leftSrc : ui.rightSrc);

	uisrc->SendScintilla(SCI_SETTARGETRANGE, start, end);
	uisrc->SendScintilla(SCI_REPLACETARGET, textLen, text);

#ifdef _test
	QCoreApplication::processEvents();
#endif
}

//开始编辑后的对比
void CompareWin::slot_doCmpAfterEdit(int direction)
{
	disableTextChangeSignal();

	doCmpAfterEdit(static_cast<RC_DIRECTION>(direction));

	enableTextChangeSignal();

	if (RC_LEFT == direction)
	{
		asyncVerticalScrollValue(RC_LEFT);
	}
	else
	{
		asyncVerticalScrollValue(RC_RIGHT);
	}

	ui.leftSrc->update();
	ui.rightSrc->update();

	ui.statusBar->showMessage(tr("current has %1 differents").arg(m_leftNoEqualBlocks.count()));

#ifdef _DEBUG
	check();
#endif

	updateDiffStatusWin(ui.leftSrc->lines(), m_leftNoEqualBlocks);
}

//右边的发生了文字的修改。完全参考左边的镜像修改而来
void CompareWin::slot_right_SCN_MODIFIED(int position, int modificationType, const char* text, int length, int linesAdded, int /*line*/, int /*foldLevelNow*/, int /*foldLevelPrev*/, int /*token*/, int /*annotationLinesAdded*/)
{

	//1是增加，2是删除
	modificationType &= (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);

	//不是关于增加与修改文字的修改
	if (modificationType == 0 || (text == nullptr))
	{
		return;
	}

	if (m_rightOperStatus != OperStatus::RC_NONE)
	{
		return;
	}


	ModifyRecords* record = new ModifyRecords(position, modificationType, length, linesAdded);
	record->isInPaste = ui.leftSrc->isInPasteStatus();

	m_rightModifyRecord.append(record);

#ifdef OPEN_UNDO_REDO
	ModifyOperRecords* operRecord = new ModifyOperRecords(position, modificationType, length, linesAdded);
	operRecord->dir = RC_RIGHT;
	operRecord->contents = new char[length + 1];
	memset(operRecord->contents, 0, length + 1);
	memcpy(operRecord->contents, text, length);

	EditCommand* ec = new EditCommand(this);
	ec->setChangeStatus(RIGHT_CHANGE);
	ec->addRecord(operRecord);

	ec->setOperIndex(m_commandIndex);

	m_undoList.append(ec);
#endif

	//20210707 可能增加行，可能减少行，也会同时出现增加和删除行的情况
	//但是可能会出现先删除字符，有增加字符的情况，这种情况出现在拷贝替换的过程。

	//注意，替换的过程就是先删除、再添加。如果是多行替换少行，则是先删除几行，删除后再添加，但是两次的pos位置是一样的
	//如果是少替换多，也是先删除行，删除后再添加。记住：可以尝试最多只处理2个触发合并为一的情况。
	//因为替换就是先删除、再添加，这两次是一个动作的触发，需要合并一起处理。而且这两次的pos一定相等

	//20210722 如果第一个修改是删除行，则需要延迟处理。因为替换就是先删除/后添加，二者合一处理比较好
	//而如果是纯粹增加行，则直接处理。
	//这样的想法是好的，发现一个bug，左边的内容在删除时，无法生效！可能时直接执行，还没有退出当前的执行流程导致。必须要走延迟sendModify处理
	//这里先留着，等于是给自己留一个教训，避免后面又放开这里，犯了同样的错误。

	//这里其实还没有执行真正的行删除，至少界面上还没有更新，所以在这里执行删除行marker的操作是比较好的。
	//20210722发现一个问题，当在行的首字符，执行删除或添加时，marker容易残留，而且删除的时候压根删除不掉，可能时时机太晚，故提前到这里
	//复现过程见deleteLineMarkerBeforeDelLine的说明。只有这里不执行QCoreApplication::processEvents()，行就还在，就可以在这里对行执行删除marker操作
	//这里删除的行数量，可能不是太精确，还可以再斟酌一下


	emit sendModify(1, record);


	if (!m_isRightDirty)
	{
		m_isRightDirty = true;
		syncNeedSaveIcon();
	}

	//如果是删除内容，则要注意，后面很可能短期来一个新增的消息。比如替换时，总是先删除，再增加。
	//而如果是增加，则往往直接就是增加。对于删除，延时半秒的时间。没有更好的办法，还是只能严格的来一个修改，做一次修正。

	//qDebug() << "pos:" << position << "text: " << text << "lens:" << length << "linesadd:" << linesAdded << line;

}

void CompareWin::setUserDataMarkerFlag(int dire, int line, int status)
{
	if (dire == RC_LEFT)
	{
		m_leftExternBlockInfo->value(line)->m_MarkerFlag = status;
	}
	else
	{
		m_rightExternBlockInfo->value(line)->m_MarkerFlag = status;
	}
}

void CompareWin::doMarginClicked(RC_DIRECTION dir, int /*margin*/, int line, Qt::KeyboardModifiers /*state*/)
{

	RC_DIRECTION curDir = dir;
	RC_DIRECTION otherDir = ((dir == RC_LEFT) ? RC_RIGHT : RC_LEFT);

	QList<NoEqualBlock>& noEqualBlocks = ((curDir == RC_LEFT) ? m_leftNoEqualBlocks : m_rightNoEqualBlocks);
	QList<NoEqualBlock>& otherNoEqualBlocks = ((curDir == RC_LEFT) ? m_rightNoEqualBlocks : m_leftNoEqualBlocks);
	QsciDisplayWindow* uiSrc = ((curDir == RC_LEFT) ? ui.leftSrc : ui.rightSrc);
	QsciDisplayWindow* uiOtherSrc = ((curDir == RC_LEFT) ? ui.rightSrc : ui.leftSrc);

	QList<BlockUserData*>* externBlockInfo = ((curDir == RC_LEFT) ? m_leftExternBlockInfo : m_rightExternBlockInfo);
	QList<BlockUserData*>* otherExternBlockInfo = ((curDir == RC_LEFT) ? m_rightExternBlockInfo : m_leftExternBlockInfo);

	if (m_pCmpMode == nullptr || noEqualBlocks.isEmpty())
	{
		return;
	}

	NoEqualBlock findIndexBlock(line, 0);

	int index = noEqualBlocks.indexOf(findIndexBlock);
	if (-1 == index)
	{
		return;
	}

	disableTextChangeSignal();

	m_leftOperStatus = OperStatus::RC_SYNC;
	m_rightOperStatus = OperStatus::RC_SYNC;

	setAtEditStatus(true);

#ifdef _test
	QCoreApplication::processEvents();
#endif

	//因为肯定要删除一个，如果删除的不是第一个，则当前设置为删除的前一个
	if (index >= 1)
	{
		m_curShowBlockNums = index - 1;
	}
	else if (index == 0 && noEqualBlocks.size() >= 2)
	{
		//如果删除的是第一个，而且目前只有2个了，那么剩余必定只有1个，则直接设置为第0个
		m_curShowBlockNums = 0;
	}
	else
	{
		//这是就是全部删除完毕了
		m_curShowBlockNums = 0;
	}

	const NoEqualBlock bkinfo = noEqualBlocks.value(index);

#ifdef OPEN_UNDO_REDO
	//记录操作历史，便于后续做undo
	OperatorRecord* record = new OperatorRecord();
	record->dir = curDir;
	record->operatorType = RC_OPER_SYNC;

	OperatorInfo& oper = ((curDir == RC_LEFT) ? record->leftOperator : record->rightOperator);
	OperatorInfo& otherOper = ((curDir == RC_LEFT) ? record->rightOperator : record->leftOperator);

	oper.type = bkinfo.type;
	oper.startLineNums = line;
	oper.lineLens = bkinfo.blockLens;

	otherOper.type = otherNoEqualBlocks.value(index).type;
	otherOper.startLineNums = line;
	otherOper.lineLens = bkinfo.blockLens;

#endif

	//如果原来就是空块，即对齐块，则左右都直接删除
	if (bkinfo.type == PAD_BLOCK)
	{
		int endLine = line + bkinfo.blockLens - 1;


#ifdef OPEN_UNDO_REDO //左边不用获取，就获取右边内容收集起来
		//20210831注意一个坑，这个SCI_GETLINEENDPOSITION是没有包含文件行的换行符号的。
		//预计所有的获取文本内容的字段，都是没有算换行符在内的
		//所有要获取下一行的起始地址，这样选取才是对的。统一修改一下。
		//要注意是否可能最后一行很可能越界操作严重错误
		int startPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, line);
		int endPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, endLine + 1);

		//如果超过了，则返回最大文本长度
		if (endPos == -1)
		{
			endPos = uiOtherSrc->length();
		}

		struct Sci_TextRange textRange;

		textRange.chrg.cpMin = startPos;
		textRange.chrg.cpMax = endPos;

		textRange.lpstrText = new char[endPos - startPos + 1];
		memset(textRange.lpstrText, 0, endPos - startPos + 1);
		//拷贝对边的内容
		uiOtherSrc->SendScintilla(SCI_GETTEXTRANGE, 0, &textRange);

		otherOper.lineContents.push_front(textRange.lpstrText);
		otherOper.lineLength.push_front(endPos - startPos);
#endif

		//删除行的格式配置。倒着来，因为要删除，只能从尾部删除
		for (int start = endLine; start >= line; --start)
		{
			deleteLinesMarker(curDir, start, 1);
			uiSrc->SendScintilla(SCI_GOTOLINE, start);
			uiSrc->SendScintilla(SCI_LINEDELETE, start);

			deleteLinesMarker(otherDir, start, 1);
			uiOtherSrc->SendScintilla(SCI_GOTOLINE, start);

			uiOtherSrc->SendScintilla(SCI_LINEDELETE, start);
		}


#ifdef OPEN_UNDO_REDO
		oper.noEqualBlockInfo = noEqualBlocks.takeAt(index);
		oper.noEqualindex = index;
#else
		//删除掉一件被同步的元素
		noEqualBlocks.removeAt(index);
#endif


		for (int i = index; i < noEqualBlocks.size(); ++i)
		{
			noEqualBlocks[i].startBlockNums -= bkinfo.blockLens;
		}

#ifdef OPEN_UNDO_REDO
		otherOper.noEqualBlockInfo = otherNoEqualBlocks.takeAt(index);
		otherOper.noEqualindex = index;
#else
		//删除掉一件被同步的元素
		otherNoEqualBlocks.removeAt(index);
#endif

		for (int i = index; i < otherNoEqualBlocks.size(); ++i)
		{
			otherNoEqualBlocks[i].startBlockNums -= bkinfo.blockLens;
		}

		//把对应的行额外信息也删除掉
		for (int start = endLine; start >= line; --start)
		{
			BlockUserData* p = externBlockInfo->takeAt(start);

#ifdef OPEN_UNDO_REDO
			oper.lineExternInfo.push_front(p);
#else
			delete p;
#endif

			p = otherExternBlockInfo->takeAt(start);

#ifdef OPEN_UNDO_REDO
			otherOper.lineExternInfo.push_front(p);
#else
			delete p;
#endif
		}

		//由于删除了行，从删除行的下面开始，全部更新行的开始号码
		for (int i = line; i < externBlockInfo->size(); ++i)
		{
			externBlockInfo->at(i)->m_srcBlockNum -= bkinfo.blockLens;
		}

		for (int i = line; i < otherExternBlockInfo->size(); ++i)
		{
			otherExternBlockInfo->at(i)->m_srcBlockNum -= bkinfo.blockLens;
		}

		//更新其它额外行的行号开始信息
		uiSrc->viewport()->update();
		uiOtherSrc->viewport()->update();
	}
	else
	{
		//左边是不等块
		//使用左边的内容，去替换右边的内容
		int endLine = line + bkinfo.blockLens - 1;

		NoEqualBlock rightBlock = otherNoEqualBlocks.value(index);
		assert(rightBlock.startBlockNums == line);

		//删除行的格式配置。倒着来，因为要删除，只能从尾部删除
		for (int start = endLine; start >= line; --start)
		{
			//只删除格式
			deleteLinesMarker(curDir, start, 1);
			deleteLinesMarker(otherDir, start, 1);

#ifdef _test
			QCoreApplication::processEvents();
#endif

			setLineStyle(uiSrc, STYLE_EQUAL_TEXT_COLOUR, start);

			uiSrc->markerAdd(start, STYLE_BACK_COLOUR_EQUAL);
			uiOtherSrc->markerAdd(start, STYLE_BACK_COLOUR_EQUAL);
#ifdef _test
			QCoreApplication::processEvents();
#endif
			//setLineStyle(uiSrc, STYLE_BACK_COLOUR_EQUAL, start);

			//setLineStyle(uiOtherSrc, STYLE_EQUAL_TEXT_COLOUR, start);
			//setLineStyle(uiOtherSrc, STYLE_BACK_COLOUR_EQUAL, start);

	/*		int startPos = uiSrc->SendScintilla(SCI_POSITIONFROMLINE, start);
			uiSrc->SendScintilla(SCI_STARTSTYLING, startPos);

			int length = uiSrc->SendScintilla(SCI_LINELENGTH, start);
			uiSrc->SendScintilla(SCI_SETSTYLING, length, STYLE_EQUAL_TEXT_COLOUR );
			uiSrc->SendScintilla(SCI_SETSTYLING, length, STYLE_BACK_COLOUR_EQUAL);*/



		}

		int startPos = uiSrc->SendScintilla(SCI_POSITIONFROMLINE, line);
		int endPos = uiSrc->SendScintilla(SCI_POSITIONFROMLINE, endLine + 1);

		struct Sci_TextRange textRange;

		textRange.chrg.cpMin = startPos;
		textRange.chrg.cpMax = endPos;

		textRange.lpstrText = new char[endPos - startPos + 1];
		memset(textRange.lpstrText, 0, endPos - startPos + 1);

		//拷贝左边的内容
		uiSrc->SendScintilla(SCI_GETTEXTRANGE, 0, &textRange);


		startPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, line);
		endPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, endLine + 1);

#ifdef OPEN_UNDO_REDO

		struct Sci_TextRange otherTextRange;

		otherTextRange.chrg.cpMin = startPos;
		otherTextRange.chrg.cpMax = endPos;

		otherTextRange.lpstrText = new char[endPos - startPos + 1];
		memset(otherTextRange.lpstrText, 0, endPos - startPos + 1);
		//拷贝右边的内容
		uiOtherSrc->SendScintilla(SCI_GETTEXTRANGE, 0, &otherTextRange);

		//获取内容，收集起来。注意这个rightTextRange.lpstrText在此是没有删除的
		otherOper.lineContents.push_front(otherTextRange.lpstrText);
		otherOper.lineLength.push_front(endPos - startPos);
#endif

		uiOtherSrc->SendScintilla(SCI_DELETERANGE, startPos, endPos - startPos);
		uiOtherSrc->SendScintilla(SCI_INSERTTEXT, startPos, textRange.lpstrText);

		delete[]textRange.lpstrText;



#ifdef OPEN_UNDO_REDO

		oper.noEqualBlockInfo = noEqualBlocks.takeAt(index);
		otherOper.noEqualBlockInfo = otherNoEqualBlocks.takeAt(index);
		oper.noEqualindex = index;
		otherOper.noEqualindex = index;
#else
		//删除掉一件被同步的元素
		noEqualBlocks.removeAt(index);
		otherNoEqualBlocks.removeAt(index);
#endif

		//把对应的行额外信息修正为相等,因为左右已经同步过了
		for (int start = endLine; start >= line; --start)
		{
			BlockUserData* p = externBlockInfo->value(start);

#ifdef OPEN_UNDO_REDO
			oper.lineExternInfo.push_front(new BlockUserData(*p));
#endif
			p->m_blockType = EQUAL_BLOCK;

			p = otherExternBlockInfo->value(start);

#ifdef OPEN_UNDO_REDO
			otherOper.lineExternInfo.push_front(new BlockUserData(*p));
#endif
			p->m_blockType = EQUAL_BLOCK;

		}

	}

#ifdef OPEN_UNDO_REDO
	AsyncCommand* ac = new AsyncCommand(this);
	ac->setChangeStatus((ChangeDire)otherDir);
	ac->addRecord(record);
	ac->setOperIndex(m_commandIndex);
	m_undoList.append(ac);
	++m_commandIndex;
#endif



	setAtEditStatus(false);

	m_leftOperStatus = OperStatus::RC_NONE;
	m_rightOperStatus = OperStatus::RC_NONE;

	enableTextChangeSignal();


	asyncVerticalScrollValue(curDir);

	uiSrc->update();
	uiOtherSrc->update();
}

//点击左边去同步右边
void CompareWin::slot_leftMarginClicked(int margin, int line, Qt::KeyboardModifiers state)
{
	doMarginClicked(RC_LEFT, margin, line, state);

	if (!m_isRightDirty)
	{
		m_isRightDirty = true;
		syncNeedSaveIcon();
	}

	ui.statusBar->showMessage(tr("current has %1 differents").arg(m_leftNoEqualBlocks.count()));

	updateDiffStatusWin(ui.leftSrc->lines(), m_leftNoEqualBlocks);
}

void CompareWin::slot_rightMarginClicked(int margin, int line, Qt::KeyboardModifiers state)
{
	doMarginClicked(RC_RIGHT, margin, line, state);
	if (!m_isLeftDirty)
	{
		m_isLeftDirty = true;
		syncNeedSaveIcon();
	}

	ui.statusBar->showMessage(tr("current has %1 differents").arg(m_leftNoEqualBlocks.count()));
	updateDiffStatusWin(ui.leftSrc->lines(), m_leftNoEqualBlocks);
}


/* 从收集的修改内容，填充为一个BlocksInfo。 */
void CompareWin::createBlocksInfoFromStringList(QStringList &strList, BlocksInfo &blocksInfo, QVector<LineFileInfo> &linesInfo)
{
	int blockNum = 0;

	/* 如果是对齐块，需要跳过 */
	for (int i = 0; i < strList.count(); ++i)
	{

		/* 2020-6-21：m_leftBlockData存在越界访问：当文本最后新增时，由于是新增，其先前没有信息在m_leftBlockData中。 将对齐行过滤工作提前到
		* 收集strList的地方去
		*/

		LineFileInfo fileInfo;
		fileInfo.unicodeStr = strList.at(i);
		fileInfo.lineNums = blockNum;
		++blockNum;
		fileInfo.isLcsExist = false;
		fileInfo.isEmptyLine = fileInfo.unicodeStr.isEmpty();
		fileInfo.lineEndFormat = getLineEndType(fileInfo.unicodeStr);

		linesInfo.append(fileInfo);
	}

	blocksInfo.equal = false;
	blocksInfo.startLine = 0;
	blocksInfo.endLine = blockNum;
	blocksInfo.actualNums = blockNum;

}

/* 删除从位置start开始的用户块信息 */
void CompareWin::delUserBlockData(QList<UserBlockData> & blockData, int start, int len)
{
	for (int i = 0; i < len; ++i)
	{
		//从尾部删起,因为是链表，删了后面不影响前面序号
		blockData.removeAt(start + len - 1 - i);
	}
}


//中介者发过来的同步滚动条当前值信息
void CompareWin::slot_syncCurScrollValue(int direction)
{
	if (0 == direction)
	{
		//左边改变了，发出信号，右边进行同步
		ui.rightSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, m_mediator->getLeftScrollValue());
		//一旦对方同步后，则更新对方当前的值
		m_mediator->setRightScrollValue(m_mediator->getLeftScrollValue());

		ui.rightSrc->autoAdjustLineWidth(m_mediator->getRightScrollValue());
	}
	else
	{
		ui.leftSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, m_mediator->getRightScrollValue());
		m_mediator->setLeftScrollValue(m_mediator->getRightScrollValue());
		ui.leftSrc->autoAdjustLineWidth(m_mediator->getLeftScrollValue());
	}

	//通知diff窗口进行同步显示
	updateDiffWinCurView();
}

//中介者发过来的同步滚动条当前值信息
void CompareWin::slot_syncCurScrollXValue(int direction)
{
	if (0 == direction)
	{
		ui.rightSrc->SendScintilla(SCI_SETXOFFSET, m_mediator->getLeftScrollXValue());
		//一旦对方同步后，则更新对方当前的值
		m_mediator->setRightScrollXValue(m_mediator->getLeftScrollXValue());
	}
	else
	{
		ui.leftSrc->SendScintilla(SCI_SETXOFFSET, m_mediator->getRightScrollXValue());
		m_mediator->setLeftScrollXValue(m_mediator->getRightScrollXValue());
	}
}


//保存文件。这里面会只读文件，要注意在外面释放一下
void CompareWin::saveFile(RC_DIRECTION direction)
{
	QString filePath;
	QsciDisplayWindow* src = nullptr;
	CODE_ID code = UNKOWN;

	QList<BlockUserData*>* externLinesInfo = nullptr;


	if (direction == RC_LEFT)
	{
		filePath = ui.leftSrc->getFilePath();
		externLinesInfo = m_leftExternBlockInfo;
		src = ui.leftSrc;
		code = m_leftCode;

		if (ui.leftSaveCodeBox->currentIndex() != ui.leftCodeBox->currentIndex())
		{
			CODE_ID saveCode = getCodeFromIndex(ui.leftSaveCodeBox->currentIndex());
			if(saveCode != code)
			{
				code = saveCode;
	}
		}
	}
	else
	{
		filePath = ui.rightSrc->getFilePath();
		externLinesInfo = m_rightExternBlockInfo;
		src = ui.rightSrc;
		code = m_rightCode;

		if (ui.rightSaveCodeBox->currentIndex() != ui.rightCodeBox->currentIndex())
		{
			CODE_ID saveCode = getCodeFromIndex(ui.rightSaveCodeBox->currentIndex());
			if (saveCode != code)
			{
				code = saveCode;
	}
		}
	}

	auto clearFileContents = [](QFile& file, QString& filepath)
	{
		if (file.isOpen())
		{
			if (file.size() > 0)
			{
				file.close();
				file.remove();
				file.setFileName(filepath);
				file.open(QIODevice::ReadWrite);
			}
		}
		else
		{
			file.remove();
			file.setFileName(filepath);
			file.open(QIODevice::ReadWrite);
		}
	};

	if (filePath.isEmpty())
	{
		QMessageBox::warning(this, tr("Error"), tr("Current mode can not save file !"));
		return;
	}

	src->setReadOnly(true);

	QFile file(filePath);

	if (!file.open(QIODevice::WriteOnly /*| QIODevice::ExistingOnly*/ | QIODevice::Truncate))
	{
		QMessageBox::warning(this,tr("Error"),tr("open file %1 failed").arg(filePath));
		return;
	}

	bool isFirstLine = true;

	int firstLineEndType = UNKNOWN_LINE;

	QString text;

	QTextCodec* pTranCode = nullptr;

	if (externLinesInfo != nullptr)
	{
		src->travEveryBlockToSave([&](QString &src, int tailStatus) {

		text.append(src);

		//如果是第一行，则要考虑文本字符编码问题，处理是否是UTF-BOM等格式
		if (isFirstLine && (code != UNKOWN))
		{
			isFirstLine = false;

			QString codecDstName = Encode::getQtCodecNameById(code);
			if (codecDstName.isEmpty() || codecDstName == "unknown")
			{
				//这里是不严谨的，但是目前只能这样处理
					pTranCode = QTextCodec::codecForName("UTF-8");
			}
			else
			{
					pTranCode = QTextCodec::codecForName(codecDstName.toStdString().c_str());
			}


			if(code == CODE_ID::UTF8_BOM)
			{
				//UTF_BOM的不会自动在前面加上头，所以要单独写
				QByteArray codeFlag = Encode::getEncodeStartFlagByte(code);
				if (!codeFlag.isEmpty())
				{
					//先写入标识头
					file.write(codeFlag);
				}
			}

			firstLineEndType = tailStatus;
		}

		/* 如果是未知行尾，则以第一行的行号为准;当编辑后目前行尾可能丢失，则使用第一行的行号; */
		if (tailStatus == UNKNOWN_LINE)
		{
			tailStatus = firstLineEndType;
		}

		if (tailStatus == UNIX_LINE)
		{
			text.append("\n");
		}
		else if (tailStatus == DOS_LINE)
		{
			text.append("\r\n");
		}
		else if (tailStatus == MAC_LINE)
		{
			text.append("\r");
		}
		else if (tailStatus == UNKNOWN_LINE)
		{
			//最后如果实在不知道行尾换行号码，直接使用默认值
			text.append("\r\n");
		}
	}, externLinesInfo);
	}
	else
	{
		//这种就发生在编辑框中直接对比的时候
		pTranCode = QTextCodec::codecForName("UTF-8");
		text = src->text();
	}

	//加个保护
	if (pTranCode == nullptr)
	{
		pTranCode = QTextCodec::codecForName("UTF-8");
	}

	if (text.length() > 0)
	{
		//保存时注意编码问题。在调用toLocal8Bit时，除了UTF-BOM以外，其他都会自动在前面加上头。
		QByteArray t = pTranCode->fromUnicode(text);
		file.write(t);
	}

	file.close();

	ui.statusBar->showMessage(tr("save file finished !"), MSG_SHOW_TIME);
}
//保存
void CompareWin::closeEvent(QCloseEvent * event)
{
	bool isNeedSendCmpResult = false;

	bool isClose = true;

	if (!m_pFindWin.isNull())
	{
		m_pFindWin.data()->close();
	}

	//内存模式下，如果文件路径也是空，则不需要保存
	if (m_workMode == 1)
	{
		if (ui.leftPath->text().isEmpty())
		{
			m_isLeftDirty = false;
		}
		if (ui.rightPath->text().isEmpty())
		{
			m_isRightDirty = false;
		}
	}

	if (m_isLeftDirty && m_isRightDirty)
	{
		closeDlg dlg;
		int ret = dlg.exec();
		switch (ret) {
		case -1:
			event->accept();
			break;
		case -2:
			event->ignore();
			isClose = false;
			break;
		case 0:
			event->accept();
			break;
		case 1:
			saveFile(getUiRealDirection(RC_LEFT));
			event->accept();
			break;
		case 2:
			saveFile(getUiRealDirection(RC_RIGHT));
			event->accept();
			break;
		case 3:
		{
			saveFile(RC_LEFT);
			saveFile(RC_RIGHT);
			isNeedSendCmpResult = true;
			event->accept();
		}
		break;
		default:
			event->accept();
			break;
		}
	}
	else if (m_isLeftDirty)
	{
		QMessageBox msgBox(this);
		msgBox.setIcon(QMessageBox::Question);

		if (m_leftRightOrder == 0)
		{
			msgBox.setText(tr("The left document has been modified."));
		}
		else
		{
			msgBox.setText(tr("The right document has been modified."));
		}
		msgBox.setInformativeText(tr("Do you want to save your changes?"));
		msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Save);
		msgBox.button(QMessageBox::Save)->setText(tr("Save"));
		msgBox.button(QMessageBox::Discard)->setText(tr("Discard"));
		msgBox.button(QMessageBox::Cancel)->setText(tr("Cancel"));
		int ret = msgBox.exec();
		switch (ret) {
		case QMessageBox::Save:
			// Save was clicked
			event->accept();
			saveFile(RC_LEFT);
			isNeedSendCmpResult = true;
			break;
		case QMessageBox::Discard:
			// Don't Save was clicked
			event->accept();
			break;
		case QMessageBox::Cancel:
			// Cancel was clicked
			event->ignore();
			isClose = false;
			break;
		default:
			// should never be reached
			break;
		}
	}
	else if (m_isRightDirty)
	{
		QMessageBox msgBox(this);
		msgBox.setIcon(QMessageBox::Question);

		if (m_leftRightOrder == 0)
		{
			msgBox.setText(tr("The right document has been modified."));
		}
		else
		{
			msgBox.setText(tr("The left document has been modified."));
		}
		msgBox.setInformativeText(tr("Do you want to save your changes?"));
		msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Save);
		msgBox.button(QMessageBox::Save)->setText(tr("Save"));
		msgBox.button(QMessageBox::Discard)->setText(tr("Discard"));
		msgBox.button(QMessageBox::Cancel)->setText(tr("Cancel"));
		int ret = msgBox.exec();
		switch (ret) {
		case QMessageBox::Save:
			// Save was clicked
			event->accept();
			saveFile(RC_RIGHT);
			isNeedSendCmpResult = true;
			break;
		case QMessageBox::Discard:
			// Don't Save was clicked
			event->accept();
			break;
		case QMessageBox::Cancel:
			// Cancel was clicked
			event->ignore();
			isClose = false;
			break;
		default:
			// should never be reached
			break;
		}
	}

	if (isNeedSendCmpResult)
	{
		int status = ((m_leftNoEqualBlocks.count() == 0) ? RC_RESULT_EQUAL : RC_RESULT_NOEQUAL);

		emit sendCmpResultAtClose(status, ui.leftPath->text(), ui.rightPath->text());
	}
	else if ((!m_isLeftDirty) && (!m_isRightDirty) && (m_leftNoEqualBlocks.count() == 0))
	{
		int status = RC_RESULT_EQUAL;

		//没有做过任何修改，没有不等块，直接设置为相等
		//如果有1个空文件，则认定为不等。0个或2个，还是认定为相等
		if (blockCompare->getEmptyFileStatus() == 1)
		{
			status = RC_RESULT_NOEQUAL;
	}

		emit sendCmpResultAtClose(status, ui.leftPath->text(), ui.rightPath->text());
	}

	if (isClose)
	{
		emit signCmpFileClose();
	}
}





void CompareWin::slot_preBt(bool)
{
	if (m_leftNoEqualBlocks.isEmpty())
	{
		//ui.statusBar->showMessage(tr("no more unequal block!"), MSG_SHOW_TIME);
		showTips(tr("no more unequal block!"));
		return;
	}

	int firstDisLineNum = ui.leftSrc->execute(SCI_GETFIRSTVISIBLELINE);

	int nextIndex = m_leftNoEqualBlocks.size();
	bool hasNext = false;

	for (QList<NoEqualBlock>::reverse_iterator it = m_leftNoEqualBlocks.rbegin(); it != m_leftNoEqualBlocks.rend(); ++it)
	{
		--nextIndex;

		if (it->startBlockNums < firstDisLineNum)
		{
			hasNext = true;
			break;
		}
	}

	if (!hasNext)
	{
		showTips(tr("the first one!"));
	}

	m_curShowBlockNums = nextIndex + 1;

	if (m_curShowBlockNums == -1)
	{
		m_curShowBlockNums = 0;
		goNoEqualPos(0);
		return;
	}
	else if (m_curShowBlockNums >= m_leftNoEqualBlocks.count())
	{
		m_curShowBlockNums = m_leftNoEqualBlocks.count() - 1;
	}

	int start = m_curShowBlockNums;

	QList<NoEqualBlock>::iterator it = m_leftNoEqualBlocks.begin();

	while (m_curShowBlockNums >= 0 && m_curShowBlockNums < m_leftNoEqualBlocks.count())
	{
		if (m_curShowBlockNums > 0)
		{
			--m_curShowBlockNums;
		}
		else
		{
			//ui.statusBar->showMessage(tr("the first one!"), MSG_SHOW_TIME);
			showTips(tr("the first one!"));
		}

		const NoEqualBlock& block = *(it + m_curShowBlockNums);

		if (!block.isDeleted)
		{
			int curShowLineNum = block.startBlockNums;
			ui.leftSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, curShowLineNum);
			ui.rightSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, curShowLineNum);

			ui.leftSrc->SendScintilla(SCI_GOTOLINE, curShowLineNum);
			ui.rightSrc->SendScintilla(SCI_GOTOLINE, curShowLineNum);

			ui.leftSrc->updateLineNumberWidth();
			ui.rightSrc->updateLineNumberWidth();
            ui.statusBar->showMessage(tr("the %1 diff, total %2 diff").arg(m_curShowBlockNums+1).arg(m_leftNoEqualBlocks.count()));
			break;
		}
		else if (m_curShowBlockNums == 0)
		{
			//从这里出去的，就是失败了。必须要恢复到当前的位置。否则可能要点击2次才能动
			m_curShowBlockNums = start;
			ui.statusBar->showMessage(tr("already the first one!"), MSG_SHOW_TIME);

			break;
		}
	}

	updateDiffWinCurView();
}



//运行到第几个不等块
void CompareWin::goNoEqualPos(int index)
{
	if (index >= 0 && index < m_leftNoEqualBlocks.size())
	{
		const NoEqualBlock& block = m_leftNoEqualBlocks[index];

		if (!block.isDeleted)
		{
			int curShowLineNum = block.startBlockNums;
			ui.leftSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, curShowLineNum);
			ui.rightSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, curShowLineNum);

			ui.leftSrc->SendScintilla(SCI_GOTOLINE, curShowLineNum);
			ui.rightSrc->SendScintilla(SCI_GOTOLINE, curShowLineNum);
		}
	}
}

void CompareWin::slot_nextBt(bool)
{
	if (m_leftNoEqualBlocks.isEmpty())
	{
		//ui.statusBar->showMessage(tr("no more unequal block!"), MSG_SHOW_TIME);
		showTips(tr("no more unequal block!"));
		return;
	}

	int firstDisLineNum = ui.leftSrc->execute(SCI_GETFIRSTVISIBLELINE);

	int nextIndex = 0;
	bool hasNext = false;

	for (QList<NoEqualBlock>::iterator it = m_leftNoEqualBlocks.begin(); it != m_leftNoEqualBlocks.end(); ++it)
	{
		++nextIndex;

		if (it->startBlockNums > firstDisLineNum)
		{
			hasNext = true;
			break;
		}
	}

	if (!hasNext)
	{
		showTips(tr("the last one!"));
	}

	m_curShowBlockNums = nextIndex - 2;

	if (m_curShowBlockNums == -1)
	{
		m_curShowBlockNums = 0;
		goNoEqualPos(0);
		return;
	}
	else if (m_curShowBlockNums >= m_leftNoEqualBlocks.count())
	{
		m_curShowBlockNums = m_leftNoEqualBlocks.count() - 1;
	}

	int start = m_curShowBlockNums;

	QList<NoEqualBlock>::iterator it = m_leftNoEqualBlocks.begin();

	while (m_curShowBlockNums >= 0 && m_curShowBlockNums < m_leftNoEqualBlocks.count())
	{
		if (m_curShowBlockNums < m_leftNoEqualBlocks.count() - 1)
		{
			++m_curShowBlockNums;
		}
		else
		{
			//ui.statusBar->showMessage(tr("the last one!"), MSG_SHOW_TIME);
			showTips(tr("the last one!"));
		}

		const NoEqualBlock& block = *(it + m_curShowBlockNums);

		if (!block.isDeleted)
		{
			int curShowLineNum = block.startBlockNums;
			ui.leftSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, curShowLineNum);
			ui.rightSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, curShowLineNum);

			ui.leftSrc->SendScintilla(SCI_GOTOLINE, curShowLineNum);
			ui.rightSrc->SendScintilla(SCI_GOTOLINE, curShowLineNum);

			ui.leftSrc->updateLineNumberWidth();
			ui.rightSrc->updateLineNumberWidth();
            ui.statusBar->showMessage(tr("the %1 diff, total %2 diff").arg(m_curShowBlockNums+1).arg(m_leftNoEqualBlocks.count()));
			break;
		}
		else if (m_curShowBlockNums == (m_leftNoEqualBlocks.count() - 1))
		{
			m_curShowBlockNums = start;
			ui.statusBar->showMessage(tr("already the last one!"), MSG_SHOW_TIME);
			//QToolTip::showText(ui.statusBar->pos(), tr("the last one!"), this/* const QRect &rect*/);
			break;
		}
	}
	updateDiffWinCurView();
}

//交换左右
void CompareWin::slot_swapBt(bool)
{
	if (m_leftRightOrder == 0)
	{
		m_leftRightOrder = 1;

		ui.verticalLayout_left->addWidget(ui.rightChildWidget);
		ui.verticalLayout_right->addWidget(ui.leftChildWidget);

		ui.leftSrc->markerDefine(*m_rightPix, MARGIN_SYNC_BT);
		ui.rightSrc->markerDefine(*m_leftPix, MARGIN_SYNC_BT);

		ui.leftLabel->setText(tr("right text code"));
		ui.rightLabel->setText(tr("left text code"));
	}
	else if (m_leftRightOrder == 1)
	{
		m_leftRightOrder = 0;

		ui.verticalLayout_left->addWidget(ui.leftChildWidget);
		ui.verticalLayout_right->addWidget(ui.rightChildWidget);

		ui.leftSrc->markerDefine(*m_leftPix, MARGIN_SYNC_BT);
		ui.rightSrc->markerDefine(*m_rightPix, MARGIN_SYNC_BT);

		ui.leftLabel->setText(tr("left text code"));
		ui.rightLabel->setText(tr("right text code"));
	}


}

//只在内存模式时使用
void CompareWin::removePadLines()
{
	if (m_leftExternBlockInfo == nullptr || m_rightExternBlockInfo == nullptr)
	{
		return;
	}

	enableSignTextChange(false);
	disableTextChangeSignal();

	auto clearPadline = [this](RC_DIRECTION dir, QsciDisplayWindow *pSrc, QList<BlockUserData*>* externLineInfo) {
		int lineNums = pSrc->lines();
		lineNums = (lineNums > externLineInfo->size()) ? externLineInfo->size() : lineNums;

		for (int i = lineNums-1; i >=0; --i)
		{
			int markMask = pSrc->SendScintilla(SCI_MARKERGET, i);
			if (markMask != 0)
			{
				pSrc->markerDelete(i);
			}

			BlockUserData* blockUserData = externLineInfo->at(i);
			if (blockUserData != nullptr && (blockUserData->m_blockType == PAD_BLOCK || blockUserData->m_blockType == LAST_PAD_EMPTY_BLOCK))
			{
				int startPos = pSrc->SendScintilla(SCI_POSITIONFROMLINE, i);
				int	endPos = pSrc->SendScintilla(SCI_POSITIONFROMLINE, i+1);
				if (endPos == -1)
				{
					endPos = pSrc->length();
				}
				if (endPos > startPos)
				{
					replaceText(dir, startPos, endPos, 0, "");
				}
			}
		}
	};
	clearPadline(RC_LEFT, ui.leftSrc, m_leftExternBlockInfo);
	clearPadline(RC_RIGHT,ui.rightSrc, m_rightExternBlockInfo);

	enableSignTextChange(true);
}


void CompareWin::slot_reloadBt()
{
	if (m_workMode == 1)
	{
		removePadLines();
		clearInterStatus();

		slot_doMemoryCmp();
		return;
	}

	if (m_isLeftDirty && m_isRightDirty)
	{
		closeDlg dlg;
		int ret = dlg.exec();
		switch (ret) {
		case -1:
		case -2:
		case 0:
			break;
		case 1:
			saveFile(getUiRealDirection(RC_LEFT));
			break;
		case 2:
			saveFile(getUiRealDirection(RC_RIGHT));
			break;
		case 3:
		{
			saveFile(RC_LEFT);
			saveFile(RC_RIGHT);
		}
		break;
		default:
			break;
		}
	}
	else if (m_isLeftDirty)
	{
		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Question);

		if (m_leftRightOrder == 0)
		{
			msgBox.setText(tr("The left document has been modified."));
		}
		else
		{
			msgBox.setText(tr("The right document has been modified."));
		}
		msgBox.setInformativeText(tr("Do you want to save your changes?"));
		msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Save);
		int ret = msgBox.exec();
		switch (ret) {
		case QMessageBox::Save:
			saveFile(RC_LEFT);
			break;
		case QMessageBox::Discard:
			break;
		case QMessageBox::Cancel:
			return;
		default:
			break;
		}
	}
	else if (m_isRightDirty)
	{
		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Question);

		if (m_leftRightOrder == 0)
		{
			msgBox.setText(tr("The right document has been modified."));
		}
		else
		{
			msgBox.setText(tr("The left document has been modified."));
		}
		msgBox.setInformativeText(tr("Do you want to save your changes?"));
		msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		msgBox.setDefaultButton(QMessageBox::Save);
		int ret = msgBox.exec();
		switch (ret) {
		case QMessageBox::Save:
			saveFile(RC_RIGHT);
			break;
		case QMessageBox::Discard:
			break;
		case QMessageBox::Cancel:
			return;
		default:
			break;
		}
	}

	m_leftCode = CODE_ID::UNKOWN;
	m_rightCode = CODE_ID::UNKOWN;

	slot_doCmp();
}

void CompareWin::slot_diffViewBt()
{
	if (!m_dockDiffStatusWin.isNull())
	{
		m_dockDiffStatusWin->close();
	}
	else
	{
		initDiffStatusDockWin();
		updateDiffStatusWin(ui.leftSrc->lines(), m_leftNoEqualBlocks);
	}
}

//打开文件的槽函数
void CompareWin::slot_openLeftFile()
{
	m_leftCode = UNKOWN;

	openFile(RC_LEFT);
}

void CompareWin::slot_openRightFile()
{
	m_rightCode = UNKOWN;

	openFile(RC_RIGHT);
}

void CompareWin::slot_saveLeftFile()
{
	//如果是内存模式，而且当前存在内存，也需要保存。
	if (m_isLeftDirty || m_leftSaveCodeChange || ((m_workMode == 1) && !ui.leftSrc->text().isEmpty()))
	{
		if (ui.leftSrc->getFilePath().isEmpty())
		{
			//弹出要保存的路径选择框
			QString filter("Text files (*.txt);;XML files (*.xml);;h files (*.h);;cpp file(*.cpp);;All types(*.*)");
			QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), QString(), filter);
			if (!fileName.isEmpty())
			{
				ui.leftPath->setText(fileName);
				ui.leftSrc->setFilePath(fileName);
			}
			else
			{
				//取消保存
				return;
			}
		}


		saveFile(RC_LEFT);
		m_isLeftDirty = false;
		ui.leftSrc->setReadOnly(false);
		syncNeedSaveIcon();

		m_leftSaveCodeChange = true;
	}

}

void CompareWin::slot_saveRightFile()
{
	if (m_isRightDirty || m_rightSaveCodeChange || ((m_workMode == 1) && !ui.rightSrc->text().isEmpty()))
	{
		if (ui.rightSrc->getFilePath().isEmpty())
		{
			//弹出要保存的路径选择框
			QString filter("Text files (*.txt);;XML files (*.xml);;h files (*.h);;cpp file(*.cpp);;All types(*.*)");
			QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), QString(), filter);
			if (!fileName.isEmpty())
			{
				ui.rightPath->setText(fileName);
				ui.rightSrc->setFilePath(fileName);
			}
			else
			{
				//取消保存
				return;
			}
		}

		saveFile(RC_RIGHT);
		m_isRightDirty = false;
		syncNeedSaveIcon();
		ui.rightSrc->setReadOnly(false);

		m_rightSaveCodeChange = true;
	}
}

void CompareWin::openFile(int type)
{
	QFileDialog fd(this);
	fd.setFileMode(QFileDialog::ExistingFile);

	QString workDir = ((RC_LEFT == type) ? ui.leftPath->text() : ui.rightPath->text());

	if (!workDir.isEmpty())
	{
		QFileInfo t(workDir);

		fd.setDirectory(t.dir());
	}

	if (fd.exec() == QDialog::Accepted)   //如果成功的执行
	{
		QStringList fileNameList = fd.selectedFiles();      //返回文件列表的名称
		QFileInfo fi(fileNameList[0]);

		disableTextChangeSignal();

		if (RC_LEFT == type)
		{
			ui.leftPath->setText(fi.filePath());

			ui.leftSrc->setFilePath(fi.filePath());
		}
		else if (RC_RIGHT == type)
		{
			ui.rightPath->setText(fi.filePath());

			ui.rightSrc->setFilePath(fi.filePath());
		}
	}
	else
	{
		fd.close();
		//用户点击了取消选中
		return;
	}

	slot_doCmp();

}



void CompareWin::enableEdit(bool enable)
{
	//编辑时禁止输入
	ui.leftSrc->setEnabled(enable);
	ui.rightSrc->setEnabled(enable);
}

//设置是否在编辑状态，如果在则输入没有完成；如果不在编辑状态，则表示输入完成
//一旦进入编辑专题，此时理解为正在输入，显示刷新等事件先忽略，避免一遍输入，一遍触发修改，而导致输入错乱
void CompareWin::setAtEditStatus(bool isInEdit)
{
	//编辑时禁止输入
	ui.leftSrc->setEnabled(!isInEdit);
	ui.rightSrc->setEnabled(!isInEdit);
}

//#define CHECK_GBK

//同步左右的垂直滚动条位置。发现修改或同步后，左右的垂直滚动条没有同步
void CompareWin::asyncVerticalScrollValue(RC_DIRECTION dir)
{
	if (ui.leftSrc->getCurVerticalScrollValue() != ui.rightSrc->getCurVerticalScrollValue())
	{
		if (dir == RC_LEFT)
		{
			//左边触发，需要修改右边
			m_mediator->setLeftScrollValue(ui.leftSrc->getCurVerticalScrollValue());
		}
		else
		{
			//右边触发，需要修改左边
			m_mediator->setRightScrollValue(ui.rightSrc->getCurVerticalScrollValue());
		}
	}
}

void CompareWin::setLineMarkers(QsciDisplayWindow *uiSrc, int line, int markMask)
{
	if ((markMask & MARGIN_SYNC_BT_BIT_MASK) != 0)
	{
		uiSrc->markerAdd(line, MARGIN_SYNC_BT);
	}
	if ((markMask & MARGIN_VER_LINE_BIT_MASK) != 0)
	{
		uiSrc->markerAdd(line, MARGIN_VER_LINE);
	}
	if ((markMask & STYLE_PAD_BACK_PIXMAP_BIT_MASK) != 0)
	{
		uiSrc->markerAdd(line, STYLE_PAD_BACK_PIXMAP);
	}
	if ((markMask & STYLE_BACK_COLOUR_NO_EQUAL_BIT_MASK) != 0)
	{
		uiSrc->markerAdd(line, STYLE_BACK_COLOUR_NO_EQUAL);
	}
	if ((markMask & STYLE_LAST_PAD_LINE_BIT_MASK) != 0)
	{
		uiSrc->markerAdd(line, STYLE_LAST_PAD_LINE);
	}
	if ((markMask & STYLE_BACK_COLOUR_EQUAL_BIT_MASK) != 0)
	{
		uiSrc->markerAdd(line, STYLE_BACK_COLOUR_EQUAL);
}
}

/* 设置最后一行的格式。最后一行由于可能没有换行符，会导致没有换行符的一边会看起来缺少一行
* 对于这种没有换行符的结尾行，插入一个特殊背景的对齐行
*/
void CompareWin::setLastBlockFormat(QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInf)
{
	int leftLineNums = ui.leftSrc->SendScintilla(SCI_GETLINECOUNT);
	int rightLineNums = ui.rightSrc->SendScintilla(SCI_GETLINECOUNT);

	assert(leftExternBlockInfo->size() == rightExternBlockInf->size());

	//20211014发现一个问题，只能运行实际行超过leftExternBlockInfo.size()一行。多了则有问题
	//多了的时候，假设leftExternBlockInfo只有140，而用户可以在142行操作，则中间有个141的行是不存在的，在后续中
	//会导致乱序。问题不好描述，但是最多只允许实际行超过leftExternBlockInfo的1行

	//多余的行是垃圾行，直接裁掉
	if (leftLineNums > leftExternBlockInfo->size() + 1)
	{
		for (int start = leftLineNums-1; start >= leftExternBlockInfo->size(); --start)
		{
			//先删除marker和纹理后，再删除行，顺序不能乱

			//删除行目前所有的marken，发现删除一行后，下行会上前到前面一行
			//但是他们的market居然会叠加，所有和增加不一样，删除必须采用删除所有marker的方法
			int markMask = ui.leftSrc->SendScintilla(SCI_MARKERGET, start);
			if (markMask != 0)
			{
				ui.leftSrc->markerDelete(start);
			}
		}

		int StartPos = ui.leftSrc->SendScintilla(SCI_POSITIONFROMLINE, leftExternBlockInfo->size());
		int	endPos = ui.leftSrc->length();
		replaceText(RC_LEFT, StartPos, endPos, 0, "");
	}

	if (rightLineNums > rightExternBlockInf->size() + 1)
	{
		for (int start = rightLineNums-1; start >= rightExternBlockInf->size(); --start)
		{
			//先删除marker和纹理后，再删除行，顺序不能乱

			//删除行目前所有的marken，发现删除一行后，下行会上前到前面一行
			//但是他们的market居然会叠加，所有和增加不一样，删除必须采用删除所有marker的方法
			int markMask = ui.rightSrc->SendScintilla(SCI_MARKERGET, start);
			if (markMask != 0)
			{
				ui.rightSrc->markerDelete(start);
			}
		}

		int StartPos = ui.rightSrc->SendScintilla(SCI_POSITIONFROMLINE, rightExternBlockInf->size());
		int	endPos = ui.rightSrc->length();
		replaceText(RC_RIGHT, StartPos, endPos, 0, "");
	}

	leftLineNums = ui.leftSrc->SendScintilla(SCI_GETLINECOUNT);
	rightLineNums = ui.rightSrc->SendScintilla(SCI_GETLINECOUNT);

	if (leftLineNums == rightLineNums)
	{
		return;
	}
	else if (leftLineNums == (1 + rightLineNums))//实际上只有一行之多
	{
		//20211014最后一行如果添加一个空白行，其实导致当前行位置插入一行，而原来的当前行被调整到下一行。
		//会导致marker也跑到下一行去了。这个错误很早就发现过，修改又被否定。最后还是发现有改问题。
		//给自己留一个教训，不轻易删除该注释

		int markMask = ui.rightSrc->SendScintilla(SCI_MARKERGET, rightLineNums-1);

#ifdef _WIN32
		ui.rightSrc->SendScintilla(SCI_APPENDTEXT, 2, "\r\n");
#else
		ui.rightSrc->SendScintilla(SCI_APPENDTEXT, 1, "\n");
#endif

		//因为加入换行导致marker下移，把marker取出，恢复到上一行去
		setLineMarkers(ui.rightSrc, rightLineNums - 1, markMask);
		//去掉多余的marker
		ui.rightSrc->markerDelete(leftLineNums - 1);
		//sci中编号从0开始,所以nums需要减去1
		ui.rightSrc->markerAdd(leftLineNums - 1, STYLE_LAST_PAD_LINE);

	}
	else if (rightLineNums == (1 + leftLineNums))
	{

		int markMask = ui.leftSrc->SendScintilla(SCI_MARKERGET, leftLineNums - 1);
#ifdef _WIN32
		ui.leftSrc->SendScintilla(SCI_APPENDTEXT, 2, "\r\n");
#else
		ui.leftSrc->SendScintilla(SCI_APPENDTEXT, 1, "\n");
#endif
		//因为加入换行导致marker下移，把marker取出，恢复到上一行去
		setLineMarkers(ui.leftSrc, leftLineNums - 1, markMask);
		//去掉多余的marker
		ui.leftSrc->markerDelete(rightLineNums - 1);
		ui.leftSrc->markerAdd(rightLineNums - 1, STYLE_LAST_PAD_LINE);

	}
	else
	{
		QMessageBox::warning(this, tr("Error"), tr("The current comparison has encountered an error.Quit temporarily."));
		close();
		assert(false);
	}
}

/*outputEditBlock从outputBlock函数变化而来。输出包含在BlockNode里面的块信息，只部分输出编辑块后的内容。
*编辑块总是认定为不相等块。
2020-4-11发现一个问题，如果是\r\n分开两个，会换行2次，所以需要将\r\n一次性输出
*编辑块输入时，要认定为是插入，而非追加。插入是，此时编辑光标位于待插入点，这个点往往是一个行的尾巴，或者一个行的开头。
*返回值：增加的块数
*/
//int CompareWin::outputEditBlock(int direction, QVector<BlockNode> &blocks, QMap<int, UserBlockData>& blockStatus)
//{
//	return 0;
//}

/* 创建不等块的信息 */
void CompareWin::createNonEqualBlock(QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInfo)
{
	m_leftNoEqualBlocks.clear();

	/* 最后一个块，可能是空的，只是光标再那里 */


	for (QList<BlockUserData*>::iterator leftIt = leftExternBlockInfo->begin(); leftIt != leftExternBlockInfo->end(); ++leftIt)
	{
		BlockUserData* userData = *leftIt;

		//qDebug(" left empty block num %d status %d", userData->m_srcBlockNum, userData->m_blockType);

		if (userData->m_blockType == UNEQUAL_BLOCK || userData->m_blockType == PAD_BLOCK)
		{
			NoEqualBlock info(userData->m_srcBlockNum, 1);
			info.type = userData->m_blockType;
			m_leftNoEqualBlocks.append(info);

			if (userData->m_blockType == PAD_BLOCK)
			{
				ui.leftSrc->markerAdd(userData->m_srcBlockNum, STYLE_PAD_BACK_PIXMAP);


				//ui.leftSrc->SendScintilla(SCI_SETFOLDFLAGS,userData->m_srcBlockNum);
				//ui.leftSrc->SendScintilla(SCI_MARGINSETTEXT, userData->m_srcBlockNum, "p");

			}
			else
			{
				ui.leftSrc->markerDelete(userData->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
				ui.leftSrc->markerAdd(userData->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
			}
		}
	}


	m_rightNoEqualBlocks.clear();

	for (QList<BlockUserData*>::iterator rightIt = rightExternBlockInfo->begin(); rightIt != rightExternBlockInfo->end(); ++rightIt)
	{
		BlockUserData* userData = *rightIt;

		//qDebug(" left empty block num %d status %d", userData->m_srcBlockNum, userData->m_blockType);

		if (userData->m_blockType == UNEQUAL_BLOCK || userData->m_blockType == PAD_BLOCK)
		{
			NoEqualBlock info(userData->m_srcBlockNum, 1);
			info.type = userData->m_blockType;

			m_rightNoEqualBlocks.append(info);

			if (userData->m_blockType == PAD_BLOCK)
			{
				ui.rightSrc->markerAdd(userData->m_srcBlockNum, STYLE_PAD_BACK_PIXMAP);
			}
			else
			{
				ui.rightSrc->markerDelete(userData->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
				ui.rightSrc->markerAdd(userData->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
			}

		}
	}

}

void CompareWin::createNonEqualBlock(QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInfo, QList<NoEqualBlock>& leftNoEqualBlocks, QList<NoEqualBlock> &rightNoEqualBlocks)
{
	leftNoEqualBlocks.clear();

	for (QList<BlockUserData*>::iterator leftIt = leftExternBlockInfo->begin(); leftIt != leftExternBlockInfo->end(); ++leftIt)
	{
		BlockUserData* userData = *leftIt;

		//qDebug(" left empty block num %d status %d", userData->m_srcBlockNum, userData->m_blockType);

		if (userData->m_blockType == UNEQUAL_BLOCK || userData->m_blockType == PAD_BLOCK)
		{
			NoEqualBlock info(userData->m_srcBlockNum, 1);
			info.type = userData->m_blockType;
			leftNoEqualBlocks.append(info);
		}
	}

	rightNoEqualBlocks.clear();

	for (QList<BlockUserData*>::iterator rightIt = rightExternBlockInfo->begin(); rightIt != rightExternBlockInfo->end(); ++rightIt)
	{
		BlockUserData* userData = *rightIt;

		//qDebug(" left empty block num %d status %d", userData->m_srcBlockNum, userData->m_blockType);

		if (userData->m_blockType == UNEQUAL_BLOCK || userData->m_blockType == PAD_BLOCK)
		{
			NoEqualBlock info(userData->m_srcBlockNum, 1);
			info.type = userData->m_blockType;

			rightNoEqualBlocks.append(info);

		}
	}
}



void CompareWin::clearMarker(RC_DIRECTION dir)
{
	QsciDisplayWindow* uiSrc = ((dir == RC_LEFT) ? ui.leftSrc : ui.rightSrc);

	uiSrc->SendScintilla(SCI_MARKERDELETEALL, STYLE_PAD_BACK_PIXMAP);
	uiSrc->SendScintilla(SCI_MARKERDELETEALL, STYLE_BACK_COLOUR_NO_EQUAL);
	uiSrc->SendScintilla(SCI_MARKERDELETEALL, STYLE_BACK_COLOUR_EQUAL);
}

void  CompareWin::setMarker(RC_DIRECTION dir, QList<BlockUserData*>* externBlockInfo)
{
	QsciDisplayWindow* uiSrc = ((dir == RC_LEFT) ? ui.leftSrc : ui.rightSrc);

	for (QList<BlockUserData*>::iterator it = externBlockInfo->begin(); it != externBlockInfo->end(); ++it)
	{
		BlockUserData* userData = *it;

		if (userData->m_blockType == PAD_BLOCK)
		{
			uiSrc->markerAdd(userData->m_srcBlockNum, STYLE_PAD_BACK_PIXMAP);

			//uiSrc->SendScintilla(SCI_MARGINSETSTYLE, userData->m_srcBlockNum, 6);
			//uiSrc->SendScintilla(SCI_MARGINSETTEXT, userData->m_srcBlockNum, "pad");
			//uiSrc->setMarginText(userData->m_srcBlockNum,"pad",6);	
		}
		else if (userData->m_blockType == UNEQUAL_BLOCK)
		{
			uiSrc->markerDelete(userData->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
			uiSrc->markerAdd(userData->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
		}
		else if (userData->m_blockType == EQUAL_BLOCK)
		{
			uiSrc->markerDelete(userData->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
			uiSrc->markerAdd(userData->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
	}
}
}


//20210713根据megreUnEqualBlock修改而来
void CompareWin::megreUnEqualBlock(QList<NoEqualBlock>& leftNoEqualBlocks, QList<NoEqualBlock>& rightNoEqualBlocks)
{
	//2021/6/19 发现：如果左右分开合并，得到两个左右的容器长度并不是一样的，行号自然也不会对齐。
	//需要把结合起来，当合并的时候，不能只看左边的上下是否类型一样，还需要检查右边的左右也是否类型一样。
	//简化：干脆就合并左边，按照左边为准，把左边相同类型的全部归拢起来，右边不归拢，按照左边的来，要注意，右边是不归拢的。

	assert(leftNoEqualBlocks.size() == rightNoEqualBlocks.size());

	if (!leftNoEqualBlocks.isEmpty())
	{
		QList<NoEqualBlock>::iterator leftIt = leftNoEqualBlocks.begin();
		QList<NoEqualBlock>::iterator rightIt = rightNoEqualBlocks.begin();

		//前一个连续块的序号
		int leftPreBlockNum = -1;
		int rightPreBlockNum = -1;

		/* 将前一个块的类型（对齐和不等行）记住，当发生对齐和不等行切换时，单独分割出一个不等块
		* 目的是为了防止一个不等块太大，既包含对齐块又包含不等块;将这两种夹杂的块分割开；其实可以
		* 做一个配置选项，来区分这两种情况
		*/
		int leftPreBlockType = leftIt->type;
		int rightPreBlockType = rightIt->type;


		QList<NoEqualBlock>::iterator leftPreBlockIndex = leftIt;
		QList<NoEqualBlock>::iterator rightPreBlockIndex = rightIt;

		leftPreBlockNum = leftIt->startBlockNums + leftIt->blockLens;
		rightPreBlockNum = rightIt->startBlockNums + rightIt->blockLens;


		for (leftIt; leftIt != leftNoEqualBlocks.end(); ++leftIt, ++rightIt)
		{
			//1)左边上下不同的行是连续的，而且类型相同，才考虑合并
			//2）满足条件1）时，还要考虑右边也是连续而且相同类型。只有左右都是可以归拢的，才真正归拢
			if ((leftIt->startBlockNums == leftPreBlockNum && leftIt->type == leftPreBlockType) && (rightIt->startBlockNums == rightPreBlockNum && rightIt->type == rightPreBlockType))
			{
				//则前一个长度加1
				leftPreBlockIndex->blockLens += leftIt->blockLens;

				//而当前这个删除，先标记为删除，迭代器循环不能直接删除
				leftIt->blockLens = 0;
				leftPreBlockNum = leftPreBlockIndex->startBlockNums + leftPreBlockIndex->blockLens;

				rightPreBlockIndex->blockLens += rightIt->blockLens;

				//而当前这个删除，先标记为删除，迭代器循环不能直接删除
				rightIt->blockLens = 0;
				rightPreBlockNum = rightPreBlockIndex->startBlockNums + rightPreBlockIndex->blockLens;
			}
			else
			{
				leftPreBlockNum = leftIt->startBlockNums + leftIt->blockLens;
				leftPreBlockType = leftIt->type;
				leftPreBlockIndex = leftIt;

				rightPreBlockNum = rightIt->startBlockNums + rightIt->blockLens;
				rightPreBlockType = rightIt->type;
				rightPreBlockIndex = rightIt;
			}


		}

		//删除长度为0的元素
		QMutableListIterator<NoEqualBlock> blockMap(leftNoEqualBlocks);

		while (blockMap.hasNext()) {
			NoEqualBlock val = blockMap.next();
			if (val.blockLens == 0)
			{
				blockMap.remove();
			}
		}

		//删除长度为0的元素
		QMutableListIterator<NoEqualBlock> rightBlockMap(rightNoEqualBlocks);

		while (rightBlockMap.hasNext()) {
			NoEqualBlock val = rightBlockMap.next();
			if (val.blockLens == 0)
			{
				rightBlockMap.remove();
			}
		}

		assert(leftNoEqualBlocks.size() == rightNoEqualBlocks.size());

	}
}

/*多个不同的块，其实有一些是连续的，那么归并为一个，只保留最上面的一个
*对齐块不合并。只有联系的类型相同的块，才会合并。发现一个问题，如果左右各自合并各自的，发现两个容器的长度并不一样
*/
void CompareWin::megreUnEqualBlock()
{
	//2021/6/19 发现：如果左右分开合并，得到两个左右的容器长度并不是一样的，行号自然也不会对齐。
	//需要把结合起来，当合并的时候，不能只看左边的上下是否类型一样，还需要检查右边的左右也是否类型一样。
	//简化：干脆就合并左边，按照左边为准，把左边相同类型的全部归拢起来，右边不归拢，按照左边的来，要注意，右边是不归拢的。

	assert(m_leftNoEqualBlocks.size() == m_rightNoEqualBlocks.size());

	if (!m_leftNoEqualBlocks.isEmpty())
	{
		QList<NoEqualBlock>::iterator leftIt = m_leftNoEqualBlocks.begin();
		QList<NoEqualBlock>::iterator rightIt = m_rightNoEqualBlocks.begin();

		//前一个连续块的序号
		int leftPreBlockNum = -1;
		int rightPreBlockNum = -1;

		/* 将前一个块的类型（对齐和不等行）记住，当发生对齐和不等行切换时，单独分割出一个不等块
		* 目的是为了防止一个不等块太大，既包含对齐块又包含不等块;将这两种夹杂的块分割开；其实可以
		* 做一个配置选项，来区分这两种情况
		*/
		int leftPreBlockType = leftIt->type;
		int rightPreBlockType = rightIt->type;


		QList<NoEqualBlock>::iterator leftPreBlockIndex = leftIt;
		QList<NoEqualBlock>::iterator rightPreBlockIndex = rightIt;

		leftPreBlockNum = leftIt->startBlockNums + leftIt->blockLens;
		rightPreBlockNum = rightIt->startBlockNums + rightIt->blockLens;


		for (leftIt; leftIt != m_leftNoEqualBlocks.end(); ++leftIt, ++rightIt)
		{
			//1)左边上下不同的行是连续的，而且类型相同，才考虑合并
			//2）满足条件1）时，还要考虑右边也是连续而且相同类型。只有左右都是可以归拢的，才真正归拢
			if ((leftIt->startBlockNums == leftPreBlockNum && leftIt->type == leftPreBlockType) && (rightIt->startBlockNums == rightPreBlockNum && rightIt->type == rightPreBlockType))
			{
				//则前一个长度加1
				leftPreBlockIndex->blockLens += leftIt->blockLens;

				//而当前这个删除，先标记为删除，迭代器循环不能直接删除
				leftIt->blockLens = 0;
				leftPreBlockNum = leftPreBlockIndex->startBlockNums + leftPreBlockIndex->blockLens;

				rightPreBlockIndex->blockLens += rightIt->blockLens;

				//而当前这个删除，先标记为删除，迭代器循环不能直接删除
				rightIt->blockLens = 0;
				rightPreBlockNum = rightPreBlockIndex->startBlockNums + rightPreBlockIndex->blockLens;
			}
			else
			{
				leftPreBlockNum = leftIt->startBlockNums + leftIt->blockLens;
				leftPreBlockType = leftIt->type;
				leftPreBlockIndex = leftIt;

				rightPreBlockNum = rightIt->startBlockNums + rightIt->blockLens;
				rightPreBlockType = rightIt->type;
				rightPreBlockIndex = rightIt;
			}


		}

		//删除长度为0的元素
		QMutableListIterator<NoEqualBlock> blockMap(m_leftNoEqualBlocks);

		while (blockMap.hasNext()) {
			NoEqualBlock val = blockMap.next();
			if (val.blockLens == 0)
			{
				blockMap.remove();
			}
		}

		//删除长度为0的元素
		QMutableListIterator<NoEqualBlock> rightBlockMap(m_rightNoEqualBlocks);

		while (rightBlockMap.hasNext()) {
			NoEqualBlock val = rightBlockMap.next();
			if (val.blockLens == 0)
			{
				rightBlockMap.remove();
			}
		}

		assert(m_leftNoEqualBlocks.size() == m_rightNoEqualBlocks.size());

		ui.statusBar->showMessage(tr("has %1 differents").arg(m_leftNoEqualBlocks.count()));
	}
}

//创建同步按钮信息，点击按钮后，即可开始同步
//注意：必须在megreUnEqualBlock函数之后
//后面需要自己做一个scintilla的扩展，可以一次性执行多个设置markerAdd的操作。

void CompareWin::createMargins(QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInfo)
{
	if (!m_leftNoEqualBlocks.isEmpty())
	{
		int i = 0;
		for (QList<NoEqualBlock>::iterator it = m_leftNoEqualBlocks.begin(); it != m_leftNoEqualBlocks.end(); ++it)
		{
			BlockUserData* leftUserData = leftExternBlockInfo->value(it->startBlockNums);
			BlockUserData* rightUserData = rightExternBlockInfo->value(it->startBlockNums);

			if (leftUserData == nullptr || rightUserData == nullptr)
			{
				continue;
			}

			leftUserData->setParam(MARGIN_SYNC_BT, it->blockLens);
			rightUserData->setParam(MARGIN_SYNC_BT, it->blockLens);

			ui.leftSrc->markerAdd(it->startBlockNums, MARGIN_SYNC_BT);
			ui.rightSrc->markerAdd(it->startBlockNums, MARGIN_SYNC_BT);


			//从第二个开始，不会包含同步箭头图标，只包含递减的块长度。在图标拖动显示的时候，需要画一条包含的括弧线
			for (int j = 1; j < it->blockLens; ++j)
			{
				if (it->startBlockNums + j < leftExternBlockInfo->size())
				{
					BlockUserData* leftUserData = leftExternBlockInfo->value(it->startBlockNums + j);
					leftUserData->setParam(MARGIN_VER_LINE, it->blockLens - j);

					BlockUserData* rightUserData = rightExternBlockInfo->value(it->startBlockNums + j);
					rightUserData->setParam(MARGIN_VER_LINE, it->blockLens - j);

					ui.leftSrc->markerAdd(it->startBlockNums + j, MARGIN_VER_LINE);
					ui.rightSrc->markerAdd(it->startBlockNums + j, MARGIN_VER_LINE);
				}
				else
				{
					assert(false);
					QMessageBox::warning(this, tr("Error"), tr("The current comparison has encountered an error.Quit temporarily."));
					close();
				}

			}

			++i;
		}
	}
}


void CompareWin::cleanMargins(RC_DIRECTION dir)
{
	QsciDisplayWindow* uiSrc = ((dir == RC_LEFT) ? ui.leftSrc : ui.rightSrc);

	uiSrc->SendScintilla(SCI_MARKERDELETEALL, MARGIN_SYNC_BT);
	uiSrc->SendScintilla(SCI_MARKERDELETEALL, MARGIN_VER_LINE);

}


//文件对比完成的槽函数，该槽函数是在主线程中执行的
//20240501这个函数千万不能调用processEvents，否则可能导致死锁。
//因为关闭析构函数中会检查m_isCmpFinished是否为true，如果这里processEvents，则再也没有机会进来了。
//析构函数中会死循环下去。
void CompareWin::slot_fileCompareFinish()
{
	QFutureWatcher<ThreadFileCmpParameter*> * s = dynamic_cast<QFutureWatcher<ThreadFileCmpParameter*> *>(sender());

	ThreadFileCmpParameter* result = s->result();

	//如果是空文件，直接显示文件拉倒。发现文件其实有1边或2边是空文件
	if (blockCompare->getEmptyFileStatus() > 0)
	{
		loadFileToDisplay();
	}
	//这里释放的内容，其实是在mode里面new出来的
	else if (result != nullptr )
	{
		//获取文件的文字编码
		int leftSkip = 0;
		int rightSkip = 0;

		m_pCmpMode = result->resultCmpObj;

		m_pCmpMode->getCodeSkip(m_leftCode, m_rightCode, leftSkip, rightSkip);

		int codeIndex = getIndexFromCode(m_leftCode);
		if (ui.leftCodeBox->currentIndex() != int(codeIndex))
		{
			if (codeIndex >= 0)
			{
				ui.leftCodeBox->setCurrentIndex(codeIndex);

				ui.leftSaveCodeBox->setCurrentIndex(codeIndex);
		}
			else
			{
				m_leftCode = CODE_ID::UNKOWN;
				ui.leftCodeBox->setCurrentIndex(getIndexFromCode(CODE_ID::UNKOWN));
				ui.leftSaveCodeBox->setCurrentIndex(getIndexFromCode(CODE_ID::UNKOWN));
			}
		}

		codeIndex = getIndexFromCode(m_rightCode);
		if (ui.rightCodeBox->currentIndex() != int(codeIndex))
		{
			if (codeIndex >= 0)
			{
				ui.rightCodeBox->setCurrentIndex(codeIndex);

				ui.rightSaveCodeBox->setCurrentIndex(codeIndex);
		}
			else
			{
				m_rightCode = CODE_ID::UNKOWN;
				ui.rightCodeBox->setCurrentIndex(getIndexFromCode(CODE_ID::UNKOWN));
				ui.rightSaveCodeBox->setCurrentIndex(getIndexFromCode(CODE_ID::UNKOWN));
			}
		}


		QStringList* leftContent = nullptr;
		QStringList* rightContent = nullptr;

		m_pCmpMode->getResult(leftContent, rightContent);

		QList<BlockUserData*>* leftExternBlockInfo = nullptr;
		QList<BlockUserData*>* rightExternBlockInfo = nullptr;

		m_pCmpMode->getLinesExternInfo(leftExternBlockInfo, rightExternBlockInfo);

		//这里后面还得改一改，有点别扭
		m_leftExternBlockInfo = leftExternBlockInfo;
		m_rightExternBlockInfo = rightExternBlockInfo;

		ui.leftSrc->setFocus();

		if (nullptr != leftContent)
		{
			ui.leftSrc->setText(leftContent->join(""));
		}

		if (nullptr != rightContent)
		{
			ui.rightSrc->setText(rightContent->join(""));
		}

		QVector<UnequalCharsPosInfo>* leftUnequalPos = nullptr;
		QVector<UnequalCharsPosInfo>* rightUnequalPos = nullptr;

		m_pCmpMode->getUnqealPosList(leftUnequalPos, rightUnequalPos);

		for (auto it = leftUnequalPos->begin(); it != leftUnequalPos->end(); ++it)
		{
			ui.leftSrc->SendScintilla(SCI_STARTSTYLING, it->start);
			ui.leftSrc->SendScintilla(SCI_SETSTYLING, it->length, (long)STYLE_COLOUR_RED);
		}

		for (auto it = rightUnequalPos->begin(); it != rightUnequalPos->end(); ++it)
		{
			ui.rightSrc->SendScintilla(SCI_STARTSTYLING, it->start);
			ui.rightSrc->SendScintilla(SCI_SETSTYLING, it->length, (long)STYLE_COLOUR_RED);
		}


		//createNonEqualBlock(leftExternBlockInfo, rightExternBlockInfo);

		createNonEqualBlock(leftExternBlockInfo, rightExternBlockInfo, m_leftNoEqualBlocks, m_rightNoEqualBlocks);

		megreUnEqualBlock();

		setMarker(RC_LEFT, leftExternBlockInfo);
		setMarker(RC_RIGHT, rightExternBlockInfo);

		setLastBlockFormat(leftExternBlockInfo, rightExternBlockInfo);

		createMargins(leftExternBlockInfo, rightExternBlockInfo);

		//先不delete，只是释放了里面的文件内容
		//delete result->resultCmpObj;

		m_pCmpMode->releaseFile();

		//内存模式要释放内存
		if (m_workMode == 1)
		{
			delete []result->leftText;
			delete []result->rightText;
		}
		delete result;
		result = nullptr;

		setAtEditStatus(false);
		enableTextChangeSignal();

		if (!m_leftNoEqualBlocks.isEmpty())
		{
			gotoViewLine(m_leftNoEqualBlocks.first().startBlockNums);
		}
		ui.statusBar->showMessage(tr("current has %1 differents").arg(m_leftNoEqualBlocks.count()));

		initDiffStatusDockWin();

		updateDiffStatusWin(ui.leftSrc->lines(), m_leftNoEqualBlocks);

		on_updateDiffView(0,0);
	}

	delete s;
	s = nullptr;

	if (m_workMode == 0)
	{
		this->showMaximized();
	}

	//调用一下，让箭头和行宽自动适应一下。否则总是容易拥挤在一起。
	ui.leftSrc->updateLineNumberWidth();
	ui.rightSrc->updateLineNumberWidth();

	m_workStatus = FREE_STATUS;

	m_leftSaveCodeChange = false;
	m_rightSaveCodeChange = false;

	m_isCmpFinished = true;
}

//单纯的显示文本。返回空文件的个数
int CompareWin::loadFileToDisplay()
{
	disableTextChangeSignal();

	//开始比较之前做一下清理初始化的操作
	ui.leftSrc->clear();
	ui.rightSrc->clear();

	m_leftNoEqualBlocks.clear();
	m_rightNoEqualBlocks.clear();

	setAtEditStatus(true);

	m_curShowBlockNums = -1;

	QString leftPath = ui.leftPath->text();
	QString rightPath = ui.rightPath->text();

	int emptyFileNum = 2;

	if (!leftPath.isEmpty())
	{
		--emptyFileNum;

		QList<LineFileInfo> outputLineInfoVec;

		int maxLineSize = 0;
		int charsNums = 0;
		bool isHexFile = false;

		m_leftCode = CmpareMode::scanFileOutPut(m_leftCode,leftPath, outputLineInfoVec, maxLineSize, charsNums, isHexFile);

		//对于错误的编码，选择为CODE_END，会在界面上显示为UNKNOWN,这是一个特殊。
		if (m_leftCode < 0)
		{
			m_leftCode = CODE_END;
		}

		QString text;
		text.reserve(charsNums);

		for (QList<LineFileInfo>::iterator it = outputLineInfoVec.begin(); it != outputLineInfoVec.end(); ++it)
		{
			text.append(it->unicodeStr);
		}

		ui.leftSrc->setText(text);

		ui.leftCodeBox->setCurrentIndex(getIndexFromCode(m_leftCode));
		ui.leftSaveCodeBox->setCurrentIndex(getIndexFromCode(m_leftCode));
	}

	if (!rightPath.isEmpty())
	{
		--emptyFileNum;

		QList<LineFileInfo> outputLineInfoVec;

		int maxLineSize = 0;
		int charsNums = 0;
		bool isHexFile = false;

		m_rightCode = CmpareMode::scanFileOutPut(m_rightCode,rightPath, outputLineInfoVec, maxLineSize, charsNums, isHexFile);

		if (m_rightCode < 0)
		{
			m_rightCode = CODE_END;
		}

		QString text;

		for (QList<LineFileInfo>::iterator it = outputLineInfoVec.begin(); it != outputLineInfoVec.end(); ++it)
		{
			text.append(it->unicodeStr);
		}

		ui.rightSrc->setText(text);

		ui.rightCodeBox->setCurrentIndex(getIndexFromCode(m_rightCode));
		ui.rightSaveCodeBox->setCurrentIndex(getIndexFromCode(m_rightCode));

	}

	setAtEditStatus(false);


	//只有一边有文件时，不能打开对比，否则崩溃
	this->showMaximized();

	return emptyFileNum;
}

bool CompareWin::isTextModeFile(QString filePath)
{
	//如果不支持该文件ext，则以二进制形式打开
	QFileInfo fi(filePath);

	if (DocTypeListView::isSupportExt(fi.suffix()))
	{
		return true;
	}
	else
	{

		QMimeType mime = s_filetype_db.mimeTypeForFile(fi);
		if (mime.name().startsWith("text")) {
			return true;
		}
		return false;
	}
}
//开始做文件比较
void CompareWin::slot_doCmp()
{
	QString leftPath = ui.leftPath->text();
	QString rightPath = ui.rightPath->text();

	if (leftPath.isEmpty() && rightPath.isEmpty())
	{
		return;
	}

	if (m_workStatus != FREE_STATUS)
	{
		//已经在比较中，不能重入
		ui.statusBar->showMessage(tr("Comparison in progress, please wait ..."));
		return;
	}

	//必须两边都有文件，才做比较；否则走单纯的显示
	if (leftPath.isEmpty() || rightPath.isEmpty())
	{
		int emptyFileNums = loadFileToDisplay();

		if (isNeedMemoryCmp())
		{
			slot_doMemoryCmp();
		}
		else
		{
			//没有触发对比的情况下，要开启textChange。否则后续无法触发内存对比
			enableSignTextChange(true);
		}

		blockCompare->setEmptyFileStatus(emptyFileNums);
		return;
	}

    bool fauseCmp = false;

	if(!isTextModeFile(leftPath))
	{
        if(QMessageBox::No == QMessageBox::question(this, "Error", tr("file [%1] maybe not a text file, forse cmpare?(dangerous, may be core)").arg(leftPath)))
        {
			close();
            return;
        }
        fauseCmp = true;
	}
    if(!fauseCmp && !isTextModeFile(rightPath))
	{
        if(QMessageBox::No == QMessageBox::question(this, "Error", tr("file [%1] maybe not a text file, forse cmpare?(dangerous, may be core)").arg(rightPath)))
        {
			close();
            return;
        }
	}


	QFileInfo leftFi(leftPath);
	if (leftFi.isFile() && leftFi.exists())
	{
		++s_useTimes;

		emit receneFilePath(leftPath, rightPath);

		QFileInfo rightFi(rightPath);
		if (rightFi.isFile() && rightFi.exists())
		{

			if (leftFi.size() == 0 || rightFi.size() == 0)
			{
				int emptyFileNums = loadFileToDisplay();
				blockCompare->setEmptyFileStatus(emptyFileNums);
				return;
			}

			//因为编辑框回车也可能触发比较，而回车触发不会设置路径，需要设置一下
			if (ui.leftSrc->getFilePath().isEmpty())
			{
				ui.leftSrc->setFilePath(leftFi.filePath());
			}
			if (ui.rightSrc->getFilePath().isEmpty())
			{
				ui.rightSrc->setFilePath(rightFi.filePath());
			}

			clearStatus();

			if (m_workStatus == FREE_STATUS)
			{
				m_workStatus = CMP_WORKING;
			}

			disableTextChangeSignal();
			setAtEditStatus(true);

			//进入文件对比模式
			if (m_workMode == 1)
			{
				m_workMode = 0;
				enableSignTextChange(false);
			}

			//如果左右文件都存在，而且都是文件，则开始比较

			QFutureWatcher<ThreadFileCmpParameter*> *futureWatcher = new QFutureWatcher<ThreadFileCmpParameter*>();

			m_isCmpFinished = false;

			QObject::connect(futureWatcher, &QFutureWatcher<ThreadFileCmpParameter*>::finished, this, &CompareWin::slot_fileCompareFinish);

			//使用异步的线程是做耗时的比较工作。就是把之前下面的work比较工作，移动到异步线程中去处理

			ThreadFileCmpParameter * threadParater = new ThreadFileCmpParameter(leftPath, rightPath);

			futureWatcher->setFuture(CmpareMode::commitAsyncTask([&](ThreadFileCmpParameter *parameter)->ThreadFileCmpParameter *
			{
				CmpareMode * pCmpMode = new CmpareMode(parameter->leftPath, parameter->rightPath);
				connect(pCmpMode, &CmpareMode::outputMsg, this, &CompareWin::slot_outCmpMsg);
				pCmpMode->setCmpMode(m_cmpMode);
				pCmpMode->setCmpTextCode(m_leftCode,m_rightCode);

				//这里pCmpMode的释放，是在后续clearStatus中释放的
				parameter->resultCmpObj = pCmpMode;
				pCmpMode->work(blockCompare);

				return parameter;
			}, threadParater));

		}
	}

}

void CompareWin::slot_outCmpMsg(int code, QString msg)
{
	ui.statusBar->showMessage(msg);
}

void CompareWin::dragEnterEvent(QDragEnterEvent* event)
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

void CompareWin::dropEvent(QDropEvent* e)
{
	QList<QUrl> urls = e->mimeData()->urls();
	if (urls.isEmpty())
		return;

	QString fileName = urls.first().toLocalFile();

	if (fileName.isEmpty())
	{
		return;
	}

	if (!QFile::exists(fileName))
	{
		return;
	}

	disableTextChangeSignal();

	//在左边
	if (ui.leftSrc->geometry().contains(ui.leftChildWidget->mapFromGlobal(QCursor::pos())))
	{
		ui.leftPath->setText(fileName);

		ui.leftSrc->setFilePath(fileName);

		e->accept();
	}
	else if (ui.rightSrc->geometry().contains(ui.rightChildWidget->mapFromGlobal(QCursor::pos())))
	{
		ui.rightPath->setText(fileName);

		ui.rightSrc->setFilePath(fileName);

		e->accept();
	}
	else
	{
		e->ignore();
	}

	//每次打开文件，或者拖入文件，都把上次残留编码清空一下
	m_leftCode = UNKOWN;
	m_rightCode = UNKOWN;

	slot_doCmp();

	//qDebug() << ui.leftSrc->geometry() << ui.rightSrc->geometry() << QCursor::pos() << this->mapFromGlobal(QCursor::pos());
}


//撤销
void CompareWin::slot_undoBt(bool)
{

#ifdef OPEN_UNDO_REDO

	if (m_undoList.isEmpty())
	{
		ui.statusBar->showMessage(tr("no more undo operator!"));
		return;
	}

	//int srcPostion = ui.leftSrc->execute(SCI_GETCURRENTPOS);
	//int firstDisLineNum = ui.leftSrc->execute(SCI_GETFIRSTVISIBLELINE);

	//int srcPostion1 = ui.rightSrc->execute(SCI_GETCURRENTPOS);
	//int firstDisLineNum1 = ui.rightSrc->execute(SCI_GETFIRSTVISIBLELINE);


	disableTextChangeSignal();

	//把同一序号的全部undo

	int firstIndex = m_undoList.last()->getOperIndex();
	Command* pc = nullptr;

	for (int i = m_undoList.size() - 1; i >= 0; --i)
	{
		if (firstIndex == m_undoList.at(i)->getOperIndex())
		{
			m_undoList.at(i)->undo();

#ifdef _test
			QCoreApplication::processEvents();
#endif
			pc = m_undoList.takeLast();
			if (ChangeDire::LEFT_CHANGE == pc->getChangeStatus())
			{
				m_isLeftDirty = true;
			}
			else if (ChangeDire::RIGHT_CHANGE == pc->getChangeStatus())
			{
				m_isRightDirty = true;
			}
			else  if (ChangeDire::BOTH_CHANGE == pc->getChangeStatus())
			{
				m_isLeftDirty = true;
				m_isRightDirty = true;
			}
			delete pc;
		}
		else
		{
			break;
		}
	}

	//这里必须调用一下，发现也会出现左右不一样长的问题
	setLastBlockFormat(m_leftExternBlockInfo, m_rightExternBlockInfo);
	ui.statusBar->showMessage(tr("current has %1 differents").arg(m_leftNoEqualBlocks.count()));

	enableTextChangeSignal();


	//不能要恢复，发现不恢复位置还本来就是对的。恢复后，反而是错误。
	//恢复一下左右的位置，20230404发现撤销后，位置发生了跳转。
	//ui.leftSrc->execute(SCI_GOTOPOS, srcPostion);
	//ui.leftSrc->execute(SCI_SETFIRSTVISIBLELINE, firstDisLineNum);

	//ui.rightSrc->execute(SCI_GOTOPOS, srcPostion1);
	//ui.rightSrc->execute(SCI_SETFIRSTVISIBLELINE, firstDisLineNum1);

#endif
	syncNeedSaveIcon();
#ifdef _DEBUG
	check();
#endif

	updateDiffStatusWin(ui.leftSrc->lines(), m_leftNoEqualBlocks);
}


//取消之前的同步操作
#ifdef OPEN_UNDO_REDO

void CompareWin::undoReplace(ReplaceOperRecords* record)
{
	int endPos = record->startPos + record->replaceLens;
	int srclength = record->srcEndPos - record->startPos;

	replaceText(record->dir, record->startPos, endPos, srclength, record->srcContents);

#ifdef _test
	QCoreApplication::processEvents();
#endif
}

void CompareWin::undoEdit(ModifyOperRecords* record)
{
	//一个一个去做undo

	QsciDisplayWindow* uiSrc = ((record->dir == RC_LEFT) ? ui.leftSrc : ui.rightSrc);

	if (record->modificationType == SC_MOD_DELETETEXT)
	{
		//删除则增加
		uiSrc->SendScintilla(SCI_INSERTTEXT, record->position, record->contents);
	}
	else if (record->modificationType == SC_MOD_INSERTTEXT)
	{
		//增加则删除
		uiSrc->SendScintilla(SCI_DELETERANGE, record->position, record->length);
	}
#ifdef _test
	QCoreApplication::processEvents();
#endif
}


void CompareWin::undoSync(OperatorRecord* record)
{

	//左边去发起同步的右边
	OperatorInfo& opera = ((record->dir == RC_LEFT) ? record->leftOperator : record->rightOperator);
	OperatorInfo& otherOpera = ((record->dir == RC_LEFT) ? record->rightOperator : record->leftOperator);

	RC_DIRECTION otherDir = ((record->dir == RC_LEFT) ? RC_RIGHT : RC_LEFT);

	QsciDisplayWindow* uiSrc = ((record->dir == RC_LEFT) ? ui.leftSrc : ui.rightSrc);
	QsciDisplayWindow* uiOtherSrc = ((record->dir == RC_LEFT) ? ui.rightSrc : ui.leftSrc);

	QList<BlockUserData*>* externBlockInfo = ((record->dir == RC_LEFT) ? m_leftExternBlockInfo : m_rightExternBlockInfo);
	QList<BlockUserData*>* otherExternBlockInfo = ((record->dir == RC_LEFT) ? m_rightExternBlockInfo : m_leftExternBlockInfo);

	QList<NoEqualBlock>& noEqualBlocks = ((record->dir == RC_LEFT) ? m_leftNoEqualBlocks : m_rightNoEqualBlocks);
	QList<NoEqualBlock>& otherNoEqualBlocks = ((record->dir == RC_LEFT) ? m_rightNoEqualBlocks : m_leftNoEqualBlocks);

	//左边原来是空白，则恢复原来的内容
	if (opera.type == PAD_BLOCK)
	{
		int startLineNum = opera.startLineNums;

		int leftStartPos = uiSrc->SendScintilla(SCI_POSITIONFROMLINE, startLineNum);

		QString leftContent;

		for (int i = 0; i < opera.lineLens; ++i)
		{
			leftContent.append("\r\n");
		}

		uiSrc->SendScintilla(SCI_INSERTTEXT, leftStartPos, leftContent.toStdString().c_str());

		//右边恢复原来的内容

		int rightStartPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, startLineNum);

		uiOtherSrc->SendScintilla(SCI_INSERTTEXT, rightStartPos, otherOpera.lineContents.at(0));

		//把额外行值也恢复一下。删除的则直接添加

		for (int i = 0; i < opera.lineLens; ++i)
		{
			externBlockInfo->insert(startLineNum + i, opera.lineExternInfo.at(i));

			otherExternBlockInfo->insert(startLineNum + i, otherOpera.lineExternInfo.at(i));
		}

		//由于删除了行，从删除行的下面开始，全部更新行的开始号码
		for (int i = opera.startLineNums + opera.lineLens; i < externBlockInfo->size(); ++i)
		{
			externBlockInfo->at(i)->m_srcBlockNum += opera.lineLens;
		}

		for (int i = otherOpera.startLineNums + otherOpera.lineLens; i < otherExternBlockInfo->size(); ++i)
		{
			otherExternBlockInfo->at(i)->m_srcBlockNum += otherOpera.lineLens;
		}
	}
	else
	{
		//原来是内容，则恢复左右的内容。左边一直在，左边不需要恢复

		//右边恢复原来的内容。
		int startLineNum = otherOpera.startLineNums;
		int endLine = startLineNum + otherOpera.lineLens - 1;

		int rightStartPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, startLineNum);
		int endPos = uiOtherSrc->SendScintilla(SCI_POSITIONFROMLINE, endLine + 1);

		if (endPos == -1)
		{
			endPos = uiOtherSrc->length();
		}

#if 0
		//删除现在值，把旧的填回去
		uiOtherSrc->SendScintilla(SCI_DELETERANGE, rightStartPos, endPos - rightStartPos);
		uiOtherSrc->SendScintilla(SCI_INSERTTEXT, rightStartPos, otherOpera.lineContents.at(0));
#endif
		replaceText(otherDir, rightStartPos, endPos, otherOpera.lineLength.at(0), otherOpera.lineContents.at(0));

		//把额外行值也恢复一下。替换的则替换

		for (int i = 0; i < opera.lineLens; ++i)
		{
			BlockUserData* p = externBlockInfo->takeAt(startLineNum + i);
			delete p;

			externBlockInfo->insert(startLineNum + i, opera.lineExternInfo.at(i));

			p = otherExternBlockInfo->takeAt(startLineNum + i);
			delete p;

			otherExternBlockInfo->insert(startLineNum + i, otherOpera.lineExternInfo.at(i));
		}
	}

	//还需要恢复不等块和行值

	//必须要删除，因为内容已经不在，后续会消耗。这里不清空，则后续消耗导致externBlockInfo上面是野指针
	opera.lineExternInfo.clear();
	otherOpera.lineExternInfo.clear();

	int startLineNum = otherOpera.startLineNums;

	//恢复marker
	uiSrc->markerAdd(startLineNum, MARGIN_SYNC_BT);
	uiOtherSrc->markerAdd(startLineNum, MARGIN_SYNC_BT);

	//从第二个开始，不会包含同步箭头图标，只包含递减的块长度。在图标拖动显示的时候，需要画一条包含的括弧线
	for (int j = 1; j < opera.lineLens; ++j)
	{
		uiSrc->markerAdd(startLineNum + j, MARGIN_VER_LINE);
		uiOtherSrc->markerAdd(startLineNum + j, MARGIN_VER_LINE);
	}

	//恢复底纹,不等块
	for (int j = 0; j < opera.lineLens; ++j)
	{
		BlockUserData* p = externBlockInfo->at(startLineNum + j);

		if (p->m_blockType == PAD_BLOCK)
		{
			uiSrc->markerAdd(p->m_srcBlockNum, STYLE_PAD_BACK_PIXMAP);
		}
		else if(p->m_blockType == UNEQUAL_BLOCK)
		{
//#ifdef _test
//			unsigned int mvalue = uiSrc->markersAtLine(p->m_srcBlockNum);
//#endif
			uiSrc->markerDelete(p->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
			uiSrc->markerAdd(p->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
		}
		else if (p->m_blockType == EQUAL_BLOCK)
		{
			uiSrc->markerDelete(p->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
			uiSrc->markerAdd(p->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
		}
#ifdef _test
		QCoreApplication::processEvents();
#endif
		p = otherExternBlockInfo->at(startLineNum + j);

		if (p->m_blockType == PAD_BLOCK)
		{
			uiOtherSrc->markerAdd(p->m_srcBlockNum, STYLE_PAD_BACK_PIXMAP);
		}
		else if (p->m_blockType == UNEQUAL_BLOCK)
		{
#ifdef _test
			unsigned int mvalue = uiSrc->markersAtLine(p->m_srcBlockNum);
#endif
			uiOtherSrc->markerDelete(p->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
			uiOtherSrc->markerAdd(p->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
		}
		else if (p->m_blockType == EQUAL_BLOCK)
		{
			uiOtherSrc->markerDelete(p->m_srcBlockNum, STYLE_BACK_COLOUR_NO_EQUAL);
			uiOtherSrc->markerAdd(p->m_srcBlockNum, STYLE_BACK_COLOUR_EQUAL);
	}
#ifdef _test
		QCoreApplication::processEvents();
#endif
	}

	//恢复不等块


	noEqualBlocks.insert(opera.noEqualindex, opera.noEqualBlockInfo);
	otherNoEqualBlocks.insert(otherOpera.noEqualindex, otherOpera.noEqualBlockInfo);

	int index = opera.noEqualindex;
	int blockLens = opera.lineLens;

	//如果是删除一个块的操作，则现在需要恢复回来，调整对应的偏移
	if (opera.type == PAD_BLOCK)
	{
		for (int i = index + 1; i < noEqualBlocks.size(); ++i)
		{
			noEqualBlocks[i].startBlockNums += blockLens;
		}

		for (int i = index + 1; i < otherNoEqualBlocks.size(); ++i)
		{
			otherNoEqualBlocks[i].startBlockNums += blockLens;
		}
	}

	signLineNoEqualText(opera.startLineNums, opera.lineLens);

	gotoViewLine(opera.startLineNums);

}

//撤销后恢复行中原来不等行的文字标记。20211015发现崩溃。这里SCI_SETSTYLING时长度超过了文本最大长度,qcsint里面引发断言错误。
//现在userdata是在最后才做恢复的，这个函数也是在恢复userdata是才调用。梳理下关于记录恢复信息，已经执行恢复信息时的时序问题。
//该时序问题，现在来看，是要严格保证，不能随意乱序。该函数只能给同步操作使用，即左右2边没有usedata相等的情况。
void CompareWin::signLineNoEqualText(int lineStart, int lineNums)
{
	//修改文字颜色
	QVector<BlockNode> leftBlocks;
	QVector<BlockNode> rightBlocks;

	QVector<UnequalCharsPosInfo> leftUnqealPosList;
	QVector<UnequalCharsPosInfo> rightUnqealPosList;

	//20210724 这里发现可能崩溃，在界面上的最后一行添加字符，最后一行其实不是文本的内容，不过因为换行导致界面显示下面有一个空的行。
	//但是这个空行其实没有externBlockInfo中对应的元素

	cmpLineOneByOne(lineNums, lineStart, lineStart, \
		ui.leftSrc, ui.rightSrc, \
		m_leftExternBlockInfo, m_rightExternBlockInfo, \
		leftBlocks, rightBlocks);

	QStringList leftContent;
	QStringList rightContent;

	QList<BlockUserData*> leftExternBlockInfo;
	QList<BlockUserData*> rightExternBlockInfo;

	QVector<int> leftLineStatus;
	QVector<int> rightLineStatus;

	getCompareUnqealPos(leftBlocks, leftContent, leftExternBlockInfo, leftUnqealPosList, leftLineStatus);
	getCompareUnqealPos(rightBlocks, rightContent, rightExternBlockInfo, rightUnqealPosList, rightLineStatus);


	int leftStartPos = ui.leftSrc->SendScintilla(SCI_POSITIONFROMLINE, lineStart);

	for (auto it = leftUnqealPosList.begin(); it != leftUnqealPosList.end(); ++it)
	{
		ui.leftSrc->SendScintilla(SCI_STARTSTYLING, leftStartPos + it->start);
		ui.leftSrc->SendScintilla(SCI_SETSTYLING, it->length, (long)STYLE_COLOUR_RED);
	}

	int rightStartPos = ui.rightSrc->SendScintilla(SCI_POSITIONFROMLINE, lineStart);

	for (auto it = rightUnqealPosList.begin(); it != rightUnqealPosList.end(); ++it)
	{
		ui.rightSrc->SendScintilla(SCI_STARTSTYLING, rightStartPos + it->start);
		ui.rightSrc->SendScintilla(SCI_SETSTYLING, it->length, (long)STYLE_COLOUR_RED);
	}
}


//撤销其中userdata的变化
void CompareWin::undoUserData(UserDataOperRecords* userData)
{
	//QsciDisplayWindow* uiSrc = ((userData->dir == RC_LEFT) ? ui.leftSrc: ui.rightSrc);
	QList<BlockUserData*>* externBlockInfo = ((userData->dir == RC_LEFT) ? m_leftExternBlockInfo : m_rightExternBlockInfo);

	//这里起始一定是先有增加的，后有删除的。

	if (userData->operType == ADD_USER_DATA)
	{
		//增加则删除。从尾部往前删除
		for (int i = userData->userData.size() - 1; i >= 0; --i)
		{
			BlockUserData* d = userData->userData.at(i);
			externBlockInfo->removeAt(d->m_srcBlockNum);
		}
	}
	else if (userData->operType == DEL_USER_DATA)
	{
		//删除则增加。
		for (int i = 0, size = userData->userData.size(); i < size; ++i)
		{
			BlockUserData* d = userData->userData.at(i);
			externBlockInfo->insert(d->m_srcBlockNum, new BlockUserData(*d));
		}

		//在最后一个做恢复 1）恢复不同块的颜色 2）定位到修改行号
		//20211014发现不对，上面一个insert后，可能出现一个增加，另外一个没有增加，进而导致左右ExternBlockInfo不等的情况。
		//所以不是只做一边即可。而是要如果2个相等，最后才能做一次。否则崩溃
		if(userData->isLastOne && (userData->userData.size() > 0))
		{
			assert(m_leftExternBlockInfo->size() == m_rightExternBlockInfo->size());

			BlockUserData* d = userData->userData.at(0);
			signLineNoEqualText(d->m_srcBlockNum, userData->userData.size());

			gotoViewLine(d->m_srcBlockNum);
		}
	}
}

void CompareWin::gotoViewLine(int lineNum)
{
	ui.leftSrc->SendScintilla(SCI_GOTOLINE, lineNum);
	ui.leftSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, lineNum);

	ui.rightSrc->SendScintilla(SCI_GOTOLINE, lineNum);
	ui.rightSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, lineNum);
}

//自动调整UserData,用在加删行后，调整UserData里面的开始行号
void CompareWin::autoAdjustLineUserData(RC_DIRECTION dir)
{
	QList<BlockUserData*>* externBlockInfo = ((dir == RC_LEFT) ? m_leftExternBlockInfo : m_rightExternBlockInfo);

	BlockUserData* d = nullptr;

	for (int i = externBlockInfo->size() - 1; i >= 0; --i)
	{
		d = externBlockInfo->at(i);
		if (i != d->m_srcBlockNum)
		{
			d->m_srcBlockNum = i;
		}
		else
		{
			break;
		}
	}
	autoAdjustMarker(dir);
}

void CompareWin::autoAdjustMarker(RC_DIRECTION dir)
{
	clearMarker(dir);

	QList<BlockUserData*>* externBlockInfo = ((dir == RC_LEFT) ? m_leftExternBlockInfo : m_rightExternBlockInfo);

	setMarker(dir, externBlockInfo);

}

void CompareWin::autoAdjustMargins()
{
	createNonEqualBlock(m_leftExternBlockInfo, m_rightExternBlockInfo, m_leftNoEqualBlocks, m_rightNoEqualBlocks);
	megreUnEqualBlock();

	cleanMargins(RC_LEFT);
	cleanMargins(RC_RIGHT);
	createMargins(m_leftExternBlockInfo, m_rightExternBlockInfo);
}
#endif



//重做
void CompareWin::slot_redoBt(bool)
{
}

#ifdef _DEBUG

//合法性检测。操作后调用检测一下
void CompareWin::check()
{
	assert(m_leftExternBlockInfo->size() == m_rightExternBlockInfo->size());

	int leftLineNums = ui.leftSrc->SendScintilla(SCI_GETLINECOUNT);
	int rightLineNums = ui.rightSrc->SendScintilla(SCI_GETLINECOUNT);

	assert(leftLineNums == rightLineNums);

	assert(leftLineNums <= m_leftExternBlockInfo->size()+1);
	assert(rightLineNums <= m_rightExternBlockInfo->size() + 1);

	for (int i = 0; i < m_leftExternBlockInfo->size(); ++i)
	{
		assert(i == m_leftExternBlockInfo->at(i)->m_srcBlockNum);
	}

	for (int i = 0; i < m_rightExternBlockInfo->size(); ++i)
	{
		assert(i == m_rightExternBlockInfo->at(i)->m_srcBlockNum);
	}

	for (int i = 0; i < m_leftNoEqualBlocks.size(); ++i)
	{
		int line = m_leftNoEqualBlocks.at(i).startBlockNums;
		assert(m_leftExternBlockInfo->at(line)->m_MarkerFlag == 1);

		int markMask = ui.leftSrc->SendScintilla(SCI_MARKERGET, line);
		assert((markMask & MARGIN_SYNC_BT_BIT_MASK) != 0);
	}

	for (int i = 0; i < m_rightNoEqualBlocks.size(); ++i)
	{
		int line = m_rightNoEqualBlocks.at(i).startBlockNums;
		assert(m_rightExternBlockInfo->at(line)->m_MarkerFlag == 1);

		int markMask = ui.rightSrc->SendScintilla(SCI_MARKERGET, line);
		assert((markMask & MARGIN_SYNC_BT_BIT_MASK) != 0);
	}

}

#endif // DEBUG

void CompareWin::showTips(QString text, int sec)
{
	/*CTipWin* pWin = new CTipWin();
	pWin->setTipText(text);
	pWin->setAttribute(Qt::WA_DeleteOnClose);
	pWin->showMsg(sec);

	QPoint pos = this->pos();
	QSize size = this->size();

	QPoint newPos(pos.x()+10, pos.y() + size.height()-70);
	pWin->move(newPos);*/
	QPoint pos = this->pos();
	QSize size = this->size();

	QPoint newPos(pos.x() + 10, pos.y() + size.height() - 70);

	CTipWin::showTips(this, text, sec, false, newPos, QColor(0xfff29d));
}


//清空目录项目
void CompareWin::slot_clearBt(bool)
{

	if (m_workStatus == FREE_STATUS)
	{
		clearStatus();

		ui.leftPath->clear();
		ui.rightPath->clear();
		ui.leftSrc->setFilePath(QString(""));
		ui.rightSrc->setFilePath(QString(""));

		m_leftCode = CODE_ID::UNKOWN;
		m_rightCode = CODE_ID::UNKOWN;
	}
	else
	{
		ui.statusBar->showMessage(tr("now busy, please try later ..."));
	}

	updateDiffStatusWin(ui.leftSrc->lines(), m_leftNoEqualBlocks);
}

//放大缩小本质还是修改字体。如果没有对应大小的字体，则无法进行放大缩小。目前发现"Courier New"是可以的。
void CompareWin::slot_zoomin()
{
	if (m_zoomValue < 300)
	{
		m_zoomValue += 10;

		ui.leftSrc->zoomIn();
		ui.rightSrc->zoomIn();

		ui.leftSrc->updateLineNumberWidth();
		ui.rightSrc->updateLineNumberWidth();

		CmpSql::updataKeyValueFromNumSets(CMP_ZOOM_VALUE, m_zoomValue);

		showTips(tr("Zoom Value is %1% ").arg(m_zoomValue));
	}
}

void CompareWin::slot_zoomout()
{
	//60对应的是 60 - 100 = -40 ,
	if (m_zoomValue > 70)
	{
		m_zoomValue -= 10;

		ui.leftSrc->zoomOut();
		ui.rightSrc->zoomOut();

		ui.leftSrc->updateLineNumberWidth();
		ui.rightSrc->updateLineNumberWidth();

		CmpSql::updataKeyValueFromNumSets(CMP_ZOOM_VALUE, m_zoomValue);

		showTips(tr("Zoom Value is %1% ").arg(m_zoomValue));
}
}



void CompareWin::slot_leftCodeChange(int index)
{
	CODE_ID newCode = getCodeFromIndex(index);

	//如果是未知，说明编码失败错误。此时不做修改，弹窗告诉用户选择即可。
	if (newCode == CODE_ID::UNKOWN)
	{
		//右边文件编码探测错误，请手动指定编码！
		QMessageBox::warning(this, tr("Left Code Error"), tr("Left file %1 \n encoding detection error, please manually specify!").arg(ui.leftPath->text()));
		return;
	}

	if (newCode != m_leftCode)
	{
		m_leftCode = (CODE_ID)newCode;

		//避免编码同步，引起on_leftSaveCodeChange触发。
		disconnect(ui.leftSaveCodeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CompareWin::on_leftSaveCodeChange);

		ui.leftSaveCodeBox->setCurrentIndex(getIndexFromCode(m_leftCode));

		slot_doCmp();
		m_isLeftDirty = true;
		syncNeedSaveIcon();

		m_leftSaveCodeChange = false;

		connect(ui.leftSaveCodeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CompareWin::on_leftSaveCodeChange);
	}
}

void CompareWin::slot_rightCodeChange(int index)
{
	CODE_ID newCode = getCodeFromIndex(index);

	//如果是未知，说明编码失败错误。此时不做修改，弹窗告诉用户选择即可。
	if (newCode == CODE_ID::UNKOWN)
	{
		//右边文件编码探测错误，请手动指定编码！
		QMessageBox::warning(this, tr("Right Code Error"), tr("Right file %1 \n encoding detection error, please manually specify!").arg(ui.rightPath->text()));
		return;
	}

	if (newCode != m_rightCode)
	{
		m_rightCode = (CODE_ID)newCode;

		disconnect(ui.rightSaveCodeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CompareWin::on_leftSaveCodeChange);

		ui.rightSaveCodeBox->setCurrentIndex(getIndexFromCode(m_rightCode));

		slot_doCmp();
		m_isRightDirty = true;
		syncNeedSaveIcon();

		m_rightSaveCodeChange = false;

		connect(ui.rightSaveCodeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CompareWin::on_leftSaveCodeChange);
	}
}

//保存编码发生变化
void CompareWin::on_leftSaveCodeChange(int index)
{
	//如果保存编码和原始编码一样，不需要做什么。说明是原始编码切换同步导致
	if (ui.leftCodeBox->currentIndex() == index)
	{
		m_leftSaveCodeChange = false;
		return;
	}
	//如果不一样，说明用户手动切换了需要保存的编码。
	m_leftSaveCodeChange = true;

	//保存变红
	ui.leftSaveBt->setIcon(QIcon(NEEDSAVE));
}

void CompareWin::on_rightSaveCodeChange(int index)
{
	if (ui.rightCodeBox->currentIndex() == index)
	{
		return;
	}
	m_rightSaveCodeChange = true;
	ui.rightSaveBt->setIcon(QIcon(NEEDSAVE));
}


void  CompareWin::initDiffStatusDockWin()
{
	//停靠窗口1
	if (m_dockDiffStatusWin.isNull())
	{
		m_dockDiffStatusWin = new QDockWidget(tr("Diff Status"), this);
		m_dockDiffStatusWin->layout()->setMargin(0);
		m_dockDiffStatusWin->layout()->setSpacing(0);
		m_dockDiffStatusWin->setAttribute(Qt::WA_DeleteOnClose);

		//暂时不提供关闭，因为关闭后需要同步菜单的check状态

		m_dockDiffStatusWin->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
		m_dockDiffStatusWin->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

		m_diffStatusWin = new DiffStatusWin(m_dockDiffStatusWin);

		connect(m_diffStatusWin, &DiffStatusWin::gotoLine, this, [this](int line) {
			slot_gotoEditLine(0,line); 
			}, Qt::QueuedConnection);

		//联动其鼠标滚轮事件，差异滚动时，外部对比窗口跟着同步滚动。
		connect(m_diffStatusWin, &DiffStatusWin::syncWheelScroll, this, [this](int delta) {
			emit ui.leftSrc->verticalScrollBar()->valueChanged(ui.leftSrc->verticalScrollBar()->value() - (delta >0 ? ui.leftSrc->verticalScrollBar()->singleStep(): -ui.leftSrc->verticalScrollBar()->singleStep()));
			});

		m_diffStatusWin->setAttribute(Qt::WA_DeleteOnClose);

		m_dockDiffStatusWin->setWidget(m_diffStatusWin);

		addDockWidget(Qt::RightDockWidgetArea, m_dockDiffStatusWin);
	}
}

void CompareWin::updateDiffStatusWin(int maxLength, QList<NoEqualBlock>& diffLines)
{
	if (!m_diffStatusWin.isNull())
	{
		m_diffStatusWin->updateDiffStatus(maxLength, diffLines);

		updateDiffWinCurView();
	}
}

void CompareWin::updateDiffWinCurView()
{
	if (!m_diffStatusWin.isNull())
	{
		int startLine = ui.leftSrc->SendScintilla(SCI_GETFIRSTVISIBLELINE);
		int nbLineOnScreen = ui.leftSrc->execute(SCI_LINESONSCREEN);

		m_diffStatusWin->updateDiffWinCurView(startLine, nbLineOnScreen);
	}
}
