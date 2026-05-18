#include "ctipwin.h"

#include <QTimer>

CTipWin::CTipWin(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	this->setWindowFlags(Qt::ToolTip);

	/*QPalette  palette(this->palette());
	palette.setColor(QPalette::Window, QColor(0xfff29d));
	this->setPalette(palette);*/
}

CTipWin::~CTipWin()
{
}

void CTipWin::setTipText(QString text)
{
	ui.labelInfo->setText(text);
}

void CTipWin::showMsg(int sec)
{
	show();

	QTimer::singleShot(sec, this, SLOT(slot_delayClose()));
}

void CTipWin::slot_delayClose()
{
	close();
}


void CTipWin::showTips(QWidget* parent, QString text, int sec, bool isMousePos, QPoint pos, QColor backColor)
{

	if (parent != nullptr)
	{
		CTipWin* pWin = new CTipWin();
		pWin->setTipText(text);
		pWin->setAttribute(Qt::WA_DeleteOnClose);
		pWin->showMsg(sec);

		if (backColor.isValid())
		{
			QPalette  palette(pWin->palette());
			palette.setColor(QPalette::Window, backColor);
			pWin->setPalette(palette);
		}

		QFont font;
		font.fromString("Arial,9,-1,5,50,0,0,0,0,0,Regular");

		QFontMetrics FontMetrics(font);
		QRect unreadCountFontMetricsRect = FontMetrics.boundingRect(text);
		int width = unreadCountFontMetricsRect.width();
		int height = unreadCountFontMetricsRect.height();

		pWin->resize(width+20, height+10);

		if (!isMousePos)
		{
			if (pos.isNull())
			{
			QPoint pos = parent->pos();
			QSize size = parent->size();

			QPoint newPos(pos.x() + 10, pos.y() + size.height() - 20);
			pWin->move(newPos);
		}
		else
		{
				pWin->move(pos);
		}
	}
		else
		{
			QPoint p(parent->cursor().pos());
			QPoint newPos(p.x() + 15, p.y());

			pWin->move(newPos);
}

	}
}
