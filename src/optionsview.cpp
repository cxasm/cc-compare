#include "optionsview.h"
#include "doctypelistview.h"
#include "dircmpextwin.h"

OptionsView::OptionsView(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);


	DirCmpExtWin* p1 = new DirCmpExtWin(this);
	//p1->show();
	ui.stackedWidget->addWidget(p1);

	ui.optionListWidget->addItem(tr("Compare File Types"));

	DocTypeListView* p2 = new DocTypeListView(this);
	//p->show();
	ui.stackedWidget->addWidget(p2);

	//文件关联 file correlation
	ui.optionListWidget->addItem(tr("File Correlation"));

	connect(ui.optionListWidget, &QListWidget::currentRowChanged, this, &OptionsView::slot_curRowChanged);
}

OptionsView::~OptionsView()
{
}


void OptionsView::slot_curRowChanged(int row)
{
	if (row < ui.stackedWidget->count())
	{
		ui.stackedWidget->setCurrentIndex(row);
	}
}

void OptionsView::slot_ok()
{
	close();
}
