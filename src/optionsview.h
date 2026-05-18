#pragma once

#include <QWidget>
#include "ui_optionsview.h"

class OptionsView : public QWidget
{
	Q_OBJECT

public:
	OptionsView(QWidget *parent = Q_NULLPTR);
	~OptionsView();

	

private slots:
	void slot_curRowChanged(int row);
	void slot_ok();

private:
	Ui::OptionsView ui;
};
