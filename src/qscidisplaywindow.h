#pragma once

#include <qsciscintilla.h>
#include <Scintilla.h>
#include <functional>
#include <QMenu>
#include <QInputMethodEvent>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>

#include "rcglobal.h"

class MediatorDisplay;
class BlockUserData;
class TextFind;

typedef sptr_t(*SCINTILLA_FUNC) (sptr_t ptr, unsigned int, uptr_t, sptr_t);
typedef sptr_t SCINTILLA_PTR;

class QsciDisplayWindow : public QsciScintilla
{
	Q_OBJECT

public:
	QsciDisplayWindow(QWidget *parent);
	virtual ~QsciDisplayWindow();

	void setIsShowFindItem(bool v);
	sptr_t execute(quint32 Msg, uptr_t wParam = 0, sptr_t lParam = 0) const;
	void setMediator(MediatorDisplay* mediator);

	void setDirection(RC_DIRECTION direction);
	void travEveryBlockToSave(std::function<void(QString&, int)> savefun, QList<BlockUserData*>* externLineInfo);

	//获取当前垂直进度条的滚动位置
	int getCurVerticalScrollValue();

	QString getFilePath()
	{
		return m_filePath;
	}

	void setFilePath(QString v)
	{
		m_filePath = v;
	}
	void autoAdjustLineWidth(int xScrollValue);
	void updateLineNumberWidth();

	void setCopyNeedReplace(bool v)
	{
		m_copyNeedReplace = v;
	}
signals:
	void delayWork();
	void sign_find(bool v=true);
	void sign_saveAsFile();
	void sign_pullOpen();

private slots:
	void slot_delayWork();
	void on_copy();

public slots:
	void slot_showFileInExplorer();
	void slot_scrollYValueChange(int value);
	void slot_scrollXValueChange(int value);
	void slot_findText();
	void slot_FindTextWithPara(int prevOrNext, QString text);

protected:
	void contextUserDefineMenuEvent(QMenu* menu, QContextMenuEvent* e) override;
	void inputMethodEvent(QInputMethodEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void clearIndicator(int indicatorNumber);
	void slot_clearHightWord();
	void highlightViewWithWord(QString & word2Hilite);


	void setStyleOptions();
	void setFoldColor(int margin, QColor fgClack, QColor bkColor);

private:
	RC_DIRECTION m_direction;
	MediatorDisplay* m_mediator;

	QString m_filePath;
	TextFind* m_textFindWin;

	int m_findStartPos;
	int m_findCurPos;
	int m_findEndPos;

	int m_preFirstLineNum;

	SCINTILLA_FUNC m_pScintillaFunc = nullptr;
	SCINTILLA_PTR  m_pScintillaPtr = 0;

	bool m_isShowFindItem;
	bool m_hasHighlight;

	//在对比中，需要对右键拷贝做替换操作。
	bool m_copyNeedReplace;
};
