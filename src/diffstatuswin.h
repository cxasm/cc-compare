#pragma once

#include <QWidget>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QWheelEvent>
#include "ui_diffstatuswin.h"
#include "rcglobal.h"

class DiffStatusWin : public QWidget
{
	Q_OBJECT

public:
	DiffStatusWin(QWidget *parent = nullptr);
	~DiffStatusWin();

	void setMaxLineNums(int lineNum);
	void updateDiffStatus(int maxLength, QList<NoEqualBlock>& diffLines);
	void updateDiffWinCurView(int startLine, int len);

signals:
	void gotoLine(int lineNum);
	void syncWheelScroll(int delta);
private:
	int lineToHeigthPix(int lineNum);
	int yPixToLine(int pixY);
	void drawDiffLog(QList<NoEqualBlock>& diffLines);
	void drawDiffLine(int startLine);
	void updateAll();

protected:
	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void wheelEvent(QWheelEvent* event) override;

private:
	Ui::DiffStatusWinClass ui;
	QGraphicsScene * m_scene;
	int m_lineNums;

	QList<NoEqualBlock> m_diffLines;

	QGraphicsRectItem* m_curViewRect;
};
