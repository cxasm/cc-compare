#include "comparehexwin.h"
#include "CmpareMode.h"
#include "BlockCompare.h"
#include "MediatorDisplay.h"
#include "Lcs.h"
#include "hexrulewin.h"
#include "cmpsql.h"
#include "hexcmprangewin.h"

#include <QFileDialog>
#include <QFutureWatcher>
#include <Scintilla.h>
#include <QMessageBox>

static const long STYLE_COLOUR_RED = 1;
static const long STYLE_EQUAL_TEXT_COLOUR = 2;
static const long STYLE_HEX_ADDR_BK_COLOUR = 3;

//两种显示line的模式。0 显示行号 1 显示 0x00xx00xx的16进制地址
int const MARGIN_HEX_LINE_NUM = 1;
int const MARGIN_OFFSET_ADDR = 0;

CompareHexWin::CompareHexWin(QWidget *parent)
	: QMainWindow(parent), m_loadFileProcessWin(nullptr), m_workStatus(FREE_STATUS)
{
	ui.setupUi(this);

	init();
}

CompareHexWin::CompareHexWin(QString leftFile, QString rightFile, QWidget * parent) : QMainWindow(parent), m_loadFileProcessWin(nullptr), m_workStatus(FREE_STATUS)
{
	ui.setupUi(this);

	init();

	ui.leftPath->setText(leftFile);
	ui.rightPath->setText(rightFile);

	ui.leftSrc->setFilePath(leftFile);
	ui.rightSrc->setFilePath(rightFile);
}

void CompareHexWin::init()
{
	QToolButton* cmpInfoBt = new QToolButton(NULL);
	cmpInfoBt->setFixedSize(48, 48);
	cmpInfoBt->setIcon(QIcon(":/Resources/img/info.png"));
	cmpInfoBt->setText(tr("info"));
	cmpInfoBt->setToolTip(tr("info"));
	cmpInfoBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(cmpInfoBt);
	connect(cmpInfoBt, &QToolButton::clicked, this, &CompareHexWin::slot_cmpInfoBt);

	QToolButton* ruleBt = new QToolButton(NULL);
	ruleBt->setFixedSize(48, 48);
	ruleBt->setIcon(QIcon(":/Resources/img/rule.png"));
	ruleBt->setText(tr("rule"));
	ruleBt->setToolTip(tr("rule"));
	ruleBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(ruleBt);
	connect(ruleBt, &QToolButton::clicked, this, &CompareHexWin::slot_rultBt);

	QToolButton* clearBt = new QToolButton(NULL);
	clearBt->setFixedSize(48, 48);
	clearBt->setIcon(QIcon(":/Resources/img/clear.png"));
	clearBt->setText(tr("clear"));
	clearBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(clearBt);
	connect(clearBt, &QToolButton::clicked, this, &CompareHexWin::slot_clearBt);


	m_leftRightOrder = 0;

	QToolButton* swapBt = new QToolButton(NULL);
	swapBt->setFixedSize(48, 48);
	swapBt->setIcon(QIcon(":/Resources/img/swap.png"));
	swapBt->setText(tr("swap"));
	swapBt->setToolTip(tr("swap"));
	swapBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(swapBt);
	connect(swapBt, &QToolButton::clicked, this, &CompareHexWin::slot_swapBt);

	QToolButton* reloadBt = new QToolButton(NULL);
	reloadBt->setFixedSize(48, 48);
	reloadBt->setIcon(QIcon(":/Resources/img/reload.png"));
	reloadBt->setText(tr("reload"));
	reloadBt->setToolTip(tr("reload"));
	reloadBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(reloadBt);
	connect(reloadBt, &QToolButton::clicked, this, &CompareHexWin::slot_reloadBt);

	ui.mainToolBar->addSeparator();

	QToolButton* preBt = new QToolButton(NULL);
	preBt->setFixedSize(48, 48);
	preBt->setIcon(QIcon(":/Resources/img/pre2.png"));
	preBt->setText(tr("pre page"));
	preBt->setToolTip(tr("pre page"));
	preBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(preBt);
	connect(preBt, &QToolButton::clicked, this, &CompareHexWin::on_preBt);

	QToolButton* nextBt = new QToolButton(NULL);
	nextBt->setFixedSize(48, 48);
	nextBt->setIcon(QIcon(":/Resources/img/next2.png"));
	nextBt->setText(tr("next page"));
	nextBt->setToolTip(tr("next page"));
	nextBt->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.mainToolBar->addWidget(nextBt);
	connect(nextBt, &QToolButton::clicked, this, &CompareHexWin::on_nextBt);

	ui.mainToolBar->addSeparator();

	//默认有图标，但是黑色模式，需要切换
	//if (StyleSet::getCurrentSytleId() == BLACK_SE)
	//{
	//	ui.leftBt->setIcon(QIcon(":/Resources/img/opendark.png"));
	//	ui.rightBt->setIcon(QIcon(":/Resources/img/opendark.png"));
	//}

	m_blockCompare = new BinLcsResult();

	ui.leftSrc->setDirection(RC_LEFT);
	ui.rightSrc->setDirection(RC_RIGHT);

	m_mediator = new MediatorDisplay();

	ui.leftSrc->setMediator(m_mediator);
	ui.rightSrc->setMediator(m_mediator);

	connect(m_mediator, &MediatorDisplay::syncCurScrollValue, this, &CompareHexWin::slot_syncCurScrollValue);
	connect(m_mediator, &MediatorDisplay::syncCurScrollXValue, this, &CompareHexWin::slot_syncCurScrollXValue);


	//设置不同部分的颜色
	ui.leftSrc->SendScintilla(SCI_STYLESETFORE, STYLE_COLOUR_RED, 0x0000ff);
	ui.rightSrc->SendScintilla(SCI_STYLESETFORE, STYLE_COLOUR_RED, 0x0000ff);

	ui.leftSrc->SendScintilla(SCI_STYLESETFORE, STYLE_EQUAL_TEXT_COLOUR, (long)0x0);
	ui.rightSrc->SendScintilla(SCI_STYLESETFORE, STYLE_EQUAL_TEXT_COLOUR, (long)0x0);

	ui.leftSrc->setMarginsBackgroundColor(QColor(0xf0f0f0));
	ui.rightSrc->setMarginsBackgroundColor(QColor(0xf0f0f0));


	ui.leftSrc->SendScintilla(SCI_STYLESETBACK, STYLE_HEX_ADDR_BK_COLOUR, QColor(0xf0f0f0));
	ui.rightSrc->SendScintilla(SCI_STYLESETBACK, STYLE_HEX_ADDR_BK_COLOUR, QColor(0xf0f0f0));

	//设置字体
#if defined (Q_OS_WIN)
	QFont font("Courier New", 10, QFont::Normal);
    ui.leftSrc->setFont(font);
    ui.rightSrc->setFont(font);
#elif defined(Q_OS_MAC)
	QFont font("Courier New", 10, QFont::Normal);
	ui.leftSrc->setFont(font);
	ui.rightSrc->setFont(font);
#elif defined(Q_OS_UNIX)
    QString fontName("Courier 10 Pitch");
    QFont font(fontName,10, QFont::Normal);
    ui.leftSrc->setFont(font);
    ui.rightSrc->setFont(font);
#endif

	QFontMetrics fontmetrics = QFontMetrics(font);

	ui.leftSrc->setMarginWidth(MARGIN_HEX_LINE_NUM, fontmetrics.width("00000000"));
	ui.leftSrc->setMarginLineNumbers(MARGIN_HEX_LINE_NUM, true);
	ui.leftSrc->setTabWidth(4);

	ui.rightSrc->setMarginWidth(MARGIN_HEX_LINE_NUM, fontmetrics.width("00000000"));
	ui.rightSrc->setMarginLineNumbers(MARGIN_HEX_LINE_NUM, true);
	ui.rightSrc->setTabWidth(4);

	ui.leftSrc->SendScintilla(STYLE_LINENUMBER);
	ui.rightSrc->SendScintilla(STYLE_LINENUMBER);

	CmpSql::init();

	QString key("hexmode");
	m_cmpMode = CmpSql::getKeyValueFromNumSets(key);

	QString key1("hexhigh");
	m_isHighLightBackgroud = CmpSql::getKeyValueFromNumSets(key1);

	if (m_isHighLightBackgroud)
	{
		ui.leftSrc->SendScintilla(SCI_STYLESETBACK, STYLE_COLOUR_RED, QColor(0xf0f0f0));
		ui.rightSrc->SendScintilla(SCI_STYLESETBACK, STYLE_COLOUR_RED, QColor(0xf0f0f0));
	}

	//监控回车
	connect(ui.leftPath, &QLineEdit::returnPressed, this, &CompareHexWin::slot_doCmp);
	connect(ui.rightPath, &QLineEdit::returnPressed, this, &CompareHexWin::slot_doCmp);

	ui.statusBar->showMessage(tr("Drag file support ..."));
}

CompareHexWin::~CompareHexWin()
{
	QString key("hexmode");
	CmpSql::updataKeyValueFromNumSets(key, m_cmpMode);

	QString key1("hexhigh");
	CmpSql::updataKeyValueFromNumSets(key1, m_isHighLightBackgroud);

	if (m_blockCompare != nullptr)
	{
		delete m_blockCompare;
		m_blockCompare = nullptr;
	}

	if (m_mediator != nullptr)
	{
		delete m_mediator;
		m_mediator = nullptr;
	}

	if (m_loadFileProcessWin != nullptr)
	{
		delete m_loadFileProcessWin;
		m_loadFileProcessWin = nullptr;
	}

	CmpSql::close();
}

void CompareHexWin::slot_rultBt(bool)
{
	HexRuleWin* p = new HexRuleWin(m_cmpMode,m_isHighLightBackgroud);
	p->setAttribute(Qt::WA_DeleteOnClose);
	p->setWindowModality(Qt::ApplicationModal);
	connect(p, &HexRuleWin::modeChange, this, &CompareHexWin::slot_cmpModeChange);
	p->show();
}

//切换对比模式
void CompareHexWin::slot_cmpModeChange(int mode, int highlightBack)
{
	bool isChange = false;

	if (m_cmpMode != mode)
	{
		m_cmpMode = mode;
		isChange = true;
	}

	if (m_isHighLightBackgroud != highlightBack)
	{
		m_isHighLightBackgroud = highlightBack;
		isChange = true;

		if (m_isHighLightBackgroud)
		{
			ui.leftSrc->SendScintilla(SCI_STYLESETBACK, STYLE_COLOUR_RED, QColor(0xf0f0f0));
			ui.rightSrc->SendScintilla(SCI_STYLESETBACK, STYLE_COLOUR_RED, QColor(0xf0f0f0));
		}
		else
		{
			ui.leftSrc->SendScintilla(SCI_STYLESETBACK, STYLE_COLOUR_RED, QColor(0xffffff));
			ui.rightSrc->SendScintilla(SCI_STYLESETBACK, STYLE_COLOUR_RED, QColor(0xffffff));
		}
	}

	if (isChange)
	{
		clearStatus();
		slot_doCmp();
	}
}

void CompareHexWin::on_preBt()
{
	doCmp(1);
}

void CompareHexWin::on_nextBt()
{
	doCmp(2);
}

//重加载
void CompareHexWin::slot_reloadBt(bool)
{
	clearStatus();
	slot_doCmp();
}

//交换左右
void CompareHexWin::slot_swapBt(bool)
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

//每次重来对比之前都调用一下
void CompareHexWin::clearStatus()
{
	ui.leftSrc->clear();
	ui.rightSrc->clear();

	ui.leftSrc->setReadOnly(false);
	ui.rightSrc->setReadOnly(false);
}

//打开文件的槽函数
void CompareHexWin::slot_openLeftFile()
{
	openFile(RC_LEFT);
}

void CompareHexWin::slot_openRightFile()
{
	openFile(RC_RIGHT);
}

void CompareHexWin::openFile(int type)
{
	QFileDialog fd(this);
	fd.setFileMode(QFileDialog::ExistingFile);

	QString workFilePath = ((RC_LEFT == type) ? ui.leftPath->text() : ui.rightPath->text());

	if (!workFilePath.isEmpty())
	{
		QFileInfo t(workFilePath);

		fd.setDirectory(t.dir());
	}

	if (fd.exec() == QDialog::Accepted)   //如果成功的执行
	{
		QStringList fileNameList = fd.selectedFiles();      //返回文件列表的名称
		QFileInfo fi(fileNameList[0]);

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

//文件对比完成的槽函数，该槽函数是在主线程中执行的
void CompareHexWin::slot_fileCompareFinish()
{
	QFutureWatcher<ThreadFileCmpParameter*> * s = dynamic_cast<QFutureWatcher<ThreadFileCmpParameter*> *>(sender());
	ThreadFileCmpParameter* result = nullptr;

	try {
		result = s->result();
	}
	catch (...)
	{	
		QString fileName = s->property("file").toString();

		QMessageBox::warning(this, tr("Error"), tr("Binary comparison failed, file '%1' may not be in binary format !").arg(fileName));
		if (m_loadFileProcessWin != nullptr)
		{
			delete m_loadFileProcessWin;
			m_loadFileProcessWin = nullptr;
		}
		if (s != nullptr)
		{
			delete s;
			s = nullptr;
		}
		m_workStatus = FREE_STATUS;
		this->deleteLater();
		return;
	}
		//这里释放的内容，其实是在mode里面new出来的
		if (result != nullptr)
		{
			//获取文件的文字编码
			CmpareMode* pCmpMode = result->resultCmpObj;
			pCmpMode->releaseFile();

			delete pCmpMode;
			delete result;
			result = nullptr;

			if (!m_blockCompare->isCmpCancel)
			{

				ui.leftSrc->setText(m_blockCompare->leftPrintContents);
				ui.rightSrc->setText(m_blockCompare->rightPrintContents);
				setLineDisplayMode(m_blockCompare->cmpMode);

				//5）根据最新对比信息，设置不同文字的颜色为红色
				for (auto it = m_blockCompare->leftUnequalPos.begin(); it != m_blockCompare->leftUnequalPos.end(); ++it)
				{
					ui.leftSrc->SendScintilla(SCI_STARTSTYLING, it->start);
					ui.leftSrc->SendScintilla(SCI_SETSTYLING, (it->end - it->start), (long)STYLE_COLOUR_RED);
				}

				m_loadFileProcessWin->moveStep();

				for (auto it = m_blockCompare->rightUnequalPos.begin(); it != m_blockCompare->rightUnequalPos.end(); ++it)
				{
					ui.rightSrc->SendScintilla(SCI_STARTSTYLING, it->start);
					ui.rightSrc->SendScintilla(SCI_SETSTYLING, (it->end - it->start), (long)STYLE_COLOUR_RED);
				}

				m_loadFileProcessWin->moveStep();

				//5）根据最新对比信息，设置文本文字的颜色为黑色
				for (auto it = m_blockCompare->leftSkipUnequalPos.begin(); it != m_blockCompare->leftSkipUnequalPos.end(); ++it)
				{
					ui.leftSrc->SendScintilla(SCI_STARTSTYLING, it->start);
					ui.leftSrc->SendScintilla(SCI_SETSTYLING, (it->end - it->start), (long)STYLE_EQUAL_TEXT_COLOUR);
				}

				for (auto it = m_blockCompare->rightSkipUnequalPos.begin(); it != m_blockCompare->rightSkipUnequalPos.end(); ++it)
				{
					ui.rightSrc->SendScintilla(SCI_STARTSTYLING, it->start);
					ui.rightSrc->SendScintilla(SCI_SETSTYLING, (it->end - it->start), (long)STYLE_EQUAL_TEXT_COLOUR);
				}

			}
			m_lastResult = *m_blockCompare;
			m_blockCompare->release();

			m_loadFileProcessWin->moveStep();

			ui.leftSrc->setReadOnly(true);
			ui.rightSrc->setReadOnly(true);
		}

		delete s;
		s = nullptr;

		if (m_loadFileProcessWin != nullptr)
		{
			delete m_loadFileProcessWin;
			m_loadFileProcessWin = nullptr;
		}

		this->showMaximized();

		m_workStatus = FREE_STATUS;
}

void CompareHexWin::doCmp(int workMode)
{
	QString leftPath = ui.leftPath->text();
	QString rightPath = ui.rightPath->text();

	if (leftPath.isEmpty() && rightPath.isEmpty())
	{
		return;
	}

	//必须两边都有文件，才做比较；否则走单纯的显示
	if (leftPath.isEmpty() || rightPath.isEmpty())
	{
		//loadFileToDisplay();
		return;
	}

	if (m_workStatus != FREE_STATUS)
	{
		//已经在比较中，不能重入
		ui.statusBar->showMessage(tr("Comparison in progress, please wait ..."));
		return;
	}

	QFileInfo leftFi(leftPath);
	if (leftFi.isFile() && leftFi.exists())
	{
		QFileInfo rightFi(rightPath);
		if (rightFi.isFile() && rightFi.exists())
		{
			//如果文件过大，则提示用户截取一段进行对比
			if ((leftFi.size() > MAX_BIN_SIZE) || (rightFi.size() > MAX_BIN_SIZE))
			{
				if (m_cmpMode == 0)
				{
					slot_outMsgCmpMode(-1, tr("Error : Max Bin File Size is 10M ! Exceeding file size !"));
					return;
				}
				else
				{
					//如果是一对一对比模式，弹出让用户可以选择一个范围
					if (0 == workMode)
					{
					HexCmpRangeWin rangewin(this);
					rangewin.exec();

					bool cancel = false;
					rangewin.getRange(cancel, m_blockCompare->leftStartPos, m_blockCompare->leftCmpLen, m_blockCompare->rightStartPos, m_blockCompare->rightCmpLen);
						m_leftStartPos = m_blockCompare->leftStartPos;
						m_rightStartPos = m_blockCompare->rightStartPos;
						m_leftCmpLen = m_blockCompare->leftCmpLen;
						m_rightCmpLen = m_blockCompare->rightCmpLen;;
				}
					else if (1 == workMode)
					{
						m_blockCompare->leftStartPos = m_leftStartPos - m_leftCmpLen;
						m_blockCompare->rightStartPos = m_rightStartPos - m_rightCmpLen;
						m_blockCompare->leftCmpLen = m_leftCmpLen;
						m_blockCompare->rightCmpLen = m_rightCmpLen;

						m_leftStartPos = m_blockCompare->leftStartPos;
						m_rightStartPos = m_blockCompare->rightStartPos;
			}
					else if (2 == workMode)
					{
						m_blockCompare->leftStartPos = m_leftStartPos + m_leftCmpLen;;
						m_blockCompare->rightStartPos = m_rightStartPos + m_rightCmpLen;
						m_blockCompare->leftCmpLen = m_leftCmpLen;
						m_blockCompare->rightCmpLen = m_rightCmpLen;

						m_leftStartPos = m_blockCompare->leftStartPos;
						m_rightStartPos = m_blockCompare->rightStartPos;
					}

					ui.statusBar->showMessage(tr("left File Start Pos %1, content size %2. right File Start Pos %3, content size %4.").arg(m_blockCompare->leftStartPos).arg(m_blockCompare->leftCmpLen).arg(m_blockCompare->rightStartPos).arg(m_blockCompare->rightCmpLen));
				}
			}

			clearStatus();

			if (m_workStatus == FREE_STATUS)
			{
				m_workStatus = CMP_WORKING;
			}

			if (m_loadFileProcessWin == nullptr)
			{
				m_loadFileProcessWin = new ProgressWin(this);
				connect(m_loadFileProcessWin, &ProgressWin::quitClick, this, &CompareHexWin::slot_cancelCmp);
			}

			m_loadFileProcessWin->setWindowModality(Qt::WindowModal);

			m_loadFileProcessWin->info(tr("cmpare bin file in progress\nplease wait ..."));

			m_loadFileProcessWin->setTotalSteps(Lcs::getCmpTotalStep(leftFi.size(), rightFi.size()));

			m_loadFileProcessWin->show();

			//如果左右文件都存在，而且都是文件，则开始比较
			QFutureWatcher<ThreadFileCmpParameter*> *futureWatcher = new QFutureWatcher<ThreadFileCmpParameter*>();

			QObject::connect(futureWatcher, &QFutureWatcher<ThreadFileCmpParameter*>::finished, this, &CompareHexWin::slot_fileCompareFinish);

			//使用异步的线程是做耗时的比较工作。就是把之前下面的work比较工作，移动到异步线程中去处理

			ThreadFileCmpParameter * threadParater = new ThreadFileCmpParameter(leftPath, rightPath);
			futureWatcher->setProperty("file", rightPath);

			futureWatcher->setFuture(CmpareMode::commitAsyncTask([&](ThreadFileCmpParameter *parameter)->ThreadFileCmpParameter *
			{
				CmpareMode * pCmpMode = new CmpareMode(parameter->leftPath, parameter->rightPath);

				connect(pCmpMode, &CmpareMode::outputMsg, this, &CompareHexWin::slot_outMsgCmpMode);
				parameter->resultCmpObj = pCmpMode;
				m_blockCompare->cmpMode = m_cmpMode;

				pCmpMode->workBin(m_blockCompare);

				return parameter;
			}, threadParater));

		}
	}
}

void CompareHexWin::slot_doCmp()
{
	doCmp(0);
}

//点击进度条的取消按钮
void CompareHexWin::slot_cancelCmp()
{
	m_blockCompare->isCmpCancel = true;
}

//对比过程中的文字汇报
void CompareHexWin::slot_outMsgCmpMode(int code, QString msg)
{
	//error
	if (code == -1)
	{
		QMessageBox::warning(this, "Error", msg);
		return;
	}

	if (m_loadFileProcessWin != nullptr)
	{
		if (code == 0)
		{
			m_loadFileProcessWin->info(msg);
		}
		else if (code == 1)
		{
			//再开启openmp的情况下，这里可能不准确
			m_loadFileProcessWin->moveStep();
		}
		else if (code == 2)
		{
			m_loadFileProcessWin->setStep(m_loadFileProcessWin->getTotalStep()-1);
		}
		else if (code == 3)
		{
			m_loadFileProcessWin->setStep(msg.toInt());
		}
	}
}

//对比过程中移动进度条
void CompareHexWin::slot_cmpMoveStep()
{
	if (m_loadFileProcessWin != nullptr)
	{
		m_loadFileProcessWin->moveStep();
	}
}

//中介者发过来的同步滚动条当前值信息
void CompareHexWin::slot_syncCurScrollValue(int direction)
{
	if (0 == direction)
	{
		ui.rightSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, m_mediator->getLeftScrollValue());
		//一旦对方同步后，则更新对方当前的值
		m_mediator->setRightScrollValue(m_mediator->getLeftScrollValue());
	}
	else
	{
		ui.leftSrc->SendScintilla(SCI_SETFIRSTVISIBLELINE, m_mediator->getRightScrollValue());
		m_mediator->setLeftScrollValue(m_mediator->getRightScrollValue());
	}
}

//中介者发过来的同步滚动条当前值信息
void CompareHexWin::slot_syncCurScrollXValue(int direction)
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

//清空目录项目
void CompareHexWin::slot_clearBt(bool)
{

	if (m_workStatus == FREE_STATUS)
	{
		ui.leftPath->clear();
		ui.rightPath->clear();

		ui.leftSrc->clear();
		ui.rightSrc->clear();

	}
	else
	{
		ui.statusBar->showMessage(tr("now busy, please try later ..."));
	}
}

//给出对比统计信息
void CompareHexWin::slot_cmpInfoBt(bool)
{
	QString leftPath = ui.leftPath->text();
	QString rightPath = ui.rightPath->text();

	if (leftPath.isEmpty() || rightPath.isEmpty())
	{
		return;
	}

	if (!m_lastResult.isCmpCancel)
	{
		if (m_lastResult.leftSize > 0 && m_lastResult.rightSize > 0)
		{
			float equalSize = 0;

			if(m_lastResult.isBlockCmp)
			{ 
#ifdef Q_OS_UNIX
                int i =0;
                foreach(i,m_lastResult.lcsSize);
#else
				for each(int i in m_lastResult.lcsSize)
#endif
				{
					equalSize += i;
				}
			}
			else
			{
				equalSize = m_lastResult.lcsLength;
			}
			if (m_lastResult.leftStartPos == -1)
			{
				QMessageBox::information(this, tr("Compare Result"), tr("Left size %1 byte, right size %2 byte \nEqual content size %3 \nLeft Equal ratio %4 Right Equal ratio %5")\
					.arg(m_lastResult.leftSize).arg(m_lastResult.rightSize).arg(equalSize).arg(equalSize / m_lastResult.leftSize).arg(equalSize / m_lastResult.rightSize));
			}
			else
			{
				QMessageBox::information(this, tr("Compare Result"), tr("Left size %1 byte, right size %2 byte \nEqual content size %3 \nLeft Equal ratio %4 Right Equal ratio %5")\
					.arg(m_lastResult.leftCmpLen).arg(m_lastResult.rightCmpLen).arg(equalSize).arg(equalSize / m_lastResult.leftCmpLen).arg(equalSize / m_lastResult.rightCmpLen));
			}
		}
	}
}

void CompareHexWin::dragEnterEvent(QDragEnterEvent* event)
{
	event->accept(); //可以在这个窗口部件上拖放对象
}

void CompareHexWin::dropEvent(QDropEvent* e)
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

	slot_doCmp();

}



void CompareHexWin::setLineDisplayMode(int mode)
{
	if (0 == mode)
	{
		ui.leftSrc->setMarginWidth(MARGIN_OFFSET_ADDR, 0);
		ui.rightSrc->setMarginWidth(MARGIN_OFFSET_ADDR, 0);

		ui.leftSrc->setMarginLineNumbers(MARGIN_OFFSET_ADDR, false);
		ui.rightSrc->setMarginLineNumbers(MARGIN_OFFSET_ADDR, false);

		ui.leftSrc->setMarginLineNumbers(MARGIN_HEX_LINE_NUM, true);
		ui.rightSrc->setMarginLineNumbers(MARGIN_HEX_LINE_NUM, true);

		QFont font("Courier", 8, QFont::Normal);
		QFontMetrics fontmetrics = QFontMetrics(font);
		ui.leftSrc->setMarginWidth(MARGIN_HEX_LINE_NUM, fontmetrics.width("00000000"));
		ui.rightSrc->setMarginWidth(MARGIN_HEX_LINE_NUM, fontmetrics.width("00000000"));
	}
	else if (1 == mode)
	{
		ui.leftSrc->setMarginWidth(MARGIN_HEX_LINE_NUM, 0);
		ui.rightSrc->setMarginWidth(MARGIN_HEX_LINE_NUM, 0);

		//16进制地址模式下，禁用原来的行号
		ui.leftSrc->setMarginLineNumbers(MARGIN_HEX_LINE_NUM, false);
		ui.rightSrc->setMarginLineNumbers(MARGIN_HEX_LINE_NUM, false);

		QFont font("Courier", 8, QFont::Normal);
		QFontMetrics fontmetrics = QFontMetrics(font);
		ui.leftSrc->setMarginWidth(MARGIN_OFFSET_ADDR, fontmetrics.width("0000000000"));
		ui.leftSrc->SendScintilla(SCI_SETMARGINTYPEN, MARGIN_OFFSET_ADDR, SC_MARGIN_TEXT);

		ui.rightSrc->setMarginWidth(MARGIN_OFFSET_ADDR, fontmetrics.width("0000000000"));
		ui.rightSrc->SendScintilla(SCI_SETMARGINTYPEN, MARGIN_OFFSET_ADDR, SC_MARGIN_TEXT);

		int lineNum = ui.rightSrc->lines();

		char textOut[9];
		textOut[8] = 0;

		for (int i = 0; i < lineNum; ++i)
		{
			sprintf(textOut, "%08lX", 16 * i);
			ui.leftSrc->SendScintilla(SCI_MARGINSETTEXT, i, textOut);
			ui.leftSrc->SendScintilla(SCI_MARGINSETSTYLE, i, STYLE_HEX_ADDR_BK_COLOUR);
			ui.rightSrc->SendScintilla(SCI_MARGINSETTEXT, i, textOut);
			ui.rightSrc->SendScintilla(SCI_MARGINSETSTYLE, i, STYLE_HEX_ADDR_BK_COLOUR);
		}
	}
}
