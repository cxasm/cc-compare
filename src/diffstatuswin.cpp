#include "diffstatuswin.h"
#include <QResizeEvent>
#include <QDebug>

const int MAX_WIN_SIZE = 60;

const int tailSpace = 30;

DiffStatusWin::DiffStatusWin(QWidget *parent)
	: QWidget(parent), m_lineNums(1), m_curViewRect(nullptr)
{
	ui.setupUi(this);

	this->setMaximumWidth(MAX_WIN_SIZE);

	m_scene = new QGraphicsScene(this);
	ui.graphicsView->setScene(m_scene);
}

DiffStatusWin::~DiffStatusWin()
{
}

//最大行号。根据该行进行比例的换算
void DiffStatusWin::setMaxLineNums(int lineNum)
{
	m_lineNums = lineNum;
}


//行号定位到像素
int DiffStatusWin::lineToHeigthPix(int lineNum)
{
	return ((this->size().height() - tailSpace) * lineNum / m_lineNums);
}


//与lineToHeigthPix相反
int DiffStatusWin::yPixToLine(int pixY)
{
	return pixY * m_lineNums / (this->size().height() - tailSpace);
}

void DiffStatusWin::updateDiffStatus(int maxLineLength, QList<NoEqualBlock>& diffLines)
{
	if (maxLineLength > 0)
	{
		m_lineNums = maxLineLength;
		m_diffLines = diffLines;
		drawDiffLog(diffLines);
	}
}

void DiffStatusWin::resizeEvent(QResizeEvent * event)
{
	const QSize& s = event->size();
	m_scene->setSceneRect(0, 0, 100, s.height());

	if (!m_diffLines.isEmpty())
	{
		drawDiffLog(m_diffLines);
	}
}

void DiffStatusWin::updateDiffWinCurView(int startLine, int len)
{
	if (startLine < 0)
	{
		startLine = 0;
	}

	int startY = lineToHeigthPix(startLine);
	int height = lineToHeigthPix(len);

	if (m_curViewRect != nullptr)
	{
		delete m_curViewRect;
		m_curViewRect = nullptr;
	}
	m_curViewRect = m_scene->addRect(0, startY, 90, height, QColor(0x0000ff)/*QPen((StyleSet::getCurrentSytleId() != BLACK_SE)? QColor(0x0000ff): QColor(0xffffff))*/);

}

//精细化删除。注意，这只有在同步后，行数不变的情况。
//如果同步前后行号变多或者变少，发送在空对齐去同步不同的时候，此时不能用该接口
//因为长度变化了，得全部重新绘制。这样意义不大，不如每次全部绘制


//绘制一个线条。不是矩形了，之前用的矩形
void DiffStatusWin::drawDiffLine(int startLine)
{
	int startY = lineToHeigthPix(startLine);
	m_scene->addLine(0, startY, 90, startY, QPen(QColor("red")));
}

void DiffStatusWin::updateAll()
{
	drawDiffLog(m_diffLines);
}

//绘制不同点
void DiffStatusWin::drawDiffLog(QList<NoEqualBlock>& diffLines)
{
	//刷新前把之前的全部清空一下
	m_curViewRect = nullptr;
	m_scene->clear();

	for (int i = 0; i < diffLines.size(); ++i)
	{
		drawDiffLine(diffLines.at(i).startBlockNums);
	}
}

//点击后定位到差异位置
void DiffStatusWin::mousePressEvent(QMouseEvent* event)
{
	if (Qt::LeftButton == event->button())
	{
		int pixY = event->pos().y();
		int lineNum = yPixToLine(pixY);

		emit gotoLine(lineNum);
	}

	QWidget::mousePressEvent(event);
}

void DiffStatusWin::wheelEvent(QWheelEvent* event)
{
	//qDebug() << "wheel";

	emit syncWheelScroll(yPixToLine(event->delta()));
}
