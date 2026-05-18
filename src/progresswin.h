#pragma once

#include <QDialog>
#include <QCloseEvent>
#include "ui_progresswin.h"

class ProgressWin : public QDialog
{
	Q_OBJECT

public:
	ProgressWin(QWidget *parent = Q_NULLPTR);
	virtual ~ProgressWin();


	void info(QString text);
	void setTotalSteps(int step);
	void moveStep(bool isRest = false);

	int getTotalStep();
	void setStep(int step, bool isRest = false);

	bool isCancel();

	void setCancel();

protected:
	void closeEvent(QCloseEvent* e) override;
	void keyPressEvent(QKeyEvent* event);

public slots:
	void slot_quitBt();

signals:
	void quitClick();

private:
	Ui::ProgressWin ui;
	int m_curStep;

	bool m_isCancel;
};
