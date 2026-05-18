#include "ndsplitter.h"

NdSplitter::NdSplitter(QWidget* parent)
	: QSplitter(parent)
{}

NdSplitter::~NdSplitter()
{}



void NdSplitter::moveToSplitter(int pos, int index)
{
	QSplitter::moveSplitter(pos, index);
}
