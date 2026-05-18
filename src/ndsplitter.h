#pragma once

#include <QSplitter>

class NdSplitter  : public QSplitter
{
	Q_OBJECT

public:
	NdSplitter(QWidget* parent);
	~NdSplitter();

	void moveToSplitter(int pos, int index);
};
