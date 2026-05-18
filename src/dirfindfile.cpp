#include "dirfindfile.h"

DirFindFile::DirFindFile(int dire, QWidget *parent): QWidget(parent)
{
	ui.setupUi(this);

	if (dire == 0)
	{
		ui.findLeftRadioButton->setChecked(true);
	}
	else if (dire == 1)
	{
		ui.findRightRadioButton->setChecked(true);
	}

	connect(ui.fileNameLineEdit, &QLineEdit::returnPressed, this, &DirFindFile::slot_findNext);
}

DirFindFile::~DirFindFile()
{

}

void DirFindFile::getForce()
{
	ui.fileNameLineEdit->setFocus();
	ui.fileNameLineEdit->selectAll();
}

void DirFindFile::setMode(int mode)
{
	if (1 == mode)
	{
		//在主页查找历史文件路径下， 有些差异。
		ui.findLeftRadioButton->hide();
		ui.findRightRadioButton->hide();
	}
}

void DirFindFile::slot_findPrev()
{
	QString name = ui.fileNameLineEdit->text();
	name = name.trimmed();

	int dire = (ui.findLeftRadioButton->isChecked() ? 0:1);

	bool sens = ui.caseSensitiveCheckBox->isChecked();

	bool wholeWord = ui.wholeWordCheckBox->isChecked();

	emit signFindFile(dire, 0, name, sens, wholeWord);
}

void DirFindFile::slot_findNext()
{
	QString name = ui.fileNameLineEdit->text();
	name = name.trimmed();

	int dire = (ui.findLeftRadioButton->isChecked() ? 0 : 1);

	bool sens = ui.caseSensitiveCheckBox->isChecked();

	bool wholeWord = ui.wholeWordCheckBox->isChecked();

	emit signFindFile(dire, 1, name, sens, wholeWord);
}
