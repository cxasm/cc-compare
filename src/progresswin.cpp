#include "progresswin.h"
#include <QCoreApplication>
#include <QMessageBox>

ProgressWin::ProgressWin(QWidget *parent)
	: QDialog(parent), m_curStep(0),m_isCancel(false)
{
	ui.setupUi(this);
    Qt::WindowFlags flags = Qt::Dialog;
    flags |= Qt::WindowCloseButtonHint;
    setWindowFlags(flags);
}

ProgressWin::~ProgressWin()
{
}

void ProgressWin::keyPressEvent(QKeyEvent* event)
{
	int key = event->key();

	switch (key)
	{
	case Qt::Key_Escape:
	{
		// 忽略ESC键的按下事件，不执行任何操作  
		event->ignore();
		//弹出取消的对话框,如果用户取消，则会结束过程，否则继续。
		close();
		return;
	}
	break;
	default:
		QDialog::keyPressEvent(event);
		break;
	}
}

void ProgressWin::info(QString text)
{
	ui.output->append(text);
}

void ProgressWin::setTotalSteps(int step)
{
	ui.progressBar->setValue(0);
	ui.progressBar->setMaximum(step);
	m_curStep = 0;
}

void ProgressWin::moveStep(bool isRest)
{
	++m_curStep;
	ui.progressBar->setValue(m_curStep);
	ui.progressBar->update();

	if (isRest)
	{
		QCoreApplication::processEvents();
}
}

int ProgressWin::getTotalStep()
{
	return ui.progressBar->maximum();
}

void ProgressWin::setStep(int step, bool isRest)
{
	ui.progressBar->setValue(step);
	ui.progressBar->update();
	m_curStep = step;
	if (isRest)
	{
		QCoreApplication::processEvents();
}
}

bool ProgressWin::isCancel()
{
	return m_isCancel;
}

void ProgressWin::setCancel()
{
	m_isCancel = true;
	emit quitClick();
}

void ProgressWin::closeEvent(QCloseEvent* e)
{
	if (m_curStep < ui.progressBar->maximum())
	{
		slot_quitBt();

		if (!m_isCancel)
		{
	e->ignore();
}
	}
}

void ProgressWin::slot_quitBt()
{
	if (QMessageBox::Yes != QMessageBox::question(this, tr("Notice"), tr("Are you sure to cancel?")))
	{
		return;
	}

	m_isCancel = true;
	emit quitClick();
}
