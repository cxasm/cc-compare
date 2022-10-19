#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QtWidgetsApplication1.h"
#include <ccmp.h>
#include <blockuserdata.h>

class QtWidgetsApplication1 : public QMainWindow
{
    Q_OBJECT

public:
    QtWidgetsApplication1(QWidget *parent = Q_NULLPTR);

private slots:
	void on_cmpSuccess(int resultType, QStringList* leftContents, QStringList* rightContents, QVector<UnequalCharsPosInfo>* leftUnequalInfo, QVector<UnequalCharsPosInfo>* rightUnequalInfo, \
		const QList<BlockUserData*>* leftBlockData, const QList<BlockUserData*>* rightBlockData);
private:
    Ui::QtWidgetsApplication1Class ui;
};
