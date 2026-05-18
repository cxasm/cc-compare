#include "dircmpextwin.h"
#include "CompareDirs.h"
#include "cmpsql.h"
#include <qcheckbox.h>

DirCmpExtWin::DirCmpExtWin(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);

	if (CompareDirs::s_compareHideDir)
	{
		ui.checkBoxCmpHideDir->setChecked(true);
	}
	else
	{
		ui.checkBoxCmpHideDir->setChecked(false);
	}

	if (CompareDirs::s_compareChildDir)
	{
		ui.dealChildDirs->setChecked(true);
	}
	else
	{
		ui.dealChildDirs->setChecked(false);
	}

	if (CompareDirs::s_compareAllFiles == 1)
	{
		ui.radioButtonCmpAllFiles->setChecked(true);
	}
	else if (CompareDirs::s_compareAllFiles == 2)
	{
		ui.radioButtonCmpSupportExtFile->setChecked(true);
	}

	if (CompareDirs::s_compareMode == 0)
	{
		ui.deepMode->setChecked(true);
	}
	else
	{
		ui.fastMode->setChecked(true);
	}

	if (CompareDirs::s_isSkipDir == 0)
	{
		ui.skipDirs->setChecked(false);
	}
	else
	{
		ui.skipDirs->setChecked(true);
	}

	if (!CompareDirs::s_skipDirs.isEmpty())
	{
	ui.skipDirsEdit->setText(CompareDirs::s_skipDirs);
	}

	if (CompareDirs::s_isSkipFile == 0)
	{
		ui.skipFileExt->setChecked(false);
	}
	else
	{
		ui.skipFileExt->setChecked(true);
	}

	if (!CompareDirs::s_skipFileExt.isEmpty())
	{
	ui.skipFileExts->setText(CompareDirs::s_skipFileExt);
	}

	if (CompareDirs::s_isSkipNamePrefix == 0)
	{
		ui.skipFilePrefix->setChecked(false);
	}
	else
	{
		ui.skipFilePrefix->setChecked(true);
	}

	if (!CompareDirs::s_skipNamePrefix.isEmpty())
	{
	ui.skipFileNamePrefix->setText(CompareDirs::s_skipNamePrefix);
}
}

DirCmpExtWin::~DirCmpExtWin()
{
	bool curIsHideDir = (ui.checkBoxCmpHideDir->isChecked() ? true : false);
	bool curIsChildDir = (ui.dealChildDirs->isChecked() ? true : false);
	int curCmpFileType = (ui.radioButtonCmpAllFiles->isChecked() ? 1 : 2);
	int dirCmpMode = (ui.deepMode->isChecked() ? 0 : 1);
	int skipDir = (ui.skipDirs->isChecked()?1:0);
	QString skipDirName = ui.skipDirsEdit->text();

	int skipFile = (ui.skipFileExt->isChecked() ? 1 : 0);
	QString skipFileExt = ui.skipFileExts->text();

	int skipPrefix = (ui.skipFilePrefix->isChecked() ? 1 : 0);
	QString skipFilePrefix = ui.skipFileNamePrefix->text();

	//s_compareHideDir没有持久化
	if (curIsHideDir != CompareDirs::s_compareHideDir)
	{
		CompareDirs::s_compareHideDir = curIsHideDir;
		CmpSql::updataKeyValueFromNumSets("cmphidefile", curIsHideDir ? 1 : 0);
	}
	//s_compareAllFiles没有持久化

	if (curIsChildDir != CompareDirs::s_compareChildDir)
	{
		CompareDirs::s_compareChildDir = curIsChildDir;
	}

	if (CompareDirs::s_compareAllFiles != curCmpFileType)
	{
		CompareDirs::s_compareAllFiles = curCmpFileType;
		CmpSql::updataKeyValueFromNumSets("cmpallfile", curCmpFileType);
	}

	if (dirCmpMode != CompareDirs::s_compareMode)
	{
		CompareDirs::s_compareMode = dirCmpMode;
		CmpSql::updataKeyValueFromNumSets("dirmode", dirCmpMode);
	}

	if (skipDir != CompareDirs::s_isSkipDir)
	{
		CompareDirs::s_isSkipDir = skipDir;
		CmpSql::updataKeyValueFromNumSets("isskipdir", skipDir);
	}

	if (skipDirName != CompareDirs::s_skipDirs)
	{
		CompareDirs::s_skipDirs = skipDirName;
		CmpSql::updataKeyValueFromSets("skipdir", skipDirName);
	}

	if (skipFile != CompareDirs::s_isSkipFile)
	{
		CompareDirs::s_isSkipFile = skipFile;
		CmpSql::updataKeyValueFromNumSets("isskipext", skipFile);
	}

	if (skipFileExt != CompareDirs::s_skipFileExt)
	{
		CompareDirs::s_skipFileExt = skipFileExt;
		CmpSql::updataKeyValueFromSets("skipext", skipFileExt);
	}

	if (skipPrefix != CompareDirs::s_isSkipNamePrefix)
	{
		CompareDirs::s_isSkipNamePrefix = skipPrefix;
		CmpSql::updataKeyValueFromNumSets("isskipprefix", skipPrefix);
	}

	if (skipFilePrefix != CompareDirs::s_skipNamePrefix)
	{
		CompareDirs::s_skipNamePrefix = skipFilePrefix;
		CmpSql::updataKeyValueFromSets("skipprefix", skipFilePrefix);
	}
}
