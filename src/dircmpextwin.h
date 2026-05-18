#pragma once

#include <QWidget>
#include "ui_dircmpextwin.h"

class DirCmpExtWin : public QWidget
{
	Q_OBJECT

public:
	DirCmpExtWin(QWidget *parent = Q_NULLPTR);
	~DirCmpExtWin();

private:
	Ui::DirCmpExtWin ui;
};
